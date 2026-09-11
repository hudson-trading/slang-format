//------------------------------------------------------------------------------
// NormalizedFormat.cpp
// Lossless formatter-owned representation of a slang syntax tree.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/NormalizedFormat.h"

#include "format/FormatterUtils.h"
#include <algorithm>
#include <optional>
#include <string_view>

#include "slang/parsing/Lexer.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxFacts.h"
#include "slang/syntax/SyntaxListInfo.h"
#include "slang/syntax/SyntaxNode.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/text/CharInfo.h"
#include "slang/text/SourceManager.h"
#include "slang/util/SmallVector.h"

using namespace slang;
using namespace slang::parsing;
using namespace slang::syntax;

namespace format {
namespace {

bool isConditionalDirective(SyntaxKind kind) {
    return ConditionalBranchDirectiveSyntax::isKind(kind) ||
           UnconditionalBranchDirectiveSyntax::isKind(kind);
}

std::string syntaxText(const SyntaxNode& syntax, const SourceManager* sourceManager) {
    if (ConditionalBranchDirectiveSyntax::isKind(syntax.kind)) {
        const auto& branch = syntax.as<ConditionalBranchDirectiveSyntax>();
        std::string result(branch.directive.rawText());
        for (auto it = branch.expr->tokens_begin(); it != branch.expr->tokens_end(); ++it) {
            auto token = *it;
            if (!token || token.isMissing())
                continue;
            result.push_back(' ');
            for (const auto& trivia : token.trivia()) {
                if (trivia.kind == TriviaKind::LineComment ||
                    trivia.kind == TriviaKind::BlockComment) {
                    result.append(trivia.getRawText());
                    result.push_back(trivia.kind == TriviaKind::LineComment ? '\n' : ' ');
                }
            }
            result.append(token.rawText());
        }
        return result;
    }
    if (UnconditionalBranchDirectiveSyntax::isKind(syntax.kind)) {
        return std::string(syntax.as<UnconditionalBranchDirectiveSyntax>().directive.rawText());
    }

    if (DirectiveSyntax::isKind(syntax.kind)) {
        return SyntaxPrinter()
            .setIncludeDirectives(true)
            .setIncludeSkipped(true)
            .setSquashNewlines(false)
            .printExcludingLeadingComments(syntax)
            .str();
    }

    if (sourceManager) {
        auto range = syntax.sourceRange();
        if (range.start() && range.end() && range.start().buffer() == range.end().buffer()) {
            auto source = sourceManager->getSourceText(range.start().buffer());
            size_t start = range.start().offset();
            size_t end = range.end().offset();
            if (start <= end && end <= source.size())
                return std::string(source.substr(start, end - start));
        }
    }

    std::string result;
    bool first = true;
    for (auto it = syntax.tokens_begin(); it != syntax.tokens_end(); ++it) {
        auto token = *it;
        if (!token || token.isMissing())
            continue;
        if (!first) {
            for (const auto& trivia : token.trivia())
                result.append(trivia.getRawText());
        }
        first = false;
        result.append(token.rawText());
    }
    return result;
}

std::string disabledText(const SyntaxNode& syntax, const SourceManager* sourceManager) {
    const TokenList* disabled = nullptr;
    if (ConditionalBranchDirectiveSyntax::isKind(syntax.kind))
        disabled = &syntax.as<ConditionalBranchDirectiveSyntax>().disabledTokens;
    else if (UnconditionalBranchDirectiveSyntax::isKind(syntax.kind))
        disabled = &syntax.as<UnconditionalBranchDirectiveSyntax>().disabledTokens;
    if (!disabled)
        return {};

    std::string result;
    for (auto token : *disabled) {
        if (!token || token.isMissing())
            continue;
        for (const auto& trivia : token.trivia()) {
            if (trivia.kind == TriviaKind::Directive && trivia.syntax()) {
                result.append(syntaxText(*trivia.syntax(), sourceManager));
                result.append(disabledText(*trivia.syntax(), sourceManager));
            }
            else {
                result.append(trivia.getRawText());
            }
        }
        result.append(token.rawText());
    }
    return result;
}

std::string skippedText(const Trivia& trivia, const SourceManager* sourceManager) {
    std::string result;
    for (auto token : trivia.getSkippedTokens()) {
        for (const auto& nested : token.trivia()) {
            if (nested.kind == TriviaKind::Directive && nested.syntax()) {
                for (const auto& directiveTrivia : nested.syntax()->getFirstToken().trivia())
                    result.append(directiveTrivia.getRawText());
                result.append(syntaxText(*nested.syntax(), sourceManager));
                result.append(disabledText(*nested.syntax(), sourceManager));
            }
            else {
                result.append(nested.getRawText());
            }
        }
        result.append(token.rawText());
    }
    if (result.empty())
        result.append(trivia.getRawText());
    return result;
}

std::optional<std::string> canonicalSkippedAssignment(const Trivia& trivia) {
    auto tokens = trivia.getSkippedTokens();
    if (tokens.empty() || tokens.front().kind != TokenKind::Equals)
        return std::nullopt;

    std::string result;
    bool pendingSpace = false;
    for (auto token : tokens) {
        if (token.kind == TokenKind::Directive)
            return std::nullopt;
        for (const auto& nested : token.trivia()) {
            if (nested.kind != TriviaKind::Whitespace && nested.kind != TriviaKind::EndOfLine)
                return std::nullopt;
            pendingSpace = pendingSpace || !nested.getRawText().empty();
        }
        if (pendingSpace && (result.empty() || !isWhitespace(result.back())))
            result.push_back(' ');
        result.append(token.rawText());
        pendingSpace = false;
    }
    return result;
}

bool hasForeignTemplateDirective(std::string_view text) {
    if (text.find("${") != std::string_view::npos ||
        text.find("`systemc_header") != std::string_view::npos ||
        text.find("`systemc_interface") != std::string_view::npos ||
        text.find('\\') != std::string_view::npos) {
        SourceManager sm;
        BumpAllocator allocator;
        Diagnostics diagnostics;
        Lexer lexer(sm.assignText(text), allocator, diagnostics, sm);
        for (auto token = lexer.lex();; token = lexer.lex()) {
            for (const auto& trivia : token.trivia()) {
                if (trivia.kind != TriviaKind::LineComment)
                    continue;
                auto raw = trivia.getRawText();
                auto content = raw;
                while (!content.empty() &&
                       (isTabOrSpace(content.back()) || isNewline(content.back())))
                    content.remove_suffix(1);
                if (content.ends_with('\\') && content.size() < raw.size())
                    return true;
            }
            if (token.kind == TokenKind::EndOfFile)
                break;
            if (token.kind == TokenKind::StringLiteral)
                continue;
            auto raw = token.rawText();
            auto offset = token.location().offset();
            auto spelling = text.substr(offset, raw.size() + 1);
            if (spelling.find("${") != std::string_view::npos || raw == "`systemc_header" ||
                raw == "`systemc_interface")
                return true;
            if (raw == "\\") {
                auto rest = text.substr(offset + raw.size());
                size_t spaces = 0;
                while (spaces < rest.size() && isTabOrSpace(rest[spaces]))
                    spaces++;
                if (spaces && (spaces == rest.size() || isNewline(rest[spaces])))
                    return true;
            }
        }
    }
    while (!text.empty()) {
        size_t lineEnd = text.find('\n');
        auto line = text.substr(0, lineEnd);
        while (!line.empty() && isTabOrSpace(line.front()))
            line.remove_prefix(1);
        if (line.starts_with('%')) {
            line.remove_prefix(1);
            while (!line.empty() && isTabOrSpace(line.front()))
                line.remove_prefix(1);
            auto startsWithWord = [&](std::string_view word) {
                return line.starts_with(word) &&
                       (line.size() == word.size() || !isValidCIdChar(line[word.size()]));
            };
            if (startsWithWord("if") || startsWithWord("elif") || startsWithWord("else") ||
                startsWithWord("endif") || startsWithWord("for") || startsWithWord("endfor")) {
                return true;
            }
        }
        if (lineEnd == std::string_view::npos)
            break;
        text.remove_prefix(lineEnd + 1);
    }
    return false;
}

int recoveredConditionalDepthChange(std::string_view text) {
    auto startsDirective = [](std::string_view line, std::string_view directive) {
        if (!line.starts_with(directive))
            return false;
        return line.size() == directive.size() || !isValidCIdChar(line[directive.size()]);
    };

    int result = 0;
    while (!text.empty()) {
        size_t lineEnd = findNewline(text);
        auto line = text.substr(0, lineEnd);
        while (!line.empty() && isTabOrSpace(line.front()))
            line.remove_prefix(1);
        if (startsDirective(line, "`ifdef") || startsDirective(line, "`ifndef"))
            result++;
        else if (startsDirective(line, "`endif"))
            result--;

        if (lineEnd == std::string_view::npos)
            break;
        text.remove_prefix(skipNewline(text, lineEnd));
    }
    return result;
}

} // namespace

size_t findNewline(std::string_view text, size_t offset) {
    if (offset >= text.size())
        return std::string_view::npos;
    auto it = std::find_if(text.begin() + offset, text.end(), isNewline);
    return it == text.end() ? std::string_view::npos : size_t(it - text.begin());
}

std::vector<ProtectedTextRange> protectedTextRanges(std::string_view text) {
    SourceManager sourceManager;
    BumpAllocator allocator;
    Diagnostics diagnostics;
    Lexer lexer(sourceManager.assignText(text), allocator, diagnostics, sourceManager);
    std::vector<Token> tokens;
    for (auto token = lexer.lex(); token.kind != TokenKind::EndOfFile; token = lexer.lex())
        tokens.push_back(token);

    std::vector<ProtectedTextRange> result;
    for (size_t i = 0; i < tokens.size(); i++) {
        auto token = tokens[i];
        size_t last = i;
        if (token.kind == TokenKind::Directive &&
            token.directiveKind() == SyntaxKind::DefineDirective) {
            while (last + 1 < tokens.size()) {
                bool continuation = tokens[last].kind == TokenKind::LineContinuation;
                bool done = false;
                for (const auto& trivia : tokens[last + 1].trivia()) {
                    if (trivia.kind == TriviaKind::EndOfLine) {
                        if (!continuation) {
                            done = true;
                            break;
                        }
                        continuation = false;
                    }
                    else if (trivia.kind == TriviaKind::LineComment) {
                        continuation = trivia.getRawText().ends_with('\\');
                    }
                    else {
                        continuation = false;
                    }
                }
                if (done)
                    break;
                last++;
            }
        }
        else if (token.kind == TokenKind::Directive &&
                 token.directiveKind() == SyntaxKind::MacroUsage) {
            if (last + 1 < tokens.size() && tokens[last + 1].kind == TokenKind::OpenParenthesis) {
                int depth = 0;
                do {
                    last++;
                    if (tokens[last].kind == TokenKind::OpenParenthesis)
                        depth++;
                    else if (tokens[last].kind == TokenKind::CloseParenthesis)
                        depth--;
                } while (depth && last + 1 < tokens.size());
            }
        }
        else if (token.kind != TokenKind::StringLiteral) {
            continue;
        }
        result.push_back(
            {token.location().offset(),
             tokens[last].location().offset() + tokens[last].rawText().size()}
        );
        i = last;
    }
    return result;
}

std::vector<MacroArgumentIndent> macroArgumentIndents(std::string_view text) {
    std::vector<MacroArgumentIndent> result;
    if (text.find('\n') == std::string_view::npos)
        return result;

    SourceManager sm;
    BumpAllocator allocator;
    Diagnostics diagnostics;
    Lexer lexer(sm.assignText(text), allocator, diagnostics, sm);
    auto token = lexer.lex();
    if (token.kind != TokenKind::Directive || token.directiveKind() != SyntaxKind::MacroUsage ||
        lexer.lex().kind != TokenKind::OpenParenthesis)
        return result;

    std::vector<TokenKind> delimiters{TokenKind::CloseParenthesis};
    bool argumentStart = true;
    for (token = lexer.lex(); token.kind != TokenKind::EndOfFile; token = lexer.lex()) {
        bool closing = delimiters.size() == 1 && token.kind == TokenKind::CloseParenthesis;
        if (argumentStart || closing) {
            // Expansion replaces the first argument token's trivia with the formal
            // parameter's trivia. Interior whitespace can survive stringification.
            size_t offset = token.location().offset();
            for (const auto& trivia : token.trivia())
                offset -= trivia.getRawText().size();
            for (const auto& trivia : token.trivia()) {
                auto raw = trivia.getRawText();
                if (trivia.kind == TriviaKind::EndOfLine) {
                    size_t begin = offset + raw.size();
                    size_t end = begin;
                    while (end < text.size() && isTabOrSpace(text[end]))
                        end++;
                    if (end < text.size() && !isNewline(text[end]))
                        result.push_back({begin, end, closing && end == token.location().offset()});
                }
                offset += raw.size();
            }
        }
        if (closing)
            return result;
        if (delimiters.size() == 1 && token.kind == TokenKind::Comma) {
            argumentStart = true;
            continue;
        }
        argumentStart = false;
        if (token.kind == delimiters.back())
            delimiters.pop_back();
        else if (auto close = SyntaxFacts::getDelimCloseKind(token.kind);
                 close != TokenKind::Unknown)
            delimiters.push_back(close);
    }
    return {};
}

std::unordered_set<size_t> protectedLineStarts(
    std::string_view text,
    bool protectMacroIndentation
) {
    std::unordered_set<size_t> result;
    if (text.find('\n') == std::string_view::npos)
        return result;
    for (auto range : protectedTextRanges(text)) {
        for (size_t pos = text.find('\n', range.begin); pos < range.end;
             pos = text.find('\n', pos + 1)) {
            result.insert(pos + 1);
        }
        if (!protectMacroIndentation) {
            for (auto indent :
                 macroArgumentIndents(text.substr(range.begin, range.end - range.begin)))
                result.erase(range.begin + indent.begin);
        }
    }
    // Inactive-branch comments are DisabledText, whose interior bytes must
    // survive reindentation just like opaque macro arguments.
    SourceManager sm;
    BumpAllocator allocator;
    Diagnostics diagnostics;
    Lexer lexer(sm.assignText(text), allocator, diagnostics, sm);
    while (true) {
        auto token = lexer.lex();
        size_t offset = token.location().offset();
        for (const auto& trivia : token.trivia())
            offset -= trivia.getRawText().size();
        for (const auto& trivia : token.trivia()) {
            auto raw = trivia.getRawText();
            if (trivia.kind == TriviaKind::BlockComment) {
                for (size_t pos = raw.find('\n'); pos != std::string_view::npos;
                     pos = raw.find('\n', pos + 1))
                    result.insert(offset + pos + 1);
            }
            offset += raw.size();
        }
        if (token.kind == TokenKind::EndOfFile)
            break;
    }
    return result;
}

class NormalizedDocumentBuilder {
public:
    NormalizedDocumentBuilder(const SyntaxNode& root, const SourceManager* sourceManager)
        : root(root),
          sourceManager(sourceManager) {}

    NormalizedFormatDocument build() {
        NormalizedFormatDocument result;
        result.root_ = buildNode(root, 0, false);
        classifyTrivia();
        hoistStandaloneRecovery(*result.root_);
        bool foreignTemplate = false;
        bool incompleteConfig = false;
        bool incompleteDirective = false;
        bool recoveredContainer = false;
        bool recoveredParenthesizedType = false;
        for (const auto& token : tokens) {
            auto parsed = token.token;
            recoveredContainer = recoveredContainer ||
                                 (parsed.isMissing() &&
                                  (parsed.kind == TokenKind::ModuleKeyword ||
                                   parsed.kind == TokenKind::InterfaceKeyword ||
                                   parsed.kind == TokenKind::ProgramKeyword));
            auto inspect = [&](const auto& self, const SyntaxNode& node) -> void {
                for (auto it = node.tokens_begin(); it != node.tokens_end(); ++it) {
                    auto parsed = *it;
                    incompleteDirective = incompleteDirective || parsed.isMissing();
                    for (const auto& trivia : parsed.trivia()) {
                        if (trivia.kind == TriviaKind::Directive && trivia.syntax())
                            self(self, *trivia.syntax());
                    }
                }
            };
            for (const auto& trivia : token.token.trivia()) {
                if (trivia.kind == TriviaKind::Directive && trivia.syntax())
                    inspect(inspect, *trivia.syntax());
                if (token.inDataType && trivia.kind == TriviaKind::SkippedTokens) {
                    auto skipped = trivia.getSkippedTokens();
                    // Unsupported parenthesized types spill into separate declarations,
                    // so the recovered members cannot be formatted independently.
                    if (skipped.size() >= 3 && skipped.front().kind == TokenKind::OpenParenthesis &&
                        skipped.back().kind == TokenKind::CloseParenthesis &&
                        std::ranges::all_of(skipped.subspan(1, skipped.size() - 2), [](Token t) {
                            return t.kind == TokenKind::Identifier ||
                                   t.kind == TokenKind::DoubleColon;
                        }))
                        recoveredParenthesizedType = true;
                }
            }
        }
        // Unsupported config paths can spill into later compilation-unit
        // members during recovery, so preserving just the config is insufficient.
        auto inspectConfig = [&](const NormalizedChild& item) {
            if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&item.value);
                node && (*node)->kind == SyntaxKind::ConfigDeclaration) {
                incompleteConfig =
                    incompleteConfig ||
                    (*node)->syntax->as<ConfigDeclarationSyntax>().endconfig.isMissing();
            }
        };
        for (const auto& child : result.root_->children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (const auto& item : (*list)->children)
                    inspectConfig(item);
            }
            else {
                inspectConfig(child);
            }
        }
        if (sourceManager) {
            auto location = root.getFirstToken().location();
            if (location)
                foreignTemplate =
                    hasForeignTemplateDirective(sourceManager->getSourceText(location.buffer()));
        }
        if (foreignTemplate || incompleteConfig || incompleteDirective || recoveredContainer ||
            recoveredParenthesizedType) {
            result.root_->verbatim = true;
            auto location = root.getFirstToken().location();
            result.root_->verbatimText =
                sourceManager && location
                    ? std::string(sourceManager->getSourceText(location.buffer()))
                    : syntaxText(root, sourceManager);
            if (result.root_->verbatimText.ends_with('\0'))
                result.root_->verbatimText.pop_back();
            result.root_->leading.clear();
            for (auto& token : tokens)
                token.leading.clear();
        }
        else {
            markVerbatimNodes(*result.root_);
            markFormatRegions(*result.root_, result.unmatchedFormatOnCount_);
        }
        classifyTokenLines();
        classifyMacroContinuations();
        buildConditionals();
        result.tokens_ = std::move(tokens);
        result.conditionals_ = std::move(conditionals);
        return result;
    }

private:
    struct TriviaOwner {
        NormalizedToken* token = nullptr;
        bool trailing = false;
        size_t index = 0;

        NormalizedTrivia& get() const {
            return trailing ? token->trailing.at(index) : token->leading.at(index);
        }
    };

    std::optional<size_t> firstRealToken(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& parsed = tokens.at(*token);
            if (parsed.token && !parsed.token.isMissing() && !parsed.fromMacroExpansion &&
                !parsed.token.rawText().empty()) {
                return *token;
            }
            return std::nullopt;
        }
        if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
            for (const auto& nestedChild : (*nested)->children) {
                if (auto result = firstRealToken(nestedChild))
                    return result;
            }
            return std::nullopt;
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& item : list.children) {
            if (auto result = firstRealToken(item))
                return result;
        }
        return std::nullopt;
    }

    void hoistStandaloneRecovery(NormalizedNode& node) {
        if (node.syntax &&
            (MemberSyntax::isKind(node.kind) || StatementSyntax::isKind(node.kind))) {
            std::optional<size_t> first;
            for (const auto& child : node.children) {
                if ((first = firstRealToken(child)))
                    break;
            }
            if (first) {
                auto& leading = tokens.at(*first).leading;
                size_t prefixEnd = 0;
                bool hasRecovery = false;
                while (prefixEnd < leading.size()) {
                    const auto& trivia = leading[prefixEnd];
                    if (trivia.kind == NormalizedTriviaKind::BlankLine) {
                        prefixEnd++;
                        continue;
                    }
                    if (trivia.kind == NormalizedTriviaKind::Verbatim &&
                        trivia.placement == TriviaPlacement::Standalone) {
                        hasRecovery = true;
                        prefixEnd++;
                        continue;
                    }
                    break;
                }
                if (hasRecovery) {
                    for (size_t i = 0; i < prefixEnd; i++)
                        node.leading.push_back(std::move(leading[i]));
                    leading.erase(
                        leading.begin(), leading.begin() + static_cast<ptrdiff_t>(prefixEnd)
                    );
                }
            }
        }

        for (auto& child : node.children) {
            if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
                hoistStandaloneRecovery(**nested);
            }
            else if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (auto& item : (*list)->children) {
                    if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&item.value))
                        hoistStandaloneRecovery(**nested);
                }
            }
        }
    }

    void markFormatRegions(NormalizedNode& node, size_t& unmatchedCount) {
        for (auto& child : node.children) {
            if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
                markFormatRegions(**nested, unmatchedCount);
                continue;
            }
            auto listPtr = std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
            if (!listPtr)
                continue;
            auto& list = **listPtr;
            for (auto& item : list.children) {
                if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&item.value))
                    markFormatRegions(**nested, unmatchedCount);
            }
            if (list.style == ListStyle::Inline)
                continue;

            std::optional<size_t> off;
            std::vector<std::pair<size_t, size_t>> regions;
            for (size_t i = 0; i <= list.children.size(); i++) {
                auto index = i == list.children.size() ? std::optional(list.endTokenIndex)
                                                       : firstRealToken(list.children[i]);
                if (!index || *index >= tokens.size())
                    continue;
                for (auto& trivia : tokens[*index].leading) {
                    if (off && (trivia.kind == NormalizedTriviaKind::ConditionalBranch ||
                                trivia.kind == NormalizedTriviaKind::ConditionalDirective ||
                                trivia.kind == NormalizedTriviaKind::Directive ||
                                trivia.kind == NormalizedTriviaKind::MacroUsage ||
                                trivia.kind == NormalizedTriviaKind::Verbatim)) {
                        trivia.endsLine = trivia.kind != NormalizedTriviaKind::ConditionalBranch;
                        trivia.kind = NormalizedTriviaKind::Unformatted;
                    }
                    if (trivia.kind != NormalizedTriviaKind::Comment)
                        continue;
                    auto text = std::string_view(trivia.text);
                    for (size_t pos = text.find("slang-format: "); pos != std::string_view::npos;
                         pos = text.find("slang-format: ", pos + 1)) {
                        auto directive =
                            text.substr(pos + std::string_view("slang-format: ").size());
                        auto matches = [&](std::string_view word) {
                            return directive.starts_with(word) &&
                                   (directive.size() == word.size() ||
                                    !isValidCIdChar(directive[word.size()]));
                        };
                        if (matches("off")) {
                            if (!off)
                                off = i;
                        }
                        else if (matches("on")) {
                            if (off) {
                                if (*off < i)
                                    regions.emplace_back(*off, i);
                                off.reset();
                            }
                            else {
                                unmatchedCount++;
                            }
                        }
                    }
                }
            }
            if (off && *off < list.children.size())
                regions.emplace_back(*off, list.children.size());

            // Collapse each disabled run into one verbatim node so the whitespace
            // between its list items is preserved as well as the items themselves.
            for (auto [begin, end] : std::views::reverse(regions)) {
                // Keep the final separator in the list so layout can resume normal
                // comma and continuation handling after the preserved run.
                if (auto separator = std::get_if<size_t>(&list.children[end - 1].value);
                    separator && tokens[*separator].token.kind == TokenKind::Comma)
                    end--;
                if (begin == end)
                    continue;
                auto first = firstRealToken(list.children[begin]);
                auto after = end == list.children.size() ? std::optional(list.endTokenIndex)
                                                         : firstRealToken(list.children[end]);
                if (!first || !after || *after <= *first)
                    continue;
                size_t last = *after - 1;
                while (last > *first && (!tokens[last].token || tokens[last].token.isMissing()))
                    last--;
                auto preserved = std::make_unique<NormalizedNode>();
                preserved->verbatim = true;
                preserved->verbatimFirstToken = first;
                preserved->verbatimLastToken = last;
                if (auto firstNode =
                        std::get_if<std::unique_ptr<NormalizedNode>>(&list.children[begin].value)) {
                    preserved->kind = (*firstNode)->kind;
                    preserved->leading = std::move((*firstNode)->leading);
                }
                auto startLoc = tokens[*first].token.location();
                auto endLoc = tokens[last].token.range().end();
                if (sourceManager && startLoc && endLoc && startLoc.buffer() == endLoc.buffer()) {
                    auto source = sourceManager->getSourceText(startLoc.buffer());
                    preserved->verbatimText =
                        source.substr(startLoc.offset(), endLoc.offset() - startLoc.offset());
                }
                else {
                    for (size_t t = *first; t <= last; t++) {
                        if (t != *first) {
                            for (const auto& trivia : tokens[t].token.trivia())
                                preserved->verbatimText += trivia.getRawText();
                        }
                        preserved->verbatimText += tokens[t].token.rawText();
                    }
                }
                for (size_t i = begin; i < end; i++)
                    preserved->children.push_back(std::move(list.children[i]));
                list.children.erase(list.children.begin() + begin, list.children.begin() + end);
                list.children.insert(
                    list.children.begin() + begin, NormalizedChild(std::move(preserved))
                );
            }
        }
    }

    void markVerbatimNodes(NormalizedNode& node) {
        auto firstToken = [&](const auto& self,
                              const NormalizedChild& child) -> std::optional<size_t> {
            if (auto token = std::get_if<size_t>(&child.value)) {
                const auto& parsed = tokens.at(*token);
                if (parsed.token && !parsed.token.isMissing() && !parsed.fromMacroExpansion &&
                    !parsed.token.rawText().empty()) {
                    return *token;
                }
                return std::nullopt;
            }
            if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
                for (const auto& nestedChild : (*nested)->children) {
                    if (auto result = self(self, nestedChild))
                        return result;
                }
                return std::nullopt;
            }
            const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
            for (const auto& item : list.children) {
                if (auto result = self(self, item))
                    return result;
            }
            return std::nullopt;
        };

        for (auto& child : node.children) {
            if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
                markVerbatimNodes(**nested);
            }
            else if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (auto& item : (*list)->children) {
                    if (auto nested = std::get_if<std::unique_ptr<NormalizedNode>>(&item.value)) {
                        markVerbatimNodes(**nested);
                    }
                }
            }
        }

        if (!node.syntax ||
            (!MemberSyntax::isKind(node.kind) && !StatementSyntax::isKind(node.kind))) {
            return;
        }
        std::optional<size_t> first;
        for (const auto& child : node.children) {
            if ((first = firstToken(firstToken, child)))
                break;
        }
        if (!first)
            return;
        bool skipped =
            std::ranges::any_of(tokens.at(*first).leading, [](const NormalizedTrivia& trivia) {
                return trivia.kind == NormalizedTriviaKind::Comment &&
                       trivia.text.find("slang-format: skip") != std::string::npos;
            });
        if (skipped) {
            node.verbatim = true;
            node.verbatimText = syntaxText(*node.syntax, sourceManager);
        }
    }

    std::unique_ptr<NormalizedNode> buildNode(
        const SyntaxNode& syntax,
        size_t depth,
        bool inDataType
    ) {
        auto result = std::make_unique<NormalizedNode>();
        result->kind = syntax.kind;
        result->syntax = &syntax;
        result->depth = depth;

        bool childInDataType = inDataType || DataTypeSyntax::isKind(syntax.kind);
        SmallVector<ListChildInfo, 2> listInfo;
        getChildListInfo(const_cast<SyntaxNode&>(syntax), listInfo);
        size_t nextList = 0;
        bool followsPreservedList = false;

        for (size_t i = 0; i < syntax.getChildCount();) {
            if (nextList < listInfo.size() && i == listInfo[nextList].flatStart) {
                auto list = std::make_unique<NormalizedList>();
                list->parentKind = syntax.kind;
                list->listIndex = nextList;
                list->style = getListStyle(syntax.kind, nextList);
                bool preservesBlankLines = list->style != ListStyle::Inline;
                size_t end = i + listInfo[nextList].size;
                for (; i < end; i++) {
                    auto child = buildChild(syntax, i, depth + 1, childInDataType);
                    if (std::holds_alternative<std::unique_ptr<NormalizedNode>>(child.value)) {
                        markFirstToken(child, preservesBlankLines, false);
                        markFirstToken(child, preservesBlankLines, true);
                    }
                    list->children.push_back(std::move(child));
                }
                list->endTokenIndex = tokens.size();
                result->children.emplace_back(std::move(list));
                followsPreservedList = preservesBlankLines;
                nextList++;
                continue;
            }

            auto child = buildChild(syntax, i, depth + 1, childInDataType);
            if (followsPreservedList)
                markFollowsPreservedList(child);
            followsPreservedList = false;
            result->children.push_back(std::move(child));
            i++;
        }
        return result;
    }

    NormalizedChild buildChild(
        const SyntaxNode& parent,
        size_t index,
        size_t depth,
        bool inDataType
    ) {
        if (auto child = parent.childNode(index))
            return NormalizedChild(buildNode(*child, depth, inDataType));

        auto token = parent.childToken(index);
        NormalizedToken normalized;
        normalized.token = token;
        normalized.parentKind = parent.kind;
        normalized.syntaxDepth = depth;
        normalized.inDataType = inDataType;
        normalized.fromMacroExpansion = sourceManager && token && token.location() &&
                                        sourceManager->isMacroLoc(token.location());
        size_t tokenIndex = tokens.size();
        tokens.push_back(std::move(normalized));
        return NormalizedChild(tokenIndex);
    }

    bool markFirstToken(NormalizedChild& child, bool preservesBlankBefore, bool realOnly) {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& parsed = tokens.at(*token);
            if (realOnly && (!parsed.token || parsed.token.isMissing() ||
                             parsed.fromMacroExpansion || parsed.token.rawText().empty())) {
                return false;
            }
            tokens.at(*token).startsListItem = true;
            tokens.at(*token).preservesBlankBefore = tokens.at(*token).preservesBlankBefore ||
                                                     preservesBlankBefore;
            return true;
        }
        if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
            for (auto& nested : (*node)->children) {
                if (markFirstToken(nested, preservesBlankBefore, realOnly))
                    return true;
            }
            return false;
        }
        auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (auto& nested : list.children) {
            if (markFirstToken(nested, preservesBlankBefore, realOnly))
                return true;
        }
        return false;
    }

    bool markFollowsPreservedList(NormalizedChild& child) {
        if (auto token = std::get_if<size_t>(&child.value)) {
            tokens.at(*token).followsPreservedList = true;
            return true;
        }
        if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
            for (auto& nested : (*node)->children) {
                if (markFollowsPreservedList(nested))
                    return true;
            }
            return false;
        }
        auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (auto& nested : list.children) {
            if (markFollowsPreservedList(nested))
                return true;
        }
        return false;
    }

    void appendBlankLines(
        NormalizedToken& token,
        size_t lineBreaks,
        bool atDocumentStart,
        bool preserve = false
    ) {
        if (atDocumentStart && token.leading.empty())
            return;
        if (!preserve && !token.preservesBlankBefore && !token.followsPreservedList)
            return;
        if (lineBreaks > 1) {
            NormalizedTrivia blank{
                NormalizedTriviaKind::BlankLine, TriviaPlacement::Standalone, "", nullptr, false
            };
            blank.lineBreakCount = lineBreaks;
            token.leading.push_back(std::move(blank));
        }
    }

    bool classifyDirectiveSubTrivia(
        const SyntaxNode& syntax,
        NormalizedToken& current,
        size_t& lineBreaks,
        NormalizedToken* previous,
        const std::optional<TriviaOwner>& previousMacro,
        std::vector<NormalizedTrivia>* previousDirectiveOwner,
        size_t previousDirectiveIndex,
        const SyntaxNode* previousDirectiveSyntax,
        bool disabledTextAlreadyCaptured
    ) {
        auto triviaView = syntax.getFirstToken().trivia();
        bool hadLineBreak = false;
        std::string pendingDisabledText;
        auto flushDisabledText = [&] {
            if (pendingDisabledText.empty())
                return;
            bool hasContent = std::ranges::any_of(pendingDisabledText, [](char c) {
                return !isWhitespace(c);
            });
            if (hasContent) {
                if (!disabledTextAlreadyCaptured) {
                    current.leading.push_back(
                        {NormalizedTriviaKind::ConditionalBranch, TriviaPlacement::Standalone,
                         std::move(pendingDisabledText), nullptr, false}
                    );
                }
                lineBreaks = 0;
            }
            else {
                lineBreaks += std::ranges::count(pendingDisabledText, '\n');
            }
            pendingDisabledText.clear();
        };
        for (size_t triviaIndex = 0; triviaIndex < triviaView.size(); triviaIndex++) {
            const auto& trivia = triviaView[triviaIndex];
            if (trivia.kind == TriviaKind::DisabledText) {
                pendingDisabledText.append(trivia.getRawText());
                hadLineBreak = hadLineBreak ||
                               trivia.getRawText().find('\n') != std::string_view::npos;
                continue;
            }
            flushDisabledText();
            if (trivia.kind == TriviaKind::EndOfLine) {
                lineBreaks++;
                hadLineBreak = true;
                continue;
            }
            if (!isCommentTrivia(trivia))
                continue;

            NormalizedTrivia item{
                NormalizedTriviaKind::Comment, TriviaPlacement::Standalone,
                std::string(trivia.getRawText()), nullptr, isLineCommentTrivia(trivia)
            };
            item.preserveSingleSpace = true;
            bool trailsPreviousDirective = false;
            if (lineBreaks == 0 && sourceManager && previousDirectiveOwner &&
                previousDirectiveSyntax) {
                auto previousRange = previousDirectiveSyntax->sourceRange();
                auto currentRange = syntax.sourceRange();
                if (previousRange.end() && currentRange.start() &&
                    previousRange.end().buffer() == currentRange.start().buffer()) {
                    auto source = sourceManager->getSourceText(previousRange.end().buffer());
                    size_t begin = previousRange.end().offset();
                    size_t end = currentRange.start().offset();
                    if (begin <= end && end <= source.size()) {
                        auto gap = source.substr(begin, end - begin);
                        size_t comment = gap.find(trivia.getRawText());
                        size_t newline = findNewline(gap);
                        trailsPreviousDirective = comment != std::string_view::npos &&
                                                  (newline == std::string_view::npos ||
                                                   comment < newline);
                    }
                }
            }
            if (trailsPreviousDirective) {
                auto& directive = previousDirectiveOwner->at(previousDirectiveIndex);
                directive.text.append("  ");
                directive.text.append(trivia.getRawText());
            }
            else if (lineBreaks == 0 && previousMacro) {
                auto& macro = previousMacro->get();
                macro.joinsFollowingToken = true;
                item.placement = TriviaPlacement::Trailing;
                previousMacro->token->trailing.push_back(std::move(item));
            }
            else if (lineBreaks == 0 && previous) {
                item.placement = TriviaPlacement::Trailing;
                previous->trailing.push_back(std::move(item));
            }
            else {
                appendBlankLines(
                    current, lineBreaks, !previous, syntax.kind == SyntaxKind::MacroUsage
                );
                current.leading.push_back(std::move(item));
            }
            lineBreaks = 0;
        }
        flushDisabledText();
        return hadLineBreak;
    }

    void classifyTrivia() {
        NormalizedToken* previous = nullptr;
        std::optional<TriviaOwner> previousMacro;
        std::vector<TriviaPlacement> conditionalPlacements;
        for (auto& current : tokens) {
            size_t lineBreaks = 0;
            std::vector<NormalizedTrivia>* lastDirectiveOwner = nullptr;
            size_t lastDirectiveIndex = 0;
            std::vector<NormalizedTrivia>* previousDirectiveOwner = nullptr;
            size_t previousDirectiveIndex = 0;
            const SyntaxNode* previousDirectiveSyntax = nullptr;
            auto triviaView = current.token.trivia();
            for (size_t triviaIndex = 0; triviaIndex < triviaView.size(); triviaIndex++) {
                const auto& trivia = triviaView[triviaIndex];
                if (current.fromMacroExpansion && trivia.kind != TriviaKind::Directive)
                    continue;
                if (trivia.kind == TriviaKind::Whitespace)
                    continue;
                if (trivia.kind == TriviaKind::EndOfLine) {
                    lineBreaks++;
                    lastDirectiveOwner = nullptr;
                    previousMacro.reset();
                    continue;
                }

                if (isCommentTrivia(trivia)) {
                    if (lineBreaks == 0 && !current.leading.empty() &&
                        current.leading.back().kind == NormalizedTriviaKind::Comment &&
                        current.leading.back().placement == TriviaPlacement::Standalone) {
                        auto& comment = current.leading.back();
                        comment.text.append(" ");
                        comment.text.append(trivia.getRawText());
                        comment.lineComment = comment.lineComment || isLineCommentTrivia(trivia);
                        continue;
                    }
                    if (lineBreaks == 0 && previousMacro) {
                        auto& macro = previousMacro->get();
                        macro.joinsFollowingToken = true;
                        previousMacro->token->trailing.push_back(
                            {NormalizedTriviaKind::Comment, TriviaPlacement::Trailing,
                             std::string(trivia.getRawText()), nullptr, isLineCommentTrivia(trivia)}
                        );
                        lastDirectiveOwner = nullptr;
                        continue;
                    }
                    if (lineBreaks == 0 && lastDirectiveOwner) {
                        auto& directive = lastDirectiveOwner->at(lastDirectiveIndex);
                        directive.text.append("  ");
                        directive.text.append(trivia.getRawText());
                        continue;
                    }
                    NormalizedTrivia item{
                        NormalizedTriviaKind::Comment, TriviaPlacement::Standalone,
                        std::string(trivia.getRawText()), nullptr, isLineCommentTrivia(trivia)
                    };
                    bool endsLine = item.lineComment;
                    for (size_t i = triviaIndex + 1; i < triviaView.size(); i++) {
                        if (triviaView[i].kind == TriviaKind::EndOfLine) {
                            endsLine = true;
                            break;
                        }
                    }
                    item.endsLine = endsLine;
                    bool prefixBlockComment = !item.lineComment && previous &&
                                              previous->token.kind == TokenKind::OpenParenthesis &&
                                              current.startsListItem;
                    bool inlineBeforeElse = !item.lineComment && previous &&
                                            previous->token.kind == TokenKind::EndKeyword &&
                                            current.token.kind == TokenKind::ElseKeyword;
                    if (lineBreaks == 0 && previous && !prefixBlockComment) {
                        bool blockBeforeBinary = !item.lineComment &&
                                                 SyntaxFacts::getBinaryExpression(
                                                     current.token.kind
                                                 ) != SyntaxKind::Unknown;
                        bool beforeDelimiter = current.token.kind == TokenKind::Comma ||
                                               current.token.kind == TokenKind::Semicolon ||
                                               current.token.kind == TokenKind::CloseParenthesis ||
                                               current.token.kind == TokenKind::CloseBrace ||
                                               previous->token.kind == TokenKind::Semicolon;
                        item.placement = (endsLine || beforeDelimiter) && !inlineBeforeElse &&
                                                 !blockBeforeBinary
                                             ? TriviaPlacement::Trailing
                                             : TriviaPlacement::Inline;
                        previous->trailing.push_back(std::move(item));
                    }
                    else {
                        appendBlankLines(current, lineBreaks, !previous);
                        if (prefixBlockComment)
                            item.placement = TriviaPlacement::Inline;
                        current.leading.push_back(std::move(item));
                    }
                    lineBreaks = 0;
                    continue;
                }

                if (trivia.kind == TriviaKind::Directive && trivia.syntax()) {
                    auto& syntax = *trivia.syntax();
                    bool absorbedConditionalSuffix = false;
                    if ((syntax.kind == SyntaxKind::ElseDirective ||
                         syntax.kind == SyntaxKind::ElsIfDirective ||
                         syntax.kind == SyntaxKind::EndIfDirective) &&
                        !current.leading.empty() &&
                        current.leading.back().kind == NormalizedTriviaKind::ConditionalBranch) {
                        std::string suffix;
                        for (const auto& directiveTrivia : syntax.getFirstToken().trivia()) {
                            if (directiveTrivia.kind == TriviaKind::DisabledText)
                                suffix.append(directiveTrivia.getRawText());
                        }
                        if (std::ranges::any_of(suffix, [](char c) {
                                return !isTabOrSpace(c) && !isNewline(c);
                            })) {
                            absorbedConditionalSuffix = true;
                            current.leading.back().text.append(suffix);
                        }
                    }
                    bool hadLineBreak = classifyDirectiveSubTrivia(
                        syntax, current, lineBreaks, previous, previousMacro,
                        previousDirectiveOwner, previousDirectiveIndex, previousDirectiveSyntax,
                        absorbedConditionalSuffix
                    );
                    if (absorbedConditionalSuffix)
                        lineBreaks = 0;
                    size_t directiveLineBreaks = lineBreaks;
                    appendBlankLines(current, lineBreaks, !previous);

                    NormalizedTriviaKind kind = NormalizedTriviaKind::Directive;
                    if (syntax.kind == SyntaxKind::MacroUsage)
                        kind = NormalizedTriviaKind::MacroUsage;
                    else if (isConditionalDirective(syntax.kind))
                        kind = NormalizedTriviaKind::ConditionalDirective;

                    if (kind == NormalizedTriviaKind::MacroUsage && directiveLineBreaks > 1 &&
                        std::ranges::none_of(current.leading, [](const NormalizedTrivia& item) {
                            return item.kind == NormalizedTriviaKind::BlankLine;
                        })) {
                        current.leading.push_back(
                            {NormalizedTriviaKind::BlankLine, TriviaPlacement::Standalone, "",
                             nullptr, false}
                        );
                    }

                    TriviaPlacement placement = !hadLineBreak && lineBreaks == 0 && previous
                                                    ? TriviaPlacement::Inline
                                                    : TriviaPlacement::Standalone;
                    if (isConditionalDirective(syntax.kind)) {
                        // Keep the whole conditional in the opening directive's
                        // context even when its body introduces line breaks.
                        if (syntax.kind == SyntaxKind::IfDefDirective ||
                            syntax.kind == SyntaxKind::IfNDefDirective) {
                            conditionalPlacements.push_back(placement);
                        }
                        else if (!conditionalPlacements.empty()) {
                            placement = conditionalPlacements.back();
                            if (syntax.kind == SyntaxKind::EndIfDirective)
                                conditionalPlacements.pop_back();
                        }
                    }
                    NormalizedTrivia item{
                        kind, placement, syntaxText(syntax, sourceManager), &syntax, false
                    };
                    if (kind == NormalizedTriviaKind::MacroUsage &&
                        placement == TriviaPlacement::Standalone) {
                        size_t content = 0;
                        while (content < item.text.size() && isTabOrSpace(item.text[content]))
                            content++;
                        item.text.erase(0, content);
                    }
                    if (placement == TriviaPlacement::Inline && previous &&
                        kind == NormalizedTriviaKind::MacroUsage && !previousMacro &&
                        current.token.rawText().empty() && current.leading.empty() &&
                        (previous->parentKind != SyntaxKind::HierarchicalInstance ||
                         previous->token.kind != TokenKind::OpenParenthesis) &&
                        (previous->parentKind != SyntaxKind::FunctionPortList ||
                         previous->token.kind != TokenKind::OpenParenthesis ||
                         !current.inDataType)) {
                        previous->trailing.push_back(std::move(item));
                        lastDirectiveOwner = &previous->trailing;
                        lastDirectiveIndex = previous->trailing.size() - 1;
                        previousMacro = TriviaOwner{previous, true, lastDirectiveIndex};
                    }
                    else {
                        current.leading.push_back(std::move(item));
                        lastDirectiveOwner = &current.leading;
                        lastDirectiveIndex = current.leading.size() - 1;
                        if (kind == NormalizedTriviaKind::MacroUsage)
                            previousMacro = TriviaOwner{&current, false, lastDirectiveIndex};
                    }
                    if (kind != NormalizedTriviaKind::MacroUsage)
                        previousMacro.reset();
                    previousDirectiveOwner = lastDirectiveOwner;
                    previousDirectiveIndex = lastDirectiveIndex;
                    previousDirectiveSyntax = &syntax;
                    if (isConditionalDirective(syntax.kind)) {
                        auto disabled = disabledText(syntax, sourceManager);
                        if (!disabled.empty()) {
                            size_t lineEnd = disabled.find('\n');
                            auto firstLine = std::string_view(disabled).substr(0, lineEnd);
                            while (!firstLine.empty() && isTabOrSpace(firstLine.front()))
                                firstLine.remove_prefix(1);
                            if (!firstLine.empty()) {
                                auto& directive = lastDirectiveOwner->at(lastDirectiveIndex);
                                auto inlineText = firstLine;
                                if (inlineText.starts_with("//") || inlineText.starts_with("/*")) {
                                    directive.text.append("  ");
                                }
                                else if (!inlineText.starts_with(",") &&
                                         !inlineText.starts_with(";") &&
                                         !inlineText.starts_with(")") &&
                                         !inlineText.starts_with("]")) {
                                    directive.text.push_back(' ');
                                }
                                directive.text.append(inlineText);
                                disabled.erase(0, lineEnd);
                            }
                            if (!disabled.empty()) {
                                current.leading.push_back(
                                    {NormalizedTriviaKind::ConditionalBranch,
                                     TriviaPlacement::Standalone, std::move(disabled), nullptr,
                                     false}
                                );
                            }
                        }
                    }
                    lineBreaks = 0;
                    continue;
                }

                auto assignment = trivia.kind == TriviaKind::SkippedTokens
                                      ? canonicalSkippedAssignment(trivia)
                                      : std::nullopt;
                std::string text = assignment ? std::move(*assignment)
                                   : trivia.kind == TriviaKind::SkippedTokens
                                       ? skippedText(trivia, sourceManager)
                                       : std::string(trivia.getRawText());
                if (!text.empty()) {
                    if (trivia.kind == TriviaKind::SkippedTokens) {
                        if (lineBreaks == 0 && previous) {
                            size_t lineEnd = findNewline(text);
                            auto firstLine = std::string_view(text).substr(0, lineEnd);
                            while (!firstLine.empty() && isTabOrSpace(firstLine.front()))
                                firstLine.remove_prefix(1);
                            bool trailingComment = firstLine.starts_with("//") ||
                                                   firstLine.starts_with("/*");
                            if (lineEnd != std::string::npos && trailingComment) {
                                NormalizedTrivia prefix{
                                    NormalizedTriviaKind::Verbatim, TriviaPlacement::Inline,
                                    std::string(text).substr(0, lineEnd), nullptr, false
                                };
                                prefix.endsLine = true;
                                if (previousMacro) {
                                    previousMacro->get().text.append(prefix.text);
                                }
                                else {
                                    previous->trailing.push_back(std::move(prefix));
                                }

                                text.erase(0, skipNewline(text, lineEnd));
                                lineBreaks++;
                            }
                        }

                        size_t content = 0;
                        size_t leadingLineBreaks = 0;
                        size_t afterLastNewline = 0;
                        while (content < text.size() && isWhitespace(text[content])) {
                            if (isNewline(text[content])) {
                                leadingLineBreaks++;
                                content = skipNewline(text, content);
                                afterLastNewline = content;
                            }
                            else
                                content++;
                        }
                        if (leadingLineBreaks) {
                            lineBreaks += leadingLineBreaks;
                            text.erase(0, afterLastNewline);
                        }

                        bool followedByLineBreak = false;
                        size_t following = triviaIndex + 1;
                        std::string_view separatingWhitespace;
                        if (following < triviaView.size() &&
                            triviaView[following].kind == TriviaKind::Whitespace) {
                            separatingWhitespace = triviaView[following].getRawText();
                            following++;
                        }
                        if (following < triviaView.size() &&
                            isCommentTrivia(triviaView[following])) {
                            text.append(separatingWhitespace);
                            text.append(triviaView[following].getRawText());
                            triviaIndex = following;
                            if (isLineCommentTrivia(triviaView[following]) &&
                                following + 1 < triviaView.size() &&
                                triviaView[following + 1].kind == TriviaKind::EndOfLine) {
                                text.append(triviaView[following + 1].getRawText());
                                triviaIndex++;
                                followedByLineBreak = true;
                            }
                        }
                        else {
                            for (; following < triviaView.size(); following++) {
                                if (triviaView[following].kind == TriviaKind::EndOfLine) {
                                    followedByLineBreak = true;
                                    break;
                                }
                                if (triviaView[following].kind != TriviaKind::Whitespace)
                                    break;
                            }
                        }
                        appendBlankLines(current, lineBreaks, !previous, true);
                        bool multiline = text.find('\n') != std::string::npos ||
                                         text.find('\r') != std::string::npos;
                        bool recoveredContinuation =
                            previous && (current.parentKind == SyntaxKind::CoverCross ||
                                         current.parentKind == SyntaxKind::Coverpoint);
                        bool recoveredMemberAfterEnd =
                            previous && previous->token.kind == TokenKind::EndFunctionKeyword &&
                            current.parentKind == SyntaxKind::ClassMethodDeclaration;
                        NormalizedTrivia item{
                            NormalizedTriviaKind::Verbatim,
                            !recoveredMemberAfterEnd &&
                                    (lineBreaks == 0 || recoveredContinuation) && !multiline
                                ? TriviaPlacement::Inline
                                : TriviaPlacement::Standalone,
                            std::move(text), nullptr, false
                        };
                        item.endsLine = followedByLineBreak;
                        item.conditionalDepthChange = recoveredConditionalDepthChange(item.text);
                        size_t boundary = triviaIndex + 1;
                        bool hasHorizontalSeparator = false;
                        while (boundary < triviaView.size() &&
                               triviaView[boundary].kind == TriviaKind::Whitespace) {
                            hasHorizontalSeparator = hasHorizontalSeparator ||
                                                     !triviaView[boundary].getRawText().empty();
                            boundary++;
                        }
                        item.preserveSingleSpace = !item.endsLine && hasHorizontalSeparator &&
                                                   boundary == triviaView.size();
                        if (previous && previous->parentKind == SyntaxKind::NamedBlockClause &&
                            previous->token.kind == TokenKind::Colon && lineBreaks == 0 &&
                            !multiline) {
                            previous->trailing.push_back(std::move(item));
                        }
                        else {
                            current.leading.push_back(std::move(item));
                        }
                        lineBreaks = 0;
                        continue;
                    }
                    appendBlankLines(current, lineBreaks, !previous);
                    current.leading.push_back(
                        {NormalizedTriviaKind::Verbatim,
                         lineBreaks == 0 ? TriviaPlacement::Inline : TriviaPlacement::Standalone,
                         std::move(text), nullptr, false}
                    );
                    lineBreaks = 0;
                }
            }
            current.lineBreakBefore = lineBreaks > 0;
            appendBlankLines(current, lineBreaks, !previous);
            if (current.token && !current.token.isMissing() &&
                current.token.kind != TokenKind::EndOfFile && !current.fromMacroExpansion &&
                !current.token.rawText().empty()) {
                previous = &current;
                previousMacro.reset();
            }
        }
    }

    void classifyMacroContinuations() {
        if (!sourceManager)
            return;
        for (size_t tokenIndex = 0; tokenIndex < tokens.size(); tokenIndex++) {
            auto classify = [&](auto& triviaList, bool leading) {
                for (size_t triviaIndex = 0; triviaIndex < triviaList.size(); triviaIndex++) {
                    auto& trivia = triviaList[triviaIndex];
                    if (trivia.kind != NormalizedTriviaKind::MacroUsage || !trivia.syntax)
                        continue;
                    bool ownsRecoveredContinuation = std::ranges::any_of(
                        triviaList.begin() + static_cast<ptrdiff_t>(triviaIndex + 1),
                        triviaList.end(), [](const NormalizedTrivia& following) {
                            if (following.kind != NormalizedTriviaKind::Verbatim) {
                                return false;
                            }
                            auto text = std::string_view(following.text);
                            while (!text.empty() && isTabOrSpace(text.front()))
                                text.remove_prefix(1);
                            return text.starts_with(',') || text.find('=') != std::string::npos;
                        }
                    );
                    if (ownsRecoveredContinuation)
                        trivia.joinsFollowingToken = true;
                    if (leading && tokens[tokenIndex].token &&
                        tokens[tokenIndex].token.kind == TokenKind::Comma) {
                        trivia.joinsFollowingToken = true;
                    }
                    auto end = trivia.syntax->sourceRange().end();
                    if (end) {
                        auto source = sourceManager->getSourceText(end.buffer());
                        if (end.offset() <= source.size()) {
                            auto restOfLine = source.substr(end.offset());
                            auto following = restOfLine;
                            while (!following.empty() && isWhitespace(following.front()))
                                following.remove_prefix(1);
                            if (following.starts_with(','))
                                trivia.joinsFollowingToken = true;
                            size_t lineEnd = findNewline(restOfLine);
                            if (lineEnd != std::string_view::npos)
                                restOfLine = restOfLine.substr(0, lineEnd);
                            trivia.joinsFollowingToken =
                                trivia.joinsFollowingToken ||
                                std::ranges::any_of(restOfLine, [](char c) {
                                    return !isTabOrSpace(c);
                                });
                        }
                    }
                    if (trivia.joinsFollowingToken)
                        continue;
                    const NormalizedToken* next = nullptr;
                    for (size_t i = tokenIndex; i < tokens.size(); i++) {
                        auto parsed = tokens[i].token;
                        if (parsed && !parsed.isMissing() && !tokens[i].fromMacroExpansion &&
                            !parsed.rawText().empty() && parsed.kind != TokenKind::EndOfFile) {
                            auto location = parsed.location();
                            if (end && location && end.buffer() == location.buffer() &&
                                end.offset() <= location.offset()) {
                                next = &tokens[i];
                                break;
                            }
                        }
                    }
                    if (!next)
                        continue;
                    bool binaryOperator =
                        SyntaxFacts::getBinaryExpression(next->token.kind) != SyntaxKind::Unknown ||
                        SyntaxFacts::getBinarySequenceExpr(next->token.kind) !=
                            SyntaxKind::Unknown ||
                        SyntaxFacts::getBinaryPropertyExpr(next->token.kind) != SyntaxKind::Unknown;
                    bool callBeforeClose = trivia.syntax->kind == SyntaxKind::MacroUsage &&
                                           trivia.syntax->template as<MacroUsageSyntax>().args &&
                                           (next->token.kind == TokenKind::CloseParenthesis ||
                                            next->token.kind == TokenKind::CloseBracket ||
                                            next->token.kind == TokenKind::CloseBrace);
                    if (binaryOperator || next->token.kind == TokenKind::Dot ||
                        next->token.kind == TokenKind::OpenBracket || callBeforeClose) {
                        trivia.joinsFollowingToken = true;
                        continue;
                    }
                    auto start = next->token.location();
                    if (!end || !start || end.buffer() != start.buffer() ||
                        end.offset() > start.offset()) {
                        continue;
                    }
                    auto source = sourceManager->getSourceText(end.buffer());
                    auto between = source.substr(end.offset(), start.offset() - end.offset());
                    trivia.joinsFollowingToken = trivia.joinsFollowingToken ||
                                                 std::ranges::none_of(between, isNewline);
                }
            };
            classify(tokens[tokenIndex].leading, true);
            classify(tokens[tokenIndex].trailing, false);
        }
    }

    void classifyTokenLines() {
        if (!sourceManager)
            return;
        const NormalizedToken* previous = nullptr;
        for (auto& current : tokens) {
            auto parsed = current.token;
            if (!parsed || parsed.isMissing() || current.fromMacroExpansion ||
                parsed.rawText().empty() || parsed.kind == TokenKind::EndOfFile) {
                continue;
            }
            if (previous) {
                auto previousStart = previous->token.location();
                auto currentStart = parsed.location();
                if (previousStart && currentStart &&
                    previousStart.buffer() == currentStart.buffer()) {
                    size_t previousEnd = previousStart.offset() + previous->token.rawText().size();
                    if (previousEnd <= currentStart.offset()) {
                        auto source = sourceManager->getSourceText(currentStart.buffer());
                        auto between =
                            source.substr(previousEnd, currentStart.offset() - previousEnd);
                        current.lineBreakBefore = std::ranges::any_of(between, isNewline);
                    }
                }
            }
            previous = &current;
        }
    }

    void buildConditionals() {
        std::vector<NormalizedConditional*> stack;
        for (size_t tokenIndex = 0; tokenIndex < tokens.size(); tokenIndex++) {
            for (const auto& trivia : tokens[tokenIndex].leading) {
                if (trivia.kind != NormalizedTriviaKind::ConditionalDirective || !trivia.syntax)
                    continue;

                auto kind = trivia.syntax->kind;
                if (kind == SyntaxKind::IfDefDirective || kind == SyntaxKind::IfNDefDirective) {
                    auto conditional = std::make_unique<NormalizedConditional>();
                    conditional->branches.push_back({trivia.syntax, {}});
                    auto* ptr = conditional.get();
                    if (stack.empty())
                        conditionals.push_back(std::move(conditional));
                    else
                        stack.back()->branches.back().contents.emplace_back(std::move(conditional));
                    stack.push_back(ptr);
                }
                else if ((kind == SyntaxKind::ElsIfDirective ||
                          kind == SyntaxKind::ElseDirective) &&
                         !stack.empty()) {
                    stack.back()->branches.push_back({trivia.syntax, {}});
                }
                else if (kind == SyntaxKind::EndIfDirective && !stack.empty()) {
                    stack.back()->terminated = true;
                    stack.pop_back();
                }
            }
            if (!stack.empty())
                stack.back()->branches.back().contents.emplace_back(tokenIndex);
        }
    }

    const SyntaxNode& root;
    const SourceManager* sourceManager;
    std::vector<NormalizedToken> tokens;
    std::vector<std::unique_ptr<NormalizedConditional>> conditionals;
};

void checkSyntaxDepth(const SyntaxNode& root, size_t limit) {
    // Reject oversized trees before allocating a recursively owned tree: throwing
    // from buildNode would also have to unwind and destroy that tree on the stack.
    std::vector<std::pair<const SyntaxNode*, size_t>> pending{{&root, 0}};
    while (!pending.empty()) {
        auto [node, depth] = pending.back();
        pending.pop_back();
        if (depth >= limit)
            throw FormatDepthLimitError(limit);
        for (size_t i = 0; i < node->getChildCount(); i++) {
            if (auto child = node->childNode(i))
                pending.emplace_back(child, depth + 1);
        }
    }
}

NormalizedFormatDocument NormalizedFormatDocument::build(
    const SyntaxNode& root,
    const SourceManager* sourceManager,
    size_t maxSyntaxDepth
) {
    checkSyntaxDepth(root, maxSyntaxDepth);
    return NormalizedDocumentBuilder(root, sourceManager).build();
}

} // namespace format

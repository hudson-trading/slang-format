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

#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxListInfo.h"
#include "slang/syntax/SyntaxNode.h"
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
            result.append(token.rawText());
        }
        return result;
    }
    if (UnconditionalBranchDirectiveSyntax::isKind(syntax.kind)) {
        return std::string(syntax.as<UnconditionalBranchDirectiveSyntax>().directive.rawText());
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

} // namespace

class NormalizedDocumentBuilder {
public:
    NormalizedDocumentBuilder(const SyntaxNode& root, const SourceManager* sourceManager) :
        root(root), sourceManager(sourceManager) {}

    NormalizedFormatDocument build() {
        NormalizedFormatDocument result;
        result.root_ = buildNode(root, 0, false);
        classifyTrivia();
        markVerbatimNodes(*result.root_);
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

    std::unique_ptr<NormalizedNode> buildNode(const SyntaxNode& syntax, size_t depth,
                                              bool inDataType) {
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

    NormalizedChild buildChild(const SyntaxNode& parent, size_t index, size_t depth,
                               bool inDataType) {
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

    void appendBlankLines(NormalizedToken& token, size_t lineBreaks) {
        if (!token.preservesBlankBefore && !token.followsPreservedList)
            return;
        for (size_t i = 1; i < lineBreaks; i++) {
            token.leading.push_back(
                {NormalizedTriviaKind::BlankLine, TriviaPlacement::Standalone, "", nullptr, false});
        }
    }

    bool classifyDirectiveSubTrivia(const SyntaxNode& syntax, NormalizedToken& current,
                                    size_t& lineBreaks, NormalizedToken* previous,
                                    const std::optional<TriviaOwner>& previousMacro,
                                    std::vector<NormalizedTrivia>* previousDirectiveOwner,
                                    size_t previousDirectiveIndex,
                                    const SyntaxNode* previousDirectiveSyntax) {
        auto triviaView = syntax.getFirstToken().trivia();
        bool hadLineBreak = false;
        for (size_t triviaIndex = 0; triviaIndex < triviaView.size(); triviaIndex++) {
            const auto& trivia = triviaView[triviaIndex];
            if (syntax.kind == SyntaxKind::EndIfDirective &&
                trivia.kind == TriviaKind::DisabledText) {
                if (trivia.getRawText().find('\n') != std::string_view::npos) {
                    lineBreaks = std::max<size_t>(lineBreaks, 1);
                    hadLineBreak = true;
                }
                continue;
            }
            if (trivia.kind == TriviaKind::EndOfLine ||
                (trivia.kind == TriviaKind::DisabledText && trivia.getRawText() == "\n")) {
                lineBreaks++;
                hadLineBreak = true;
                continue;
            }
            if (trivia.kind == TriviaKind::DisabledText)
                continue;
            if (!isCommentTrivia(trivia))
                continue;

            NormalizedTrivia item{NormalizedTriviaKind::Comment, TriviaPlacement::Standalone,
                                  std::string(trivia.getRawText()), nullptr,
                                  isLineCommentTrivia(trivia)};
            item.preserveSingleSpace = true;
            bool trailsPreviousDirective = false;
            if (sourceManager && previousDirectiveOwner && previousDirectiveSyntax) {
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
                        size_t newline = gap.find_first_of("\r\n");
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
                appendBlankLines(current, lineBreaks);
                current.leading.push_back(std::move(item));
            }
            lineBreaks = 0;
        }
        return hadLineBreak;
    }

    void classifyTrivia() {
        NormalizedToken* previous = nullptr;
        std::optional<TriviaOwner> previousMacro;
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
                    if (lineBreaks == 0 && previousMacro) {
                        auto& macro = previousMacro->get();
                        macro.joinsFollowingToken = true;
                        previousMacro->token->trailing.push_back(
                            {NormalizedTriviaKind::Comment, TriviaPlacement::Trailing,
                             std::string(trivia.getRawText()), nullptr,
                             isLineCommentTrivia(trivia)});
                        lastDirectiveOwner = nullptr;
                        continue;
                    }
                    if (lineBreaks == 0 && lastDirectiveOwner) {
                        auto& directive = lastDirectiveOwner->at(lastDirectiveIndex);
                        directive.text.append("  ");
                        directive.text.append(trivia.getRawText());
                        lastDirectiveOwner = nullptr;
                        continue;
                    }
                    NormalizedTrivia item{NormalizedTriviaKind::Comment,
                                          TriviaPlacement::Standalone,
                                          std::string(trivia.getRawText()), nullptr,
                                          isLineCommentTrivia(trivia)};
                    bool endsLine = item.lineComment;
                    for (size_t i = triviaIndex + 1; i < triviaView.size(); i++) {
                        if (triviaView[i].kind == TriviaKind::EndOfLine) {
                            endsLine = true;
                            break;
                        }
                    }
                    bool prefixBlockComment = lineBreaks == 0 && !endsLine && previous &&
                                              previous->token.kind == TokenKind::OpenParenthesis &&
                                              current.startsListItem;
                    bool inlineBeforeElse = !item.lineComment && previous &&
                                            previous->token.kind == TokenKind::EndKeyword &&
                                            current.token.kind == TokenKind::ElseKeyword;
                    if (lineBreaks == 0 && previous && !prefixBlockComment) {
                        item.placement = endsLine && !inlineBeforeElse ? TriviaPlacement::Trailing
                                                                       : TriviaPlacement::Inline;
                        previous->trailing.push_back(std::move(item));
                    }
                    else {
                        appendBlankLines(current, lineBreaks);
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
                            if (directiveTrivia.kind == TriviaKind::DisabledText &&
                                directiveTrivia.getRawText() != "\n") {
                                suffix.append(directiveTrivia.getRawText());
                            }
                            else if (!suffix.empty() &&
                                     directiveTrivia.kind == TriviaKind::DisabledText) {
                                suffix.append(directiveTrivia.getRawText());
                            }
                        }
                        if (suffix.find_first_not_of(" \t\r\n") != std::string::npos) {
                            absorbedConditionalSuffix = true;
                            size_t comment = suffix.find("//");
                            bool lineComment = comment != std::string::npos &&
                                               suffix.find_first_not_of(" \t\r\n", 0) == comment;
                            if (lineComment) {
                                auto& branch = current.leading.back().text;
                                while (!branch.empty() &&
                                       (branch.back() == ' ' || branch.back() == '\t' ||
                                        branch.back() == '\r' || branch.back() == '\n')) {
                                    branch.pop_back();
                                }
                                size_t indent = 0;
                                size_t suffixLine = suffix.rfind('\n', comment);
                                if (suffixLine != std::string::npos) {
                                    suffixLine++;
                                    while (suffixLine + indent < comment &&
                                           (suffix[suffixLine + indent] == ' ' ||
                                            suffix[suffixLine + indent] == '\t')) {
                                        indent++;
                                    }
                                }
                                else {
                                    size_t branchLine = branch.rfind('\n');
                                    branchLine = branchLine == std::string::npos ? 0
                                                                                 : branchLine + 1;
                                    while (branchLine + indent < branch.size() &&
                                           (branch[branchLine + indent] == ' ' ||
                                            branch[branchLine + indent] == '\t')) {
                                        indent++;
                                    }
                                }
                                if (!branch.ends_with('\n'))
                                    branch.push_back('\n');
                                branch.append(indent, ' ');
                                size_t commentEnd = suffix.find_first_of("\r\n", comment);
                                branch.append(suffix, comment, commentEnd - comment);
                            }
                            else {
                                current.leading.back().text.append(suffix);
                            }
                        }
                    }
                    bool hadLineBreak = classifyDirectiveSubTrivia(
                        syntax, current, lineBreaks, previous, previousMacro,
                        previousDirectiveOwner, previousDirectiveIndex, previousDirectiveSyntax);
                    if (absorbedConditionalSuffix)
                        lineBreaks = 0;
                    size_t directiveLineBreaks = lineBreaks;
                    appendBlankLines(current, lineBreaks);

                    NormalizedTriviaKind kind = NormalizedTriviaKind::Directive;
                    if (syntax.kind == SyntaxKind::MacroUsage)
                        kind = NormalizedTriviaKind::MacroUsage;
                    else if (isConditionalDirective(syntax.kind))
                        kind = NormalizedTriviaKind::ConditionalDirective;

                    if (kind == NormalizedTriviaKind::MacroUsage && directiveLineBreaks > 1 &&
                        std::ranges::none_of(current.leading, [](const NormalizedTrivia& item) {
                            return item.kind == NormalizedTriviaKind::BlankLine;
                        })) {
                        current.leading.push_back({NormalizedTriviaKind::BlankLine,
                                                   TriviaPlacement::Standalone, "", nullptr,
                                                   false});
                    }

                    TriviaPlacement placement = !hadLineBreak && lineBreaks == 0 && previous
                                                    ? TriviaPlacement::Inline
                                                    : TriviaPlacement::Standalone;
                    NormalizedTrivia item{kind, placement, syntaxText(syntax, sourceManager),
                                          &syntax, false};
                    if (placement == TriviaPlacement::Inline && previous &&
                        kind == NormalizedTriviaKind::MacroUsage && !previousMacro &&
                        current.token.rawText().empty()) {
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
                            size_t content = firstLine.find_first_not_of(" \t");
                            if (content != std::string_view::npos) {
                                auto& directive = lastDirectiveOwner->at(lastDirectiveIndex);
                                auto inlineText = firstLine.substr(content);
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
                                current.leading.push_back({NormalizedTriviaKind::ConditionalBranch,
                                                           TriviaPlacement::Standalone,
                                                           std::move(disabled), nullptr, false});
                            }
                        }
                    }
                    lineBreaks = 0;
                    continue;
                }

                std::string text = trivia.kind == TriviaKind::SkippedTokens
                                       ? skippedText(trivia, sourceManager)
                                       : std::string(trivia.getRawText());
                if (!text.empty()) {
                    appendBlankLines(current, lineBreaks);
                    current.leading.push_back(
                        {NormalizedTriviaKind::Verbatim,
                         lineBreaks == 0 ? TriviaPlacement::Inline : TriviaPlacement::Standalone,
                         std::move(text), nullptr, false});
                    lineBreaks = 0;
                }
            }
            current.lineBreakBefore = lineBreaks > 0;
            appendBlankLines(current, lineBreaks);
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
            auto classify = [&](auto& triviaList) {
                for (size_t triviaIndex = 0; triviaIndex < triviaList.size(); triviaIndex++) {
                    auto& trivia = triviaList[triviaIndex];
                    if (trivia.kind != NormalizedTriviaKind::MacroUsage || !trivia.syntax)
                        continue;
                    bool ownsRecoveredAssignment = std::ranges::any_of(
                        triviaList.begin() + static_cast<ptrdiff_t>(triviaIndex + 1),
                        triviaList.end(), [](const NormalizedTrivia& following) {
                            return following.kind == NormalizedTriviaKind::Verbatim &&
                                   following.placement == TriviaPlacement::Inline &&
                                   following.text.find('=') != std::string::npos;
                        });
                    if (ownsRecoveredAssignment)
                        trivia.joinsFollowingToken = true;
                    const NormalizedToken* next = nullptr;
                    for (size_t i = tokenIndex; i < tokens.size(); i++) {
                        auto parsed = tokens[i].token;
                        if (parsed && !parsed.isMissing() && !tokens[i].fromMacroExpansion &&
                            !parsed.rawText().empty() && parsed.kind != TokenKind::EndOfFile) {
                            auto end = trivia.syntax->sourceRange().end();
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
                    auto end = trivia.syntax->sourceRange().end();
                    auto start = next->token.location();
                    if (!end || !start || end.buffer() != start.buffer() ||
                        end.offset() > start.offset()) {
                        continue;
                    }
                    auto source = sourceManager->getSourceText(end.buffer());
                    auto between = source.substr(end.offset(), start.offset() - end.offset());
                    trivia.joinsFollowingToken = trivia.joinsFollowingToken ||
                                                 between.find_first_of("\r\n") ==
                                                     std::string_view::npos;
                }
            };
            classify(tokens[tokenIndex].leading);
            classify(tokens[tokenIndex].trailing);
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
                        auto between = source.substr(previousEnd,
                                                     currentStart.offset() - previousEnd);
                        current.lineBreakBefore = between.find_first_of("\r\n") !=
                                                  std::string_view::npos;
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

NormalizedFormatDocument NormalizedFormatDocument::build(const SyntaxNode& root,
                                                         const SourceManager* sourceManager) {
    return NormalizedDocumentBuilder(root, sourceManager).build();
}

} // namespace format

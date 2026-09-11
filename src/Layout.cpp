//------------------------------------------------------------------------------
// Layout.cpp
// Lower normalized syntax into formatter document IR.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/Layout.h"

#include "format/FormatConstants.h"
#include "format/FormatStyle.h"
#include "format/FormatValidation.h"
#include "format/FormatterUtils.h"
#include <algorithm>
#include <optional>
#include <unordered_map>

#include "slang/diagnostics/ParserDiags.h"
#include "slang/parsing/Lexer.h"
#include "slang/parsing/LexerFacts.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/parsing/TokenKind.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxFacts.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/CharInfo.h"
#include "slang/text/SourceManager.h"

using namespace slang::parsing;
using namespace slang::syntax;

namespace format {
namespace {

bool isExpressionKind(SyntaxKind kind) {
    return ExpressionSyntax::isKind(kind) || PropertyExprSyntax::isKind(kind) ||
           SequenceExprSyntax::isKind(kind);
}

bool isAssignmentKind(SyntaxKind kind) {
    return SyntaxFacts::isAssignmentOperator(kind) || kind == SyntaxKind::EqualsValueClause;
}

bool isBinaryKind(SyntaxKind kind) {
    return BinaryExpressionSyntax::isKind(kind) || BinaryPropertyExprSyntax::isKind(kind) ||
           BinarySequenceExprSyntax::isKind(kind);
}

bool isComparisonKind(SyntaxKind kind) {
    switch (kind) {
        case SyntaxKind::EqualityExpression:
        case SyntaxKind::InequalityExpression:
        case SyntaxKind::CaseEqualityExpression:
        case SyntaxKind::CaseInequalityExpression:
        case SyntaxKind::WildcardEqualityExpression:
        case SyntaxKind::WildcardInequalityExpression:
        case SyntaxKind::LessThanExpression:
        case SyntaxKind::LessThanEqualExpression:
        case SyntaxKind::GreaterThanExpression:
        case SyntaxKind::GreaterThanEqualExpression:
            return true;
        default:
            return false;
    }
}

bool isListHandledExpression(SyntaxKind kind) {
    return kind == SyntaxKind::ConcatenationExpression ||
           kind == SyntaxKind::MultipleConcatenationExpression ||
           kind == SyntaxKind::AssignmentPatternExpression ||
           kind == SyntaxKind::StreamingConcatenationExpression;
}

bool tokenNeedsSeparation(Token left, Token right) {
    if (!left || !right)
        return false;
    if (left.kind == TokenKind::IntegerBase || right.kind == TokenKind::IntegerBase)
        return false;
    auto a = left.rawText();
    auto b = right.rawText();
    if (!a.empty() && a.front() == '\\')
        return true;
    if (a.empty() || b.empty())
        return false;
    if (slang::isValidCIdChar(a.back()) && slang::isValidCIdChar(b.front()))
        return true;

    // A style rule may remove spaces only if the lexer still sees both tokens.
    static const auto punctuation = [] {
        std::vector<std::string_view> result{"//", "/*"};
        for (auto kind : TokenKind_traits::values) {
            auto text = LexerFacts::getTokenKindText(kind);
            if (text.size() > 1 && !slang::isValidCIdChar(text.front()))
                result.push_back(text);
        }
        return result;
    }();
    return std::ranges::any_of(punctuation, [&](std::string_view text) {
        return text.size() > a.size() && text.starts_with(a) &&
               b.starts_with(text.substr(a.size()));
    });
}

class Lowerer {
public:
    Lowerer(const NormalizedFormatDocument& normalized, const Config& config, FormatStage stage)
        : normalized(normalized),
          config(config),
          stage(stage) {
        for (const auto& token : normalized.tokens()) {
            auto hasDefine = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    return trivia.syntax && trivia.syntax->kind == SyntaxKind::DefineDirective;
                });
            };
            hasMacroDefinitions = hasMacroDefinitions || hasDefine(token.leading) ||
                                  hasDefine(token.trailing);
        }
    }

    FormatDocument build() {
        lowerNode(normalized.root(), SyntaxKind::Unknown, true);
        auto root = builder.concat(std::move(output));
        return std::move(builder).finish(root);
    }

private:
    SyntaxKind alignmentRowKind() const {
        if (parameterPortDepth)
            return SyntaxKind::ParameterDeclaration;
        switch (currentMemberKind) {
            case SyntaxKind::NetDeclaration:
                return SyntaxKind::DataDeclaration;
            case SyntaxKind::ParameterDeclarationStatement:
                return SyntaxKind::ParameterDeclarationStatement;
            case SyntaxKind::DefaultCaseItem:
                return SyntaxKind::StandardCaseItem;
            case SyntaxKind::ExplicitAnsiPort:
                return SyntaxKind::ImplicitAnsiPort;
            default:
                return currentMemberKind;
        }
    }

    size_t mark() const { return output.size(); }

    DocId capture(size_t begin) {
        std::vector<DocId> children(output.begin() + static_cast<ptrdiff_t>(begin), output.end());
        output.erase(output.begin() + static_cast<ptrdiff_t>(begin), output.end());
        return builder.concat(std::move(children));
    }

    void append(DocId doc) {
        if (doc)
            output.push_back(doc);
    }

    void hardLine(int count = 1, bool useAnchor = false) {
        append(builder.hardLine(count, useAnchor));
        lineStart = true;
        spacingProvided = false;
        lastWasMacro = false;
        inlineBlockCommentOnLine = false;
    }

    void reindentConditionalLine() {
        if (!lineStart || conditionalDepth == itemFinalConditionalDepth)
            return;
        int relativeDepth = static_cast<int>(conditionalDepth) -
                            static_cast<int>(itemFinalConditionalDepth);
        append(builder.indent(
            relativeDepth * static_cast<int>(config.indentWidth.get()), builder.hardLine()
        ));
    }

    void emitConditionalVerbatim(
        std::string_view text,
        bool memberOwned = false,
        int conditionalDepthChange = 0
    ) {
        size_t leadingLineBreaks = 0;
        for (size_t pos = 0; pos < text.size() && slang::isWhitespace(text[pos]);) {
            if (slang::isNewline(text[pos])) {
                leadingLineBreaks++;
                pos = skipNewline(text, pos);
            }
            else {
                pos++;
            }
        }

        auto protectedLines = protectedLineStarts(text, false);
        size_t subparseBaseline = SIZE_MAX;
        size_t lineStartOffset = 0;
        while (lineStartOffset < text.size()) {
            size_t lineEnd = text.find('\n', lineStartOffset);
            if (lineEnd == std::string_view::npos)
                lineEnd = text.size();
            size_t indent = 0;
            while (lineStartOffset + indent < lineEnd &&
                   slang::isTabOrSpace(text[lineStartOffset + indent])) {
                indent++;
            }
            bool hasContent = lineStartOffset + indent < lineEnd &&
                              !slang::isNewline(text[lineStartOffset + indent]);
            if (hasContent && !protectedLines.contains(lineStartOffset))
                subparseBaseline = std::min(subparseBaseline, indent);
            lineStartOffset = lineEnd == text.size() ? text.size() : lineEnd + 1;
        }
        if (subparseBaseline == SIZE_MAX)
            subparseBaseline = 0;

        std::string subparseInput;
        subparseInput.reserve(text.size());
        lineStartOffset = 0;
        while (lineStartOffset < text.size()) {
            size_t lineEnd = text.find('\n', lineStartOffset);
            if (lineEnd == std::string_view::npos)
                lineEnd = text.size();
            size_t indent = 0;
            while (!protectedLines.contains(lineStartOffset) &&
                   lineStartOffset + indent < lineEnd && indent < subparseBaseline &&
                   slang::isTabOrSpace(text[lineStartOffset + indent])) {
                indent++;
            }
            subparseInput.append(
                text.substr(lineStartOffset + indent, lineEnd - lineStartOffset - indent)
            );
            if (lineEnd != text.size())
                subparseInput.push_back('\n');
            lineStartOffset = lineEnd == text.size() ? text.size() : lineEnd + 1;
        }

        // Inactive branches are absent from the parent CST. Reparse complete branches
        // opportunistically and leave context-dependent fragments verbatim.
        std::string subparsedText;
        slang::SourceManager sourceManager;
        slang::parsing::PreprocessorOptions preprocessorOptions;
        preprocessorOptions.maxIncludeDepth = 0;
        preprocessorOptions.dontExpandMacros = true;
        slang::Bag options(preprocessorOptions);
        auto tree = SyntaxTree::fromFileInMemory(
            subparseInput, sourceManager, "inactive conditional branch", "", options
        );
        bool hasParseError = tree &&
                             std::ranges::any_of(tree->diagnostics(), [](const auto& diagnostic) {
                                 return diagnostic.isError() &&
                                        diagnostic.code != slang::diag::NotAllowedInCU;
                             });
        if (tree && !hasParseError) {
            auto subparsed = NormalizedFormatDocument::build(tree->root(), &sourceManager);
            auto document = Lowerer(subparsed, config, stage).build();
            DocumentRenderer renderer(config);
            auto layout = renderer.renderLayout(document);
            subparsedText = stage == FormatStage::Layout
                                ? std::move(layout.text)
                                : renderer.renderAligned(document, layout).text;
            slang::SourceManager formattedSourceManager;
            auto formattedTree = SyntaxTree::fromFileInMemory(
                subparsedText, formattedSourceManager, "formatted inactive conditional branch", "",
                options
            );
            if (formattedTree && isTokenEquivalentTo(tree->root(), formattedTree->root())) {
                subparsedText.erase(
                    subparsedText.begin(), std::ranges::find_if_not(subparsedText, slang::isNewline)
                );
                subparsedText.insert(0, leadingLineBreaks, '\n');
                text = subparsedText;
            }
        }

        if (lineStart && memberOwned) {
            while (!text.empty() && slang::isWhitespace(text.front()))
                text.remove_prefix(1);
        }
        else if (lineStart) {
            auto firstContent = std::ranges::find_if_not(text, slang::isTabOrSpace);
            if (firstContent != text.end() && slang::isNewline(*firstContent))
                text.remove_prefix(size_t(firstContent - text.begin()));
            text.remove_prefix(skipNewline(text, 0));
        }
        while (!text.empty() && slang::isWhitespace(text.back())) {
            text.remove_suffix(1);
        }
        protectedLines = protectedLineStarts(text, false);

        size_t baseline = 0;
        lineStartOffset = 0;
        while (lineStartOffset < text.size()) {
            size_t lineEnd = text.find('\n', lineStartOffset);
            if (lineEnd == std::string_view::npos)
                lineEnd = text.size();
            size_t indent = 0;
            while (lineStartOffset + indent < lineEnd &&
                   slang::isTabOrSpace(text[lineStartOffset + indent])) {
                indent++;
            }
            if (lineStartOffset + indent < lineEnd) {
                baseline = indent;
                break;
            }
            lineStartOffset = lineEnd == text.size() ? text.size() : lineEnd + 1;
        }

        std::vector<size_t> nestedConditionalIndents;
        size_t listIndent = !memberOwned && conditionalDepth > itemFinalConditionalDepth &&
                                    !dedentListConditionalDirective
                                ? config.indentWidth.get()
                                : 0;
        lineStartOffset = 0;
        bool firstLine = true;
        size_t blankLineRun = 0;
        while (lineStartOffset < text.size()) {
            size_t lineEnd = text.find('\n', lineStartOffset);
            if (lineEnd == std::string_view::npos)
                lineEnd = text.size();
            if (protectedLines.contains(lineStartOffset)) {
                append(builder.verbatim(
                    "\n" + std::string(text.substr(lineStartOffset, lineEnd - lineStartOffset))
                ));
                lineStartOffset = lineEnd == text.size() ? text.size() : lineEnd + 1;
                continue;
            }
            size_t indent = 0;
            while (lineStartOffset + indent < lineEnd &&
                   slang::isTabOrSpace(text[lineStartOffset + indent])) {
                indent++;
            }
            auto line = text.substr(lineStartOffset + indent, lineEnd - lineStartOffset - indent);
            bool closesNested = line.starts_with("`else") || line.starts_with("`elsif") ||
                                line.starts_with("`endif");
            if (closesNested && !nestedConditionalIndents.empty())
                nestedConditionalIndents.pop_back();
            size_t relativeIndent = memberOwned          ? 0
                                    : indent >= baseline ? indent - baseline
                                                         : indent;
            if (!nestedConditionalIndents.empty())
                relativeIndent = std::max(relativeIndent, nestedConditionalIndents.back());
            relativeIndent += listIndent;
            if (line.empty()) {
                blankLineRun++;
                append(builder.indent(
                    static_cast<int>(relativeIndent),
                    builder.hardLine(static_cast<int>(blankLineRun + 1))
                ));
            }
            else if (firstLine) {
                size_t firstLineIndent = blankLineRun ? 0 : relativeIndent;
                blankLineRun = 0;
                if (!memberOwned && lineStart) {
                    append(builder.indent(static_cast<int>(relativeIndent), builder.hardLine()));
                    firstLineIndent = 0;
                }
                append(
                    memberOwned
                        ? builder.memberVerbatim(line, static_cast<int>(relativeIndent))
                        : builder.verbatim(std::string(firstLineIndent, ' ') + std::string(line))
                );
                firstLine = false;
            }
            else {
                blankLineRun = 0;
                auto content = memberOwned
                                   ? builder.memberVerbatim(line, static_cast<int>(relativeIndent))
                                   : builder.verbatim(line);
                append(
                    memberOwned ? builder.concat({builder.hardLine(), content})
                                : builder.indent(
                                      static_cast<int>(relativeIndent),
                                      builder.concat({builder.hardLine(), content})
                                  )
                );
            }
            if (line.starts_with("`ifdef") || line.starts_with("`ifndef") ||
                line.starts_with("`else") || line.starts_with("`elsif")) {
                nestedConditionalIndents.push_back(
                    relativeIndent - listIndent + config.indentWidth.get()
                );
            }
            lineStartOffset = lineEnd == text.size() ? text.size() : lineEnd + 1;
        }
        lineStart = false;
        spacingProvided = false;
        lastWasMacro = false;
        if (conditionalDepthChange < 0) {
            conditionalDepth -=
                std::min(conditionalDepth, static_cast<size_t>(-conditionalDepthChange));
        }
        else {
            conditionalDepth += static_cast<size_t>(conditionalDepthChange);
        }
    }

    void emitRecoveredAssignment(std::string_view text) {
        while (!text.empty() && slang::isTabOrSpace(text.front()))
            text.remove_prefix(1);
        if (!lineStart && !spacingProvided)
            append(builder.text(" "));
        slang::SourceManager sm;
        slang::BumpAllocator allocator;
        slang::Diagnostics diagnostics;
        Lexer lexer(sm.assignText(text), allocator, diagnostics, sm);
        size_t equals = std::string_view::npos;
        for (auto token = lexer.lex(); token.kind != TokenKind::EndOfFile; token = lexer.lex()) {
            if (token.kind == TokenKind::Equals) {
                equals = token.location().offset();
                break;
            }
        }
        if (equals == std::string_view::npos) {
            append(builder.verbatim(text));
            lineStart = false;
            spacingProvided = false;
            lastWasMacro = false;
            return;
        }
        auto rhs = text.substr(equals + 1);
        int assignmentPriority = rhs.find(" && ") == std::string_view::npos ? 1 : 3;
        append(builder.verbatim(text.substr(0, equals + 1)));
        append(builder.indent(
            static_cast<int>(config.indentWidth.get()),
            builder.softLine(assignmentPriority, " ", 0, true)
        ));

        while (!rhs.empty() && slang::isWhitespace(rhs.front()))
            rhs.remove_prefix(1);

        if (!protectedTextRanges(rhs).empty()) {
            append(builder.verbatim(rhs));
        }
        else if (size_t logicalAnd = rhs.find(" && "); logicalAnd != std::string_view::npos) {
            append(builder.verbatim(rhs.substr(0, logicalAnd)));
            append(builder.indent(
                static_cast<int>(config.indentWidth.get()), builder.softLine(2, " ", 0, true)
            ));
            append(builder.verbatim(rhs.substr(logicalAnd + 1)));
        }
        else if (size_t openParen = rhs.find('('); openParen != std::string_view::npos &&
                                                   config.columnLimit.get() &&
                                                   rhs.size() > config.columnLimit.get() * 3 / 5) {
            append(builder.verbatim(rhs.substr(0, openParen + 1)));
            append(builder.indent(
                static_cast<int>(config.indentWidth.get() * 2), builder.softLine(2, "", 0, true)
            ));
            auto arguments = rhs.substr(openParen + 1);
            while (!arguments.empty() && slang::isWhitespace(arguments.front()))
                arguments.remove_prefix(1);
            append(builder.verbatim(arguments));
        }
        else {
            append(builder.verbatim(rhs));
        }
        lineStart = false;
        spacingProvided = true;
        lastWasMacro = false;
    }

    void emitTrivia(const NormalizedTrivia& trivia, bool trailing, bool memberOwned = false) {
        switch (trivia.kind) {
            case NormalizedTriviaKind::Unformatted: {
                auto text = std::string_view(trivia.text);
                if (trivia.endsLine && !lineStart)
                    hardLine();
                if (lineStart)
                    text.remove_prefix(skipNewline(text, 0));
                append(builder.preservedText(text, lineStart));
                lineStart = text.ends_with('\n');
                if (trivia.endsLine && !lineStart)
                    hardLine();
                spacingProvided = false;
                lastWasMacro = false;
                break;
            }
            case NormalizedTriviaKind::BlankLine:
                hardLine(static_cast<int>(trivia.lineBreakCount));
                break;
            case NormalizedTriviaKind::Comment: {
                auto commentText = std::string_view(trivia.text);
                while (!commentText.empty() && slang::isTabOrSpace(commentText.back()))
                    commentText.remove_suffix(1);
                if (trivia.placement == TriviaPlacement::Inline && !trivia.lineComment)
                    inlineBlockCommentOnLine = true;
                bool afterMacro = lastWasMacro;
                lastWasMacro = false;
                if (!trailing)
                    reindentConditionalLine();
                if (!trailing && trivia.placement == TriviaPlacement::Inline &&
                    !trivia.lineComment) {
                    if (!spacingProvided && !lineStart)
                        append(builder.text(" "));
                    append(builder.verbatim(commentText));
                    if (trivia.endsLine || (inDynamicList && dynamicListLikelyVertical))
                        hardLine(1, true);
                    else {
                        append(builder.text(" "));
                        spacingProvided = true;
                    }
                    break;
                }
                if (trailing && trivia.placement == TriviaPlacement::Trailing) {
                    bool ternaryComment = lastToken &&
                                          (lastToken->token.kind == TokenKind::Question ||
                                           lastToken->token.kind == TokenKind::Colon);
                    size_t spaces = (!trivia.lineComment && !inDynamicList && lastToken &&
                                     lastToken->token.kind != TokenKind::Semicolon) ||
                                            afterMacro ||
                                            (trivia.preserveSingleSpace &&
                                             currentMemberKind != SyntaxKind::ImplicitAnsiPort) ||
                                            currentMemberContainsMacro
                                        ? 1
                                        : config.spacesBeforeTrailingComment.get();
                    append(builder.text(std::string(spaces, ' ')));
                    if (!inDynamicList && !ternaryComment &&
                        std::ranges::none_of(commentText, slang::isNewline)) {
                        uint32_t separatorColumn =
                            currentMemberKind == SyntaxKind::NamedPortConnection ||
                                    currentMemberKind == SyntaxKind::NamedParamAssignment
                                ? 1
                                : 0;
                        append(builder.alignmentAnchor(
                            100, currentMemberKind, currentAlignmentGroup, separatorColumn
                        ));
                    }
                }
                else if (trailing) {
                    size_t spaces = (lastToken && lastToken->token.kind == TokenKind::Semicolon) ||
                                            dynamicListLikelyVertical
                                        ? config.spacesBeforeTrailingComment.get()
                                        : 1;
                    append(builder.text(std::string(spaces, ' ')));
                }
                else if (!lineStart &&
                         (!inDynamicList || trivia.placement == TriviaPlacement::Standalone)) {
                    hardLine();
                }
                append(builder.verbatim(commentText));
                if (trivia.lineComment) {
                    hardLine(
                        1, !trailing || !emittingAssignmentOperator ||
                               assignmentOperatorCommentUseAnchor
                    );
                }
                else if (!trailing) {
                    hardLine(1, true);
                }
                else {
                    spacingProvided = false;
                }
                break;
            }
            case NormalizedTriviaKind::MacroUsage: {
                if (lastWasMacro)
                    hardLine();
                bool listContinuation = inDynamicList && lastToken &&
                                        lastToken->token.kind == TokenKind::Comma;
                if (!trailing && trivia.placement == TriviaPlacement::Standalone && !lineStart &&
                    !listContinuation) {
                    hardLine();
                }
                if (trailing && inDynamicList && lastToken &&
                    lastToken->token.kind == TokenKind::Comma && !lineStart) {
                    if (dynamicListForceVertical)
                        hardLine(1, true);
                    else
                        append(builder.softLine(1, " ", currentDynamicGroup));
                    spacingProvided = true;
                }
                if (trailing && bracketDepth == 0 && lastToken && !lineStart && !spacingProvided &&
                    shouldInsertWhitespace(
                        lastToken->token.kind, TokenKind::Identifier, lastToken->parentKind,
                        SyntaxKind::Unknown, false
                    )) {
                    append(builder.text(" "));
                }
                if (!trailing && bracketDepth == 0 && trivia.placement == TriviaPlacement::Inline &&
                    lastToken && !lineStart && !spacingProvided &&
                    shouldInsertWhitespace(
                        lastToken->token.kind, TokenKind::Identifier, lastToken->parentKind,
                        SyntaxKind::Unknown, false
                    )) {
                    append(builder.text(" "));
                }
                auto indents = macroArgumentIndents(trivia.text);
                size_t macroBegin = mark();
                size_t offset = 0;
                for (auto indent : indents) {
                    append(builder.verbatim(
                        std::string_view(trivia.text).substr(offset, indent.begin - offset)
                    ));
                    append(builder.indent(
                        indent.closing ? 0 : static_cast<int>(config.indentWidth.get()),
                        builder.hardLine(1, true)
                    ));
                    offset = indent.end;
                }
                append(builder.verbatim(std::string_view(trivia.text).substr(offset)));
                if (!indents.empty())
                    append(builder.relativeAnchor(0, capture(macroBegin)));
                lineStart = false;
                spacingProvided = false;
                lastWasMacro = true;
                if (!trivia.joinsFollowingToken && !currentItemMacroOnly)
                    hardLine();
                break;
            }
            case NormalizedTriviaKind::Directive:
                lastWasMacro = false;
                {
                    size_t begin = mark();
                    append(builder.hardLine());
                    append(builder.verbatim(trivia.text));
                    auto directive = capture(begin);
                    int relativeDepth = static_cast<int>(conditionalDepth) -
                                        static_cast<int>(itemFinalConditionalDepth);
                    if (relativeDepth) {
                        directive = builder.indent(
                            relativeDepth * static_cast<int>(config.indentWidth.get()), directive
                        );
                    }
                    append(directive);
                }
                lineStart = false;
                hardLine();
                break;
            case NormalizedTriviaKind::ConditionalDirective: {
                lastWasMacro = false;
                auto kind = trivia.syntax ? trivia.syntax->kind : SyntaxKind::Unknown;
                if (trivia.placement == TriviaPlacement::Inline && !lineStart) {
                    bool closing = kind == SyntaxKind::EndIfDirective;
                    if (!lineStart && lastToken &&
                        lastToken->token.kind != TokenKind::OpenParenthesis && !spacingProvided) {
                        append(builder.text(" "));
                    }
                    append(builder.verbatim(trivia.text));
                    if (!closing) {
                        append(builder.text(" "));
                        spacingProvided = true;
                        inlineConditionalClosed = false;
                    }
                    else {
                        spacingProvided = false;
                        inlineConditionalClosed = true;
                        if (!conditionalAssignmentStack.empty())
                            conditionalAssignmentStack.pop_back();
                    }
                    if (!closing && (kind == SyntaxKind::IfDefDirective ||
                                     kind == SyntaxKind::IfNDefDirective)) {
                        conditionalAssignmentStack.push_back(
                            conditionalAssignmentRhs &&
                            currentMemberKind == SyntaxKind::ContinuousAssign
                        );
                    }
                    break;
                }

                bool closesBranch = kind == SyntaxKind::ElseDirective ||
                                    kind == SyntaxKind::ElsIfDirective ||
                                    kind == SyntaxKind::EndIfDirective;
                if (closesBranch && conditionalDepth > 0)
                    conditionalDepth--;
                if (kind == SyntaxKind::EndIfDirective && !conditionalAssignmentStack.empty()) {
                    conditionalAssignmentStack.pop_back();
                }
                auto directive =
                    builder.concat({builder.hardLine(), builder.verbatim(trivia.text)});
                int relativeDepth = static_cast<int>(conditionalDepth) -
                                    static_cast<int>(itemFinalConditionalDepth) -
                                    (dedentListConditionalDirective ? 1 : 0);
                if (relativeDepth) {
                    directive = builder.indent(
                        relativeDepth * static_cast<int>(config.indentWidth.get()), directive
                    );
                }
                append(directive);
                lineStart = false;
                hardLine();
                if (kind != SyntaxKind::EndIfDirective)
                    conditionalDepth++;
                if (kind == SyntaxKind::IfDefDirective || kind == SyntaxKind::IfNDefDirective) {
                    conditionalAssignmentStack.push_back(
                        conditionalAssignmentRhs &&
                        currentMemberKind == SyntaxKind::ContinuousAssign
                    );
                }
                break;
            }
            case NormalizedTriviaKind::ConditionalBranch:
                lastWasMacro = false;
                emitConditionalVerbatim(trivia.text);
                break;
            case NormalizedTriviaKind::Verbatim:
                if (trivia.placement == TriviaPlacement::Standalone && lastWasMacro &&
                    std::ranges::any_of(trivia.text, slang::isNewline)) {
                    auto text = std::string_view(trivia.text);
                    while (!text.empty() && slang::isWhitespace(text.front()))
                        text.remove_prefix(1);
                    if (text.starts_with('=')) {
                        append(builder.text(" "));
                        append(builder.memberVerbatim(text));
                        lineStart = text.ends_with('\n');
                        spacingProvided = !lineStart;
                        lastWasMacro = false;
                        break;
                    }
                }
                lastWasMacro = false;
                if (conditionalDepth > 0 && trivia.text.find('\n') != std::string::npos) {
                    emitConditionalVerbatim(trivia.text, true, trivia.conditionalDepthChange);
                    break;
                }
                if (trivia.placement == TriviaPlacement::Inline &&
                    trivia.text.find('\n') == std::string::npos &&
                    trivia.text.find('=') != std::string::npos) {
                    emitRecoveredAssignment(trivia.text);
                    break;
                }
                if (trivia.placement == TriviaPlacement::Standalone && !lineStart)
                    hardLine();
                {
                    std::string_view text = trivia.text;
                    if (trivia.placement == TriviaPlacement::Inline) {
                        bool separated = !text.empty() && slang::isTabOrSpace(text.front());
                        while (!text.empty() && slang::isTabOrSpace(text.front()))
                            text.remove_prefix(1);
                        if (!text.empty() && !lineStart && !spacingProvided && separated)
                            append(builder.text(" "));
                    }
                    if (lineStart && text.starts_with('\n'))
                        text.remove_prefix(1);
                    if (lineStart) {
                        while (!text.empty() && slang::isTabOrSpace(text.front()))
                            text.remove_prefix(1);
                    }
                    append(memberOwned ? builder.memberVerbatim(text) : builder.verbatim(text));
                    if (trivia.endsLine && !text.ends_with('\n'))
                        hardLine();
                    else {
                        if (trivia.preserveSingleSpace && !text.empty() &&
                            !slang::isTabOrSpace(text.back())) {
                            append(builder.text(" "));
                        }
                        lineStart = text.ends_with('\n');
                    }
                    spacingProvided =
                        trivia.preserveSingleSpace ||
                        (trivia.placement == TriviaPlacement::Inline && !trivia.endsLine &&
                         !text.ends_with('=') && (!text.ends_with(':') || text.ends_with("::")) &&
                         !text.ends_with('-') && !text.ends_with('+') && !text.ends_with('?'));
                    if (trivia.placement == TriviaPlacement::Inline && text.ends_with('=')) {
                        pendingAssignmentBreak = true;
                        pendingRecoveredAssignmentBreak = true;
                    }
                }
                break;
        }
    }

    int breakPriority(SyntaxKind parentKind) const {
        if (parentKind == SyntaxKind::ConditionalExpression)
            return ternaryBreakPriority;
        if (isBinaryKind(parentKind))
            return expressionBreakPriority;
        int precedence = SyntaxFacts::getPrecedence(parentKind);
        return precedence > 0 ? precedence : 50;
    }

    bool shouldBreakBefore(const NormalizedToken& token) const {
        if (bracketDepth > 0)
            return false;
        if (manualExpressionBreaks && token.parentKind == SyntaxKind::ConditionalExpression) {
            return false;
        }
        if (!isBinaryKind(token.parentKind) &&
            token.parentKind != SyntaxKind::ConditionalExpression) {
            return false;
        }
        if (isAssignmentKind(token.parentKind))
            return false;
        if (isComparisonKind(token.parentKind) && !allowComparisonBreak)
            return false;
        if (!allowCurrentBinaryBreak)
            return false;
        return token.token.kind != TokenKind::OpenParenthesis &&
               token.token.kind != TokenKind::CloseParenthesis;
    }

    void emitToken(size_t tokenIndex) {
        const auto& normalizedToken = normalized.tokens().at(tokenIndex);
        auto token = normalizedToken.token;
        bool savedDedentConditionalContent = dedentConditionalContent;
        if (awaitingClosingTrivia && token.kind == TokenKind::CloseParenthesis)
            dedentConditionalContent = true;
        bool closingTokenHasConditional =
            token.kind == TokenKind::EndKeyword &&
            std::ranges::any_of(normalizedToken.leading, [](const NormalizedTrivia& trivia) {
                return trivia.kind == NormalizedTriviaKind::ConditionalDirective;
            });
        if (awaitingClosingTrivia && !normalizedToken.leading.empty()) {
            size_t begin = mark();
            for (const auto& trivia : normalizedToken.leading)
                emitTrivia(trivia, false);
            auto triviaDoc = capture(begin);
            bool hasConditional =
                std::ranges::any_of(normalizedToken.leading, [](const NormalizedTrivia& trivia) {
                    return trivia.kind == NormalizedTriviaKind::ConditionalDirective;
                });
            if (hasConditional && token.kind == TokenKind::CloseParenthesis)
                append(triviaDoc);
            else if (token.kind == TokenKind::EndKeyword && hasConditional)
                append(builder.indent(
                    static_cast<int>(config.indentWidth.get()),
                    builder.concat({builder.hardLine(), triviaDoc})
                ));
            else if (token.kind == TokenKind::EndKeyword) {
                bool followsBlank = std::ranges::any_of(
                    normalizedToken.leading, [](const NormalizedTrivia& trivia) {
                        return trivia.kind == NormalizedTriviaKind::BlankLine;
                    }
                );
                if (followsBlank)
                    append(triviaDoc);
                else
                    append(builder.indent(
                        static_cast<int>(config.indentWidth.get()),
                        builder.concat({builder.hardLine(), triviaDoc})
                    ));
            }
            else
                append(builder.indent(
                    static_cast<int>(config.indentWidth.get()),
                    builder.concat({builder.hardLine(), triviaDoc})
                ));
            append(builder.hardLine());
            lineStart = true;
        }
        else if (closingTokenHasConditional) {
            size_t begin = mark();
            for (const auto& trivia : normalizedToken.leading)
                emitTrivia(trivia, false);
            auto triviaDoc = capture(begin);
            append(builder.indent(static_cast<int>(config.indentWidth.get()), triviaDoc));
            append(builder.hardLine());
            lineStart = true;
        }
        else {
            for (const auto& trivia : normalizedToken.leading) {
                if (skipLeadingCommentsToken == tokenIndex &&
                    trivia.kind == NormalizedTriviaKind::Comment) {
                    continue;
                }
                emitTrivia(trivia, false);
            }
        }
        if (skipLeadingCommentsToken == tokenIndex)
            skipLeadingCommentsToken.reset();
        awaitingClosingTrivia = false;
        dedentConditionalContent = savedDedentConditionalContent;

        if (!token || token.isMissing() || token.kind == TokenKind::EndOfFile ||
            normalizedToken.fromMacroExpansion || token.rawText().empty()) {
            for (const auto& trivia : normalizedToken.trailing)
                emitTrivia(trivia, true);
            return;
        }

        reindentConditionalLine();

        if (token.kind == TokenKind::Semicolon && lineStart)
            append(builder.hardLine());

        if (pendingTokenIndent && lineStart) {
            append(builder.text(std::string(pendingTokenIndent, ' ')));
            pendingTokenIndent = 0;
            lineStart = false;
            spacingProvided = true;
        }

        bool beforeOperator = shouldBreakBefore(normalizedToken) && lastToken && !lineStart &&
                              token.kind != TokenKind::DoubleStar;
        if (pendingAssignmentBreak && (lastWasMacro || token.kind == TokenKind::Semicolon)) {
            pendingAssignmentBreak = false;
            pendingRecoveredAssignmentBreak = false;
        }
        else if (pendingAssignmentBreak && !lineStart) {
            append(builder.softLine(100, " ", 0, pendingRecoveredAssignmentBreak));
            spacingProvided = true;
            pendingAssignmentBreak = false;
            pendingRecoveredAssignmentBreak = false;
        }
        else if (beforeOperator) {
            bool operatorHasLineComment =
                std::ranges::any_of(normalizedToken.trailing, [](const NormalizedTrivia& trivia) {
                    return trivia.lineComment;
                });
            if (hardBreakCurrentBinary ||
                (operatorHasLineComment &&
                 normalizedToken.parentKind == SyntaxKind::LogicalOrExpression)) {
                auto line = builder.hardLine(1, true);
                append(
                    binaryContinuationIndent ? builder.indent(binaryContinuationIndent, line) : line
                );
            }
            else {
                auto line = builder.softLine(breakPriority(normalizedToken.parentKind));
                append(
                    binaryContinuationIndent ? builder.indent(binaryContinuationIndent, line) : line
                );
            }
            spacingProvided = true;
        }

        bool compact = bracketDepth > 0;
        bool tokenInDataType = (normalizedToken.inDataType &&
                                normalizedToken.parentKind != SyntaxKind::VariableDimension) ||
                               macroVariableDimension || spaceBeforeMacroVariableDimension;
        if (inlineConditionalClosed && token.kind == TokenKind::Semicolon && !lineStart) {
            append(builder.text(" "));
            spacingProvided = true;
        }
        if (!lineStart && !spacingProvided) {
            if (lastToken && lastToken->token.kind == TokenKind::DoubleStar &&
                currentMemberContainsMacro) {
                append(builder.text(" "));
            }
            else if (noNameInstantiation && !noNameCallStyle &&
                     token.kind == TokenKind::OpenParenthesis) {
                append(builder.text(" "));
            }
            else if (lastWasMacro) {
                if (token.kind == TokenKind::Identifier ||
                    (!compact && shouldInsertWhitespace(
                                     TokenKind::Identifier, token.kind, SyntaxKind::Unknown,
                                     normalizedToken.parentKind, tokenInDataType
                                 ))) {
                    append(builder.text(" "));
                }
            }
            else if (lastToken &&
                     (tokenNeedsSeparation(lastToken->token, token) ||
                      (!compact &&
                       shouldInsertWhitespace(
                           lastToken->token.kind, token.kind, lastToken->parentKind,
                           normalizedToken.parentKind, tokenInDataType
                       ) &&
                       !(noNameCallStyle && token.kind == TokenKind::OpenParenthesis &&
                         (normalizedToken.parentKind == SyntaxKind::HierarchicalInstance ||
                          lastToken->parentKind == SyntaxKind::HierarchyInstantiation))))) {
                append(builder.text(" "));
            }
        }
        if (token.kind == TokenKind::OpenParenthesis &&
            normalizedToken.parentKind == SyntaxKind::HierarchicalInstance &&
            instanceOpenParenPadding) {
            append(builder.text(std::string(instanceOpenParenPadding, ' ')));
        }

        bool portDirection =
            (normalizedToken.parentKind == SyntaxKind::VariablePortHeader ||
             normalizedToken.parentKind == SyntaxKind::NetPortHeader ||
             normalizedToken.parentKind == SyntaxKind::FunctionPort) &&
            (token.kind == TokenKind::InputKeyword || token.kind == TokenKind::OutputKeyword ||
             token.kind == TokenKind::InOutKeyword || token.kind == TokenKind::RefKeyword);
        if (currentMemberKind != SyntaxKind::Unknown && !suppressAlignment) {
            if (lineStart && !portDirectionAnchorEmitted &&
                alignmentRowKind() == SyntaxKind::ImplicitAnsiPort && !portDirection) {
                append(builder.alignmentAnchor(0, alignmentRowKind(), currentAlignmentGroup));
                portDirectionAnchorEmitted = true;
            }
            auto rowKind = alignmentRowKind();
            if (token.kind == TokenKind::OpenBracket && normalizedToken.inDataType &&
                (rowKind == SyntaxKind::DataDeclaration ||
                 rowKind == SyntaxKind::ImplicitAnsiPort ||
                 rowKind == SyntaxKind::StructUnionMember ||
                 rowKind == SyntaxKind::TypedefDeclaration) &&
                !packedDimensionAnchorEmitted) {
                uint32_t column = rowKind == SyntaxKind::ImplicitAnsiPort ? 1 : 0;
                append(builder.alignmentAnchor(column, rowKind, currentAlignmentGroup));
                packedDimensionAnchorEmitted = true;
            }
            else if (token.kind == TokenKind::OpenParenthesis &&
                     (normalizedToken.parentKind == SyntaxKind::NamedPortConnection ||
                      normalizedToken.parentKind == SyntaxKind::NamedParamAssignment ||
                      normalizedToken.parentKind == SyntaxKind::NamedArgument) &&
                     !suppressEscapedAlignment) {
                append(builder.alignmentAnchor(1, alignmentRowKind(), currentAlignmentGroup, 1));
            }
            else if (token.kind == TokenKind::Identifier &&
                     normalizedToken.parentKind == SyntaxKind::Declarator &&
                     currentDeclaratorAlignmentColumn && !inlineBlockCommentOnLine &&
                     !(subroutineDepth && *currentDeclaratorAlignmentColumn > 1)) {
                uint32_t column = *currentDeclaratorAlignmentColumn;
                if (rowKind == SyntaxKind::ImplicitAnsiPort)
                    column++;
                append(builder.alignmentAnchor(column, rowKind, currentAlignmentGroup));
            }
            else if (parameterPortDepth && token.kind == TokenKind::Identifier &&
                     normalizedToken.parentKind == SyntaxKind::TypeAssignment) {
                append(builder.alignmentAnchor(1, alignmentRowKind(), currentAlignmentGroup));
            }
            else if (token.kind == TokenKind::Identifier &&
                     normalizedToken.parentKind == SyntaxKind::TypedefDeclaration &&
                     !suppressTypedefAlignment) {
                append(builder.alignmentAnchor(1, alignmentRowKind(), currentAlignmentGroup));
            }
            else if ((token.kind == TokenKind::Equals ||
                      (token.kind == TokenKind::LessThanEquals &&
                       isAssignmentKind(normalizedToken.parentKind))) &&
                     currentMemberKind != SyntaxKind::CoverageBins &&
                     currentMemberKind != SyntaxKind::ContinuousAssign &&
                     currentMemberKind != SyntaxKind::PathDeclaration &&
                     currentMemberKind != SyntaxKind::Declarator && !parameterPortDepth &&
                     !suppressEscapedAlignment) {
                uint32_t column = currentDeclaratorAlignmentColumn
                                      ? *currentDeclaratorAlignmentColumn + 1
                                      : 2;
                append(builder.alignmentAnchor(column, alignmentRowKind(), currentAlignmentGroup));
            }
        }

        append(builder.text(token.rawText(), normalizedToken.parentKind));
        if (currentMemberKind != SyntaxKind::Unknown && !suppressAlignment) {
            bool parameterKeyword =
                (normalizedToken.parentKind == SyntaxKind::ParameterDeclaration ||
                 normalizedToken.parentKind == SyntaxKind::TypeParameterDeclaration) &&
                (token.kind == TokenKind::ParameterKeyword ||
                 token.kind == TokenKind::LocalParamKeyword);
            if (portDirection || parameterKeyword) {
                append(builder.alignmentAnchor(0, alignmentRowKind(), currentAlignmentGroup));
                portDirectionAnchorEmitted = portDirection;
            }
            if (token.kind == TokenKind::Identifier &&
                normalizedToken.parentKind == SyntaxKind::Declarator &&
                currentMemberKind == SyntaxKind::Declarator) {
                append(builder.alignmentAnchor(2, alignmentRowKind(), currentAlignmentGroup));
            }
            if (parameterPortDepth && token.kind == TokenKind::Identifier &&
                ((normalizedToken.parentKind == SyntaxKind::Declarator &&
                  currentDeclaratorAlignmentColumn == 1) ||
                 normalizedToken.parentKind == SyntaxKind::TypeAssignment)) {
                append(builder.alignmentAnchor(2, alignmentRowKind(), currentAlignmentGroup));
            }
            if (token.kind == TokenKind::Colon &&
                (normalizedToken.parentKind == SyntaxKind::AssignmentPatternItem ||
                 (!suppressCaseAlignment &&
                  (normalizedToken.parentKind == SyntaxKind::StandardCaseItem ||
                   normalizedToken.parentKind == SyntaxKind::DefaultCaseItem)))) {
                append(builder.alignmentAnchor(1, alignmentRowKind(), currentAlignmentGroup));
            }
        }
        lineStart = false;
        spacingProvided = false;
        lastWasMacro = false;
        inlineConditionalClosed = false;
        lastToken = &normalizedToken;

        if (breakMacroDimensionAfterPlus && token.kind == TokenKind::Plus &&
            normalizedToken.parentKind == SyntaxKind::AddExpression) {
            hardLine(1, true);
        }

        if (token.kind == TokenKind::OpenBracket)
            bracketDepth++;
        else if (token.kind == TokenKind::CloseBracket && bracketDepth)
            bracketDepth--;

        if (isAssignmentKind(normalizedToken.parentKind)) {
            pendingAssignmentBreak = true;
            pendingRecoveredAssignmentBreak = false;
        }

        for (const auto& trivia : normalizedToken.trailing)
            emitTrivia(trivia, true);
    }

    void lowerChild(
        const NormalizedChild& child,
        SyntaxKind parentExpressionKind = SyntaxKind::Unknown
    ) {
        if (auto token = std::get_if<size_t>(&child.value)) {
            emitToken(*token);
        }
        else if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value)) {
            lowerNode(**node, parentExpressionKind, false);
        }
        else {
            lowerList(**std::get_if<std::unique_ptr<NormalizedList>>(&child.value));
        }
    }

    static const NormalizedNode* childNode(const NormalizedChild& child) {
        if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&child.value))
            return node->get();
        return nullptr;
    }

    static bool isBlock(const NormalizedNode& node) {
        return node.kind == SyntaxKind::SequentialBlockStatement ||
               node.kind == SyntaxKind::ParallelBlockStatement;
    }

    static bool containsKind(const NormalizedNode& node, SyntaxKind kind) {
        if (node.kind == kind)
            return true;
        for (const auto& child : node.children) {
            if (auto nested = childNode(child); nested && containsKind(*nested, kind))
                return true;
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (const auto& listChild : (*list)->children) {
                    if (auto nested = childNode(listChild); nested && containsKind(*nested, kind))
                        return true;
                }
            }
        }
        return false;
    }

    static bool containsMultilineList(const NormalizedNode& node) {
        for (const auto& child : node.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                if ((*list)->style != ListStyle::Inline)
                    return true;
                for (const auto& listChild : (*list)->children) {
                    if (auto nested = childNode(listChild);
                        nested && containsMultilineList(*nested))
                        return true;
                }
            }
            else if (auto nested = childNode(child); nested && containsMultilineList(*nested)) {
                return true;
            }
        }
        return false;
    }

    static bool isParenthesizedBinary(const NormalizedNode& node) {
        const NormalizedNode* current = &node;
        while (current->kind == SyntaxKind::ParenthesizedExpression) {
            const NormalizedNode* parenthesized = current;
            current = nullptr;
            for (const auto& child : parenthesized->children) {
                if (auto nested = childNode(child); nested && isExpressionKind(nested->kind)) {
                    current = nested;
                    break;
                }
            }
            if (!current)
                return false;
        }
        return isBinaryKind(current->kind);
    }

    bool tokenHasComment(size_t tokenIndex) const {
        const auto& token = normalized.tokens().at(tokenIndex);
        auto hasComment = [](const auto& trivia) {
            return std::ranges::any_of(trivia, [](const NormalizedTrivia& item) {
                return item.kind == NormalizedTriviaKind::Comment;
            });
        };
        return hasComment(token.leading) || hasComment(token.trailing);
    }

    bool childHasComment(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value))
            return tokenHasComment(*token);
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasComment(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasComment(nested);
        });
    }

    bool childHasForcingComment(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto forces = [](const auto& trivia) {
                return std::ranges::any_of(trivia, [](const NormalizedTrivia& item) {
                    return item.kind == NormalizedTriviaKind::Comment &&
                           (item.lineComment || item.endsLine ||
                            item.placement != TriviaPlacement::Inline);
                });
            };
            return forces(normalizedToken.leading) || forces(normalizedToken.trailing);
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasForcingComment(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasForcingComment(nested);
        });
    }

    bool childHasStandaloneConditional(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto hasConditional = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    return trivia.kind == NormalizedTriviaKind::ConditionalDirective &&
                           trivia.placement != TriviaPlacement::Inline;
                });
            };
            return hasConditional(normalizedToken.leading) ||
                   hasConditional(normalizedToken.trailing);
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasStandaloneConditional(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasStandaloneConditional(nested);
        });
    }

    bool childStartsWithInlineVerbatim(const NormalizedChild& child) const {
        auto findReal = [&](const auto& self,
                            const NormalizedChild& nested) -> std::optional<size_t> {
            if (auto token = std::get_if<size_t>(&nested.value)) {
                const auto& normalizedToken = normalized.tokens().at(*token);
                auto parsed = normalizedToken.token;
                if (parsed && !parsed.isMissing() && !normalizedToken.fromMacroExpansion &&
                    !parsed.rawText().empty()) {
                    return *token;
                }
                return std::nullopt;
            }
            if (auto node = childNode(nested)) {
                for (const auto& nodeChild : node->children) {
                    if (auto result = self(self, nodeChild))
                        return result;
                }
                return std::nullopt;
            }
            const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&nested.value);
            for (const auto& listChild : list.children) {
                if (auto result = self(self, listChild))
                    return result;
            }
            return std::nullopt;
        };
        auto token = findReal(findReal, child);
        if (!token)
            return false;
        return std::ranges::any_of(
            normalized.tokens().at(*token).leading, [](const NormalizedTrivia& trivia) {
                return trivia.kind == NormalizedTriviaKind::Verbatim &&
                       trivia.placement == TriviaPlacement::Inline;
            }
        );
    }

    bool childStartsWithMacroUsage(const NormalizedChild& child) const {
        auto findReal = [&](const auto& self,
                            const NormalizedChild& nested) -> std::optional<size_t> {
            if (auto token = std::get_if<size_t>(&nested.value)) {
                const auto& normalizedToken = normalized.tokens().at(*token);
                auto parsed = normalizedToken.token;
                if (parsed && !parsed.isMissing() && !normalizedToken.fromMacroExpansion &&
                    !parsed.rawText().empty()) {
                    return *token;
                }
                return std::nullopt;
            }
            if (auto node = childNode(nested)) {
                for (const auto& nodeChild : node->children) {
                    if (auto result = self(self, nodeChild))
                        return result;
                }
                return std::nullopt;
            }
            const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&nested.value);
            for (const auto& listChild : list.children) {
                if (auto result = self(self, listChild))
                    return result;
            }
            return std::nullopt;
        };
        auto token = findReal(findReal, child);
        if (!token)
            return false;
        return std::ranges::any_of(
            normalized.tokens().at(*token).leading, [](const NormalizedTrivia& trivia) {
                return trivia.kind == NormalizedTriviaKind::MacroUsage;
            }
        );
    }

    bool childStartsWithInlineBlockComment(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& leading = normalized.tokens().at(*token).leading;
            return std::ranges::any_of(leading, [](const NormalizedTrivia& item) {
                return item.kind == NormalizedTriviaKind::Comment && !item.lineComment &&
                       item.placement == TriviaPlacement::Inline;
            });
        }
        if (auto node = childNode(child)) {
            for (const auto& nested : node->children) {
                if (childStartsWithInlineBlockComment(nested))
                    return true;
                if (std::holds_alternative<size_t>(nested.value) || childNode(nested))
                    break;
            }
        }
        return false;
    }

    struct SubtreeSummary {
        size_t flatWidth = 0;
        bool containsMacro = false;
        bool containsInlineConditional = false;
    };

    SubtreeSummary summarize(const NormalizedChild& child) const {
        if (auto found = subtreeSummaries.find(&child); found != subtreeSummaries.end())
            return found->second;
        SubtreeSummary result;
        if (auto index = std::get_if<size_t>(&child.value)) {
            const auto& token = normalized.tokens().at(*index);
            result.flatWidth = token.token.rawText().size();
            auto containsMacro = [](const auto& trivia) {
                return std::ranges::any_of(trivia, [](const NormalizedTrivia& item) {
                    return item.kind == NormalizedTriviaKind::MacroUsage;
                });
            };
            result.containsMacro = token.fromMacroExpansion || containsMacro(token.leading) ||
                                   containsMacro(token.trailing);
            auto containsInlineConditional = [](const auto& trivia) {
                return std::ranges::any_of(trivia, [](const NormalizedTrivia& item) {
                    return item.kind == NormalizedTriviaKind::ConditionalDirective &&
                           item.placement == TriviaPlacement::Inline;
                });
            };
            result.containsInlineConditional = containsInlineConditional(token.leading) ||
                                               containsInlineConditional(token.trailing);
        }
        else {
            auto node = childNode(child);
            const auto& children =
                node ? node->children
                     : std::get<std::unique_ptr<NormalizedList>>(child.value)->children;
            for (const auto& nested : children) {
                auto summary = summarize(nested);
                result.flatWidth += summary.flatWidth + (result.flatWidth ? 1 : 0);
                result.containsMacro = result.containsMacro || summary.containsMacro;
                result.containsInlineConditional = result.containsInlineConditional ||
                                                   summary.containsInlineConditional;
            }
        }
        subtreeSummaries.emplace(&child, result);
        return result;
    }

    size_t flatWidth(const NormalizedChild& child) const { return summarize(child).flatWidth; }

    bool containsRealKind(const NormalizedNode& node, SyntaxKind kind) const {
        if (node.kind == kind) {
            for (const auto& child : node.children) {
                if (flatWidth(child) != 0)
                    return true;
            }
            return false;
        }
        for (const auto& child : node.children) {
            if (auto nested = childNode(child); nested && containsRealKind(*nested, kind))
                return true;
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (const auto& item : (*list)->children) {
                    if (auto nested = childNode(item); nested && containsRealKind(*nested, kind)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void collectTokens(
        const NormalizedChild& child,
        std::vector<const NormalizedToken*>& result
    ) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            result.push_back(&normalized.tokens().at(*token));
            return;
        }
        if (auto node = childNode(child)) {
            for (const auto& nested : node->children)
                collectTokens(nested, result);
            return;
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& nested : list.children)
            collectTokens(nested, result);
    }

    size_t formattedFlatWidth(const NormalizedNode& node) const {
        std::vector<const NormalizedToken*> tokens;
        for (const auto& child : node.children)
            collectTokens(child, tokens);

        size_t width = 0;
        size_t depth = 0;
        const NormalizedToken* previous = nullptr;
        for (const auto* normalizedToken : tokens) {
            auto token = normalizedToken->token;
            if (!token || token.isMissing() || normalizedToken->fromMacroExpansion ||
                token.rawText().empty() || token.kind == TokenKind::EndOfFile) {
                continue;
            }
            bool compact = depth > 0;
            if (previous &&
                (tokenNeedsSeparation(previous->token, token) ||
                 (!compact && shouldInsertWhitespace(
                                  previous->token.kind, token.kind, previous->parentKind,
                                  normalizedToken->parentKind, normalizedToken->inDataType
                              )))) {
                width++;
            }
            width += token.rawText().size();
            if (token.kind == TokenKind::OpenBracket)
                depth++;
            else if (token.kind == TokenKind::CloseBracket && depth)
                depth--;
            previous = normalizedToken;
        }
        return width;
    }

    bool castRequiresOperandBreak(const NormalizedNode& node) const {
        return node.kind == SyntaxKind::CastExpression && config.columnLimit.get() &&
               formattedFlatWidth(node) > config.columnLimit.get();
    }

    size_t maxMulticoncatItemWidth(const NormalizedNode& node) const {
        size_t width = 0;
        for (const auto& child : node.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                if (node.kind == SyntaxKind::ConcatenationExpression) {
                    for (const auto& item : (*list)->children) {
                        if (std::holds_alternative<std::unique_ptr<NormalizedNode>>(item.value))
                            width = std::max(width, flatWidth(item));
                    }
                }
                for (const auto& item : (*list)->children) {
                    if (auto nested = childNode(item))
                        width = std::max(width, maxMulticoncatItemWidth(*nested));
                }
            }
            else if (auto nested = childNode(child)) {
                width = std::max(width, maxMulticoncatItemWidth(*nested));
            }
        }
        return width;
    }

    struct TernaryParts {
        size_t question = SIZE_MAX;
        size_t colon = SIZE_MAX;
        const NormalizedNode* predicate = nullptr;
        const NormalizedNode* left = nullptr;
        const NormalizedNode* right = nullptr;
    };

    TernaryParts ternaryParts(const NormalizedNode& node) const {
        TernaryParts result;
        for (size_t i = 0; i < node.children.size(); i++) {
            if (auto tokenIndex = std::get_if<size_t>(&node.children[i].value)) {
                auto token = normalized.tokens().at(*tokenIndex).token;
                if (token.kind == TokenKind::Question)
                    result.question = i;
                else if (token.kind == TokenKind::Colon)
                    result.colon = i;
                continue;
            }
            auto nested = childNode(node.children[i]);
            if (!nested)
                continue;
            if (result.question == SIZE_MAX)
                result.predicate = nested;
            else if (result.colon == SIZE_MAX)
                result.left = nested;
            else
                result.right = nested;
        }
        return result;
    }

    bool ternaryUsesLeadingColonBreak(const NormalizedNode& node) const {
        auto parts = ternaryParts(node);
        if (!parts.left || !parts.right || parts.left->kind != SyntaxKind::ScopedName ||
            parts.right->kind != SyntaxKind::ScopedName) {
            return false;
        }
        return std::ranges::none_of(node.children, [&](const NormalizedChild& child) {
            return childHasForcingComment(child) || childContainsMacro(child);
        });
    }

    bool ternaryUsesTable(const NormalizedNode& node) const {
        auto first = ternaryParts(node);
        if (first.question == SIZE_MAX || first.colon == SIZE_MAX || !first.predicate ||
            !first.left || !first.right) {
            return false;
        }
        size_t segments = 1;
        const NormalizedNode* right = first.right;
        while (right && right->kind == SyntaxKind::ConditionalExpression) {
            auto nested = ternaryParts(*right);
            if (nested.question == SIZE_MAX || nested.colon == SIZE_MAX || !nested.right)
                break;
            segments++;
            right = nested.right;
        }
        size_t threshold = config.columnLimit.get() ? config.columnLimit.get() * 4 / 5 : SIZE_MAX;
        bool singleFitsValueColumn =
            !config.columnLimit.get() ||
            formattedFlatWidth(*first.predicate) + 3 +
                    std::max(formattedFlatWidth(*first.left), formattedFlatWidth(*first.right)) <=
                config.columnLimit.get() - config.indentWidth.get() * 2;
        return formattedFlatWidth(node) > threshold &&
               (first.left->kind == SyntaxKind::ConditionalExpression || segments > 1 ||
                (formattedFlatWidth(*first.predicate) < config.columnLimit.get() * 3 / 5 &&
                 singleFitsValueColumn));
    }

    std::optional<size_t> firstTokenIndex(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value))
            return *token;
        if (auto node = childNode(child)) {
            for (const auto& nested : node->children) {
                if (auto result = firstTokenIndex(nested))
                    return result;
            }
            return std::nullopt;
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& nested : list.children) {
            if (auto result = firstTokenIndex(nested))
                return result;
        }
        return std::nullopt;
    }

    std::optional<size_t> firstTokenIndex(
        const NormalizedNode& node,
        size_t begin,
        size_t end
    ) const {
        for (size_t i = begin; i < end; i++) {
            if (auto result = firstTokenIndex(node.children[i]))
                return result;
        }
        return std::nullopt;
    }

    void moveLeadingCommentsToCurrentLine(std::optional<size_t> tokenIndex) {
        if (!tokenIndex)
            return;
        bool moved = false;
        for (const auto& trivia : normalized.tokens().at(*tokenIndex).leading) {
            if (trivia.kind != NormalizedTriviaKind::Comment)
                continue;
            auto trailing = trivia;
            trailing.placement = TriviaPlacement::Trailing;
            emitTrivia(trailing, true);
            moved = true;
        }
        if (moved)
            skipLeadingCommentsToken = *tokenIndex;
    }

    void lowerChildren(const NormalizedNode& node, size_t begin, size_t end) {
        for (size_t i = begin; i < end; i++)
            lowerChild(node.children[i], node.kind);
    }

    bool lowerTernary(const NormalizedNode& node) {
        auto first = ternaryParts(node);
        if (first.question == SIZE_MAX || first.colon == SIZE_MAX || !first.predicate ||
            !first.left || !first.right) {
            for (const auto& child : node.children)
                lowerChild(child, node.kind);
            return false;
        }

        if (std::ranges::any_of(node.children, [&](const NormalizedChild& child) {
                return childContainsMacro(child);
            })) {
            bool savedManualBreaks = manualExpressionBreaks;
            bool savedForceInlineLists = forceInlineLists;
            manualExpressionBreaks = true;
            forceInlineLists = true;
            for (const auto& child : node.children)
                lowerChild(child, node.kind);
            manualExpressionBreaks = savedManualBreaks;
            forceInlineLists = savedForceInlineLists;
            return false;
        }

        std::vector<std::pair<const NormalizedNode*, TernaryParts>> segments;
        const NormalizedNode* current = &node;
        TernaryParts parts = first;
        while (true) {
            segments.emplace_back(current, parts);
            if (!parts.right || parts.right->kind != SyntaxKind::ConditionalExpression)
                break;
            current = parts.right;
            parts = ternaryParts(*current);
            if (parts.question == SIZE_MAX || parts.colon == SIZE_MAX || !parts.predicate ||
                !parts.left || !parts.right) {
                break;
            }
        }

        size_t predicateWidth = formattedFlatWidth(*first.predicate);
        size_t threshold = config.columnLimit.get() ? config.columnLimit.get() * 7 / 10 : SIZE_MAX;
        bool table = forceTernaryBranches || ternaryUsesTable(node);
        if (!table) {
            size_t leftWidth = formattedFlatWidth(*first.left);
            size_t rightWidth = formattedFlatWidth(*first.right);
            int savedPriority = ternaryBreakPriority;
            if (predicateWidth > threshold && leftWidth + rightWidth < threshold / 2)
                ternaryBreakPriority = 50;
            const NormalizedNode* savedTrueBranch = ternaryTrueBranch;
            const NormalizedNode* savedFalseBranch = ternaryFalseBranch;
            ternaryTrueBranch = first.left;
            ternaryFalseBranch = first.right;
            for (const auto& child : node.children)
                lowerChild(child, node.kind);
            ternaryTrueBranch = savedTrueBranch;
            ternaryFalseBranch = savedFalseBranch;
            ternaryBreakPriority = savedPriority;
            return false;
        }

        if (forceTernaryBranches || first.left->kind == SyntaxKind::ConditionalExpression) {
            bool savedManualBreaks = manualExpressionBreaks;
            manualExpressionBreaks = true;
            lowerChildren(node, 0, first.question);
            size_t branchesBegin = mark();
            hardLine(1, true);
            lowerChild(node.children[first.question], node.kind);
            moveLeadingCommentsToCurrentLine(
                firstTokenIndex(node, first.question + 1, first.colon)
            );
            bool savedForceTernaryBranches = forceTernaryBranches;
            forceTernaryBranches = true;
            lowerChildren(node, first.question + 1, first.colon);
            forceTernaryBranches = savedForceTernaryBranches;
            hardLine(1, true);
            lowerChild(node.children[first.colon], node.kind);
            moveLeadingCommentsToCurrentLine(
                firstTokenIndex(node, first.colon + 1, node.children.size())
            );
            lowerChildren(node, first.colon + 1, node.children.size());
            auto branches = capture(branchesBegin);
            append(builder.indent(static_cast<int>(config.indentWidth.get()), branches));
            manualExpressionBreaks = savedManualBreaks;
            return true;
        }

        bool hasComment = std::ranges::any_of(node.children, [&](const NormalizedChild& child) {
            return childHasForcingComment(child);
        });
        if (segments.size() == 1 && !hasComment && ternaryUsesLeadingColonBreak(node)) {
            bool savedManualBreaks = manualExpressionBreaks;
            const NormalizedNode* savedTrueBranch = ternaryTrueBranch;
            const NormalizedNode* savedFalseBranch = ternaryFalseBranch;
            manualExpressionBreaks = true;
            ternaryTrueBranch = first.left;
            ternaryFalseBranch = first.right;
            size_t ternaryBegin = mark();
            lowerChildren(node, 0, first.question);
            append(builder.indent(
                static_cast<int>(config.indentWidth.get()), builder.softLine(ternaryBreakPriority)
            ));
            spacingProvided = true;
            size_t branchesBegin = mark();
            lowerChild(node.children[first.question], node.kind);
            lowerChildren(node, first.question + 1, first.colon);
            append(builder.softLine(0));
            spacingProvided = true;
            lowerChild(node.children[first.colon], node.kind);
            lowerChildren(node, first.colon + 1, node.children.size());
            append(builder.relativeAnchor(0, capture(branchesBegin)));
            append(builder.relativeAnchor(0, capture(ternaryBegin)));
            ternaryTrueBranch = savedTrueBranch;
            ternaryFalseBranch = savedFalseBranch;
            manualExpressionBreaks = savedManualBreaks;
            return true;
        }
        if (segments.size() == 1 && !hasComment) {
            bool savedManualBreaks = manualExpressionBreaks;
            const NormalizedNode* savedTrueBranch = ternaryTrueBranch;
            const NormalizedNode* savedFalseBranch = ternaryFalseBranch;
            manualExpressionBreaks = true;
            ternaryTrueBranch = first.left;
            ternaryFalseBranch = first.right;
            GroupId group = builder.createConsistentGroup();
            size_t ternaryBegin = mark();
            lowerChildren(node, 0, first.question);
            append(builder.indent(
                static_cast<int>(config.indentWidth.get()),
                builder.softLine(ternaryBreakPriority, " ", group)
            ));
            spacingProvided = true;
            lowerChild(node.children[first.question], node.kind);
            lowerChildren(node, first.question + 1, first.colon);
            append(builder.indent(
                static_cast<int>(config.indentWidth.get()),
                builder.softLine(ternaryBreakPriority, " ", group)
            ));
            spacingProvided = true;
            lowerChild(node.children[first.colon], node.kind);
            lowerChildren(node, first.colon + 1, node.children.size());
            append(builder.relativeAnchor(0, capture(ternaryBegin)));
            ternaryTrueBranch = savedTrueBranch;
            ternaryFalseBranch = savedFalseBranch;
            manualExpressionBreaks = savedManualBreaks;
            return true;
        }

        size_t maxPredicate = 0;
        for (const auto& [segment, segmentParts] : segments)
            maxPredicate = std::max(maxPredicate, formattedFlatWidth(*segmentParts.predicate));
        bool alignQuestions = !config.columnLimit.get() ||
                              maxPredicate + config.indentWidth.get() * 2 + 2 <=
                                  config.columnLimit.get();
        bool savedManualBreaks = manualExpressionBreaks;
        manualExpressionBreaks = true;
        for (size_t segmentIndex = 0; segmentIndex < segments.size(); segmentIndex++) {
            const auto& [segment, segmentParts] = segments[segmentIndex];
            lowerChildren(*segment, 0, segmentParts.question);
            size_t width = formattedFlatWidth(*segmentParts.predicate);
            append(builder.text(std::string(alignQuestions ? maxPredicate - width + 1 : 1, ' ')));
            spacingProvided = true;
            lowerChild(segment->children[segmentParts.question], segment->kind);
            moveLeadingCommentsToCurrentLine(
                firstTokenIndex(*segment, segmentParts.question + 1, segmentParts.colon)
            );
            size_t valueBegin = mark();
            bool savedTableValue = inTernaryTableValue;
            inTernaryTableValue = true;
            lowerChildren(*segment, segmentParts.question + 1, segmentParts.colon);
            inTernaryTableValue = savedTableValue;
            auto value = capture(valueBegin);
            append(hasComment ? value : builder.relativeAnchor(1, value));
            lowerChild(segment->children[segmentParts.colon], segment->kind);
            if (segmentIndex + 1 < segments.size()) {
                const auto& [nextSegment, nextParts] = segments[segmentIndex + 1];
                moveLeadingCommentsToCurrentLine(
                    firstTokenIndex(*nextSegment, 0, nextParts.question)
                );
            }
            else {
                moveLeadingCommentsToCurrentLine(
                    firstTokenIndex(*segment, segmentParts.colon + 1, segment->children.size())
                );
            }
            hardLine(1, true);
        }
        pendingTokenIndent = maxPredicate + 3;
        const auto& [lastSegment, lastParts] = segments.back();
        lowerChildren(*lastSegment, lastParts.colon + 1, lastSegment->children.size());
        manualExpressionBreaks = savedManualBreaks;
        return true;
    }

    size_t listItemCount(const NormalizedList& list) const {
        return std::ranges::count_if(list.children, [](const NormalizedChild& child) {
            return std::holds_alternative<std::unique_ptr<NormalizedNode>>(child.value);
        });
    }

    std::optional<TokenKind> firstTokenKind(const NormalizedNode& node) const {
        for (const auto& child : node.children) {
            if (auto token = std::get_if<size_t>(&child.value)) {
                auto parsed = normalized.tokens().at(*token).token;
                if (parsed && !parsed.isMissing())
                    return parsed.kind;
            }
            else if (auto nested = childNode(child)) {
                if (auto kind = firstTokenKind(*nested))
                    return kind;
            }
            else {
                const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
                for (const auto& listChild : list.children) {
                    if (auto nested = childNode(listChild)) {
                        if (auto kind = firstTokenKind(*nested))
                            return kind;
                    }
                }
            }
        }
        return std::nullopt;
    }

    bool containsTokenKind(const NormalizedNode& node, TokenKind kind) const {
        for (const auto& child : node.children) {
            if (auto token = std::get_if<size_t>(&child.value)) {
                if (normalized.tokens().at(*token).token.kind == kind)
                    return true;
            }
            else if (auto nested = childNode(child)) {
                if (containsTokenKind(*nested, kind))
                    return true;
            }
        }
        return false;
    }

    bool childHasEscapedIdentifier(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            auto raw = normalized.tokens().at(*token).token.rawText();
            return !raw.empty() && raw.front() == '\\';
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasEscapedIdentifier(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasEscapedIdentifier(nested);
        });
    }

    size_t conditionalDepthAfter(const NormalizedChild& child, size_t depth) const {
        auto applyTrivia = [&](const auto& triviaList) {
            for (const auto& trivia : triviaList) {
                if (trivia.placement == TriviaPlacement::Inline)
                    continue;
                if (trivia.conditionalDepthChange < 0) {
                    depth -= std::min(depth, static_cast<size_t>(-trivia.conditionalDepthChange));
                }
                else {
                    depth += static_cast<size_t>(trivia.conditionalDepthChange);
                }
                if (trivia.kind != NormalizedTriviaKind::ConditionalDirective || !trivia.syntax) {
                    continue;
                }
                auto kind = trivia.syntax->kind;
                if (kind == SyntaxKind::IfDefDirective || kind == SyntaxKind::IfNDefDirective) {
                    depth++;
                }
                else if (kind == SyntaxKind::EndIfDirective && depth > 0) {
                    depth--;
                }
            }
        };
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            applyTrivia(normalizedToken.leading);
            applyTrivia(normalizedToken.trailing);
            return depth;
        }
        if (auto node = childNode(child)) {
            applyTrivia(node->leading);
            for (const auto& nested : node->children)
                depth = conditionalDepthAfter(nested, depth);
            return depth;
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& nested : list.children)
            depth = conditionalDepthAfter(nested, depth);
        return depth;
    }

    bool childIsMacroOnly(const NormalizedChild& child) const {
        bool hasMacro = false;
        bool hasRealToken = false;
        auto visit = [&](const auto& self, const NormalizedChild& nested) -> void {
            if (auto token = std::get_if<size_t>(&nested.value)) {
                const auto& normalizedToken = normalized.tokens().at(*token);
                auto classifyTrivia = [&](const auto& triviaList) {
                    for (const auto& trivia : triviaList) {
                        hasMacro = hasMacro || trivia.kind == NormalizedTriviaKind::MacroUsage;
                        hasRealToken = hasRealToken ||
                                       (trivia.kind == NormalizedTriviaKind::Verbatim &&
                                        !trivia.text.empty());
                    }
                };
                classifyTrivia(normalizedToken.leading);
                classifyTrivia(normalizedToken.trailing);
                auto parsed = normalizedToken.token;
                if (parsed && !parsed.isMissing() && !normalizedToken.fromMacroExpansion &&
                    !parsed.rawText().empty()) {
                    hasRealToken = true;
                }
                return;
            }
            if (auto node = childNode(nested)) {
                for (const auto& nodeChild : node->children)
                    self(self, nodeChild);
                return;
            }
            const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&nested.value);
            for (const auto& listChild : list.children)
                self(self, listChild);
        };
        visit(visit, child);
        return hasMacro && !hasRealToken;
    }

    bool childMacroJoinsFollowing(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto joins = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    return trivia.kind == NormalizedTriviaKind::MacroUsage &&
                           trivia.joinsFollowingToken;
                });
            };
            return joins(normalizedToken.leading) || joins(normalizedToken.trailing);
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childMacroJoinsFollowing(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childMacroJoinsFollowing(nested);
        });
    }

    bool childContainsMacro(const NormalizedChild& child) const {
        return summarize(child).containsMacro;
    }

    bool childHasVerticalMacro(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto forcesVertical = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    if (trivia.kind != NormalizedTriviaKind::MacroUsage)
                        return false;
                    if (std::ranges::any_of(trivia.text, slang::isNewline))
                        return true;
                    return !trivia.syntax || trivia.syntax->kind != SyntaxKind::MacroUsage ||
                           !trivia.syntax->as<MacroUsageSyntax>().args;
                });
            };
            return normalizedToken.fromMacroExpansion || forcesVertical(normalizedToken.leading) ||
                   forcesVertical(normalizedToken.trailing);
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasVerticalMacro(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasVerticalMacro(nested);
        });
    }

    bool childHasMacroPlaceholder(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto hasMacro = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    return trivia.kind == NormalizedTriviaKind::MacroUsage;
                });
            };
            bool carriesMacro = hasMacro(normalizedToken.leading) ||
                                hasMacro(normalizedToken.trailing);
            if (!carriesMacro)
                return false;
            if (!normalizedToken.token || normalizedToken.token.isMissing() ||
                normalizedToken.token.rawText().empty()) {
                return !normalizedToken.fromMacroExpansion;
            }
            auto kind = normalizedToken.token.kind;
            bool expandedOperand = kind == TokenKind::Identifier ||
                                   kind == TokenKind::SystemIdentifier ||
                                   kind == TokenKind::IntegerLiteral ||
                                   kind == TokenKind::IntegerBase ||
                                   kind == TokenKind::StringLiteral;
            return !normalizedToken.fromMacroExpansion && !expandedOperand;
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasMacroPlaceholder(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasMacroPlaceholder(nested);
        });
    }

    size_t childMacroCount(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto count = [](const auto& triviaList) {
                return std::ranges::count_if(triviaList, [](const NormalizedTrivia& trivia) {
                    return trivia.kind == NormalizedTriviaKind::MacroUsage;
                });
            };
            return count(normalizedToken.leading) + count(normalizedToken.trailing);
        }
        if (auto node = childNode(child)) {
            size_t count = 0;
            for (const auto& nested : node->children)
                count += childMacroCount(nested);
            return count;
        }
        size_t count = 0;
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& nested : list.children)
            count += childMacroCount(nested);
        return count;
    }

    std::optional<bool> firstRealTokenStartsLine(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            const auto& normalizedToken = normalized.tokens().at(*token);
            auto parsed = normalizedToken.token;
            if (parsed && !parsed.isMissing() && !normalizedToken.fromMacroExpansion &&
                !parsed.rawText().empty()) {
                return normalizedToken.lineBreakBefore;
            }
            return std::nullopt;
        }
        if (auto node = childNode(child)) {
            for (const auto& nested : node->children) {
                if (auto result = firstRealTokenStartsLine(nested))
                    return result;
            }
            return std::nullopt;
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        for (const auto& nested : list.children) {
            if (auto result = firstRealTokenStartsLine(nested))
                return result;
        }
        return std::nullopt;
    }

    bool childHasSemicolon(const NormalizedChild& child) const {
        if (auto token = std::get_if<size_t>(&child.value)) {
            auto parsed = normalized.tokens().at(*token).token;
            return parsed && !parsed.isMissing() && parsed.kind == TokenKind::Semicolon;
        }
        if (auto node = childNode(child)) {
            return std::ranges::any_of(node->children, [&](const auto& nested) {
                return childHasSemicolon(nested);
            });
        }
        const auto& list = **std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
        return std::ranges::any_of(list.children, [&](const auto& nested) {
            return childHasSemicolon(nested);
        });
    }

    void lowerIndentedChild(const NormalizedChild& child) {
        lineStart = true;
        spacingProvided = false;
        pendingAssignmentBreak = false;
        pendingRecoveredAssignmentBreak = false;
        size_t begin = mark();
        lowerChild(child);
        auto contents = capture(begin);
        append(builder.indent(
            static_cast<int>(config.indentWidth.get()),
            builder.concat({builder.hardLine(), contents})
        ));
        lineStart = false;
    }

    void lowerElseClause(const NormalizedNode& node, bool previousWasBlock) {
        if (!previousWasBlock)
            hardLine();

        for (size_t i = 0; i < node.children.size(); i++) {
            auto child = childNode(node.children[i]);
            if (!child || !StatementSyntax::isKind(child->kind)) {
                lowerChild(node.children[i]);
                continue;
            }

            if (isBlock(*child) || child->kind == SyntaxKind::ConditionalStatement)
                lowerChild(node.children[i]);
            else
                lowerIndentedChild(node.children[i]);
        }
    }

    void lowerConditional(const NormalizedNode& node) {
        bool sawStatement = false;
        bool statementWasBlock = false;
        bool wrapBlock = false;
        bool macroJoinsStatement = false;
        for (const auto& child : node.children) {
            auto nested = childNode(child);
            if (!nested || StatementSyntax::isKind(nested->kind))
                continue;
            if (nested->kind == SyntaxKind::ConditionalPredicate && config.columnLimit.get() &&
                formattedFlatWidth(*nested) > config.columnLimit.get() * 3 / 4) {
                wrapBlock = true;
            }
        }
        for (const auto& child : node.children) {
            auto nested = childNode(child);
            if (!nested) {
                lowerChild(child);
                macroJoinsStatement = childMacroJoinsFollowing(child);
                continue;
            }

            if (nested->kind == SyntaxKind::ElseClause) {
                lowerElseClause(*nested, statementWasBlock);
                continue;
            }
            if (!sawStatement && StatementSyntax::isKind(nested->kind)) {
                sawStatement = true;
                statementWasBlock = isBlock(*nested);
                if (statementWasBlock && wrapBlock) {
                    hardLine();
                    lowerChild(child);
                }
                else if (statementWasBlock)
                    lowerChild(child);
                else if (macroJoinsStatement)
                    lowerChild(child);
                else
                    lowerIndentedChild(child);
                macroJoinsStatement = false;
                continue;
            }
            lowerChild(child);
            macroJoinsStatement = childMacroJoinsFollowing(child);
        }
    }

    void lowerProceduralBlock(const NormalizedNode& node) {
        for (const auto& child : node.children) {
            auto nested = childNode(child);
            if (nested && nested->kind == SyntaxKind::ConditionalStatement)
                lowerIndentedChild(child);
            else
                lowerChild(child);
        }
    }

    void lowerTimingControlStatement(const NormalizedNode& node) {
        for (const auto& child : node.children) {
            auto nested = childNode(child);
            if (nested && nested->kind == SyntaxKind::ConditionalStatement)
                lowerIndentedChild(child);
            else
                lowerChild(child);
        }
    }

    void lowerControlledStatement(const NormalizedNode& node) {
        bool macroJoinsStatement = false;
        for (const auto& child : node.children) {
            auto nested = childNode(child);
            if (nested && StatementSyntax::isKind(nested->kind) && !isBlock(*nested) &&
                !macroJoinsStatement) {
                auto savedAlignmentGroup = currentAlignmentGroup;
                currentAlignmentGroup = builder.createAlignmentGroup();
                lowerIndentedChild(child);
                currentAlignmentGroup = savedAlignmentGroup;
            }
            else {
                lowerChild(child);
            }
            macroJoinsStatement = childMacroJoinsFollowing(child);
        }
    }

    void lowerAssertionDeclaration(const NormalizedNode& node, TokenKind endKeyword) {
        size_t headerEnd = node.children.size();
        size_t declarationEnd = node.children.size();
        for (size_t i = 0; i < node.children.size(); i++) {
            auto tokenIndex = std::get_if<size_t>(&node.children[i].value);
            if (!tokenIndex)
                continue;
            auto token = normalized.tokens().at(*tokenIndex).token;
            if (headerEnd == node.children.size() && token.kind == TokenKind::Semicolon)
                headerEnd = i;
            if (token.kind == endKeyword) {
                declarationEnd = i;
                break;
            }
        }
        if (headerEnd == node.children.size() || declarationEnd == node.children.size()) {
            for (const auto& child : node.children)
                lowerChild(child);
            return;
        }

        for (size_t i = 0; i <= headerEnd; i++)
            lowerChild(node.children[i]);
        size_t bodyBegin = mark();
        lineStart = true;
        spacingProvided = false;
        for (size_t i = headerEnd + 1; i < declarationEnd; i++)
            if (std::holds_alternative<std::unique_ptr<NormalizedList>>(node.children[i].value)) {
                lowerChild(node.children[i]);
                hardLine();
            }
            else {
                lowerChild(node.children[i]);
            }
        auto body = capture(bodyBegin);
        append(builder.indent(
            static_cast<int>(config.indentWidth.get()), builder.concat({builder.hardLine(), body})
        ));
        hardLine();
        for (size_t i = declarationEnd; i < node.children.size(); i++)
            lowerChild(node.children[i]);
    }

    void lowerParameterValueAssignment(const NormalizedNode& node) {
        for (const auto& child : node.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                if (forceParameterListVertical || listItemCount(**list) > 1) {
                    auto savedAlignmentGroup = currentAlignmentGroup;
                    currentAlignmentGroup = builder.createAlignmentGroup();
                    lowerVerticalList(**list, false);
                    currentAlignmentGroup = savedAlignmentGroup;
                }
                else {
                    lowerList(**list);
                }
            }
            else
                lowerChild(child);
        }
    }

    void lowerAssignment(const NormalizedNode& node) {
        std::optional<size_t> trailingCommentAnchor;
        int trailingCommentAnchorOffset = 0;
        bool commentUseAnchor = true;
        if (config.columnLimit.get()) {
            for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
                if (auto rhs = childNode(*it); rhs && isExpressionKind(rhs->kind)) {
                    commentUseAnchor = formattedFlatWidth(*rhs) <= config.columnLimit.get() * 3 / 5;
                    break;
                }
            }
        }
        for (const auto& child : node.children) {
            if (!pendingAssignmentBreak) {
                bool assignmentOperator = false;
                if (auto tokenIndex = std::get_if<size_t>(&child.value)) {
                    const auto& token = normalized.tokens().at(*tokenIndex);
                    assignmentOperator = isAssignmentKind(token.parentKind);
                    if (isAssignmentKind(token.parentKind) &&
                        std::ranges::any_of(token.trailing, [](const NormalizedTrivia& trivia) {
                            return trivia.lineComment;
                        })) {
                        trailingCommentAnchor = mark();
                        trailingCommentAnchorOffset =
                            static_cast<int>(token.token.rawText().size() + 2);
                    }
                }
                bool savedEmittingAssignmentOperator = emittingAssignmentOperator;
                bool savedAssignmentOperatorCommentUseAnchor = assignmentOperatorCommentUseAnchor;
                emittingAssignmentOperator = assignmentOperator;
                assignmentOperatorCommentUseAnchor = commentUseAnchor;
                lowerChild(child, node.kind);
                emittingAssignmentOperator = savedEmittingAssignmentOperator;
                assignmentOperatorCommentUseAnchor = savedAssignmentOperatorCommentUseAnchor;
                continue;
            }

            if (lastWasMacro) {
                pendingAssignmentBreak = false;
                pendingRecoveredAssignmentBreak = false;
                lowerChild(child, node.kind);
                continue;
            }

            if (childHasStandaloneConditional(child)) {
                pendingAssignmentBreak = false;
                pendingRecoveredAssignmentBreak = false;
                hardLine();
                bool savedConditionalAssignmentRhs = conditionalAssignmentRhs;
                conditionalAssignmentRhs = true;
                lowerChild(child, node.kind);
                conditionalAssignmentRhs = savedConditionalAssignmentRhs;
                continue;
            }

            if (std::holds_alternative<std::unique_ptr<NormalizedList>>(child.value)) {
                lowerChild(child, node.kind);
                continue;
            }

            pendingAssignmentBreak = false;
            pendingRecoveredAssignmentBreak = false;
            auto rhsNode = childNode(child);
            bool directInvocation = rhsNode && rhsNode->kind == SyntaxKind::InvocationExpression &&
                                    !containsTokenKind(*rhsNode, TokenKind::Apostrophe);
            bool parameterAssignment = currentMemberKind == SyntaxKind::ParameterDeclaration ||
                                       currentMemberKind ==
                                           SyntaxKind::ParameterDeclarationStatement;
            size_t standaloneLimit = config.columnLimit.get() > config.indentWidth.get() * 2
                                         ? config.columnLimit.get() - config.indentWidth.get() * 2
                                         : 0;
            bool dataDeclarationAssignment = (currentMemberKind == SyntaxKind::DataDeclaration ||
                                              currentMemberKind == SyntaxKind::NetDeclaration) &&
                                             rhsNode && standaloneLimit &&
                                             formattedFlatWidth(*rhsNode) <= standaloneLimit;
            bool declarationAssignment = (parameterAssignment &&
                                          (!rhsNode || !isListHandledExpression(rhsNode->kind))) ||
                                         dataDeclarationAssignment;
            const NormalizedNode* peeledRhs = rhsNode;
            while (peeledRhs && peeledRhs->kind == SyntaxKind::ParenthesizedExpression) {
                const NormalizedNode* nestedExpression = nullptr;
                for (const auto& nested : peeledRhs->children) {
                    if (auto candidate = childNode(nested);
                        candidate && isExpressionKind(candidate->kind)) {
                        nestedExpression = candidate;
                        break;
                    }
                }
                peeledRhs = nestedExpression;
            }
            bool preferAssignmentBreak = (!followsSkippedMember && declarationAssignment) ||
                                         directInvocation ||
                                         (peeledRhs &&
                                          (peeledRhs->kind == SyntaxKind::ConditionalExpression ||
                                           isComparisonKind(peeledRhs->kind)));
            bool compareDataDeclarationBinaryBreaks = !followsSkippedMember &&
                                                      dataDeclarationAssignment &&
                                                      isBinaryKind(rhsNode->kind);
            const NormalizedNode* binaryLeft = nullptr;
            if (rhsNode && isBinaryKind(rhsNode->kind)) {
                for (const auto& nested : rhsNode->children) {
                    if (auto candidate = childNode(nested);
                        candidate && isExpressionKind(candidate->kind)) {
                        binaryLeft = candidate;
                        break;
                    }
                }
            }
            const NormalizedNode* firstOperand = rhsNode;
            while (firstOperand && isBinaryKind(firstOperand->kind)) {
                const NormalizedNode* next = nullptr;
                for (const auto& nested : firstOperand->children) {
                    if (auto candidate = childNode(nested);
                        candidate && isExpressionKind(candidate->kind)) {
                        next = candidate;
                        break;
                    }
                }
                firstOperand = next;
            }
            bool forceOversizedFirstOperand =
                rhsNode && isBinaryKind(rhsNode->kind) && firstOperand &&
                firstOperand->kind != SyntaxKind::ParenthesizedExpression &&
                config.columnLimit.get() &&
                formattedFlatWidth(*firstOperand) > config.columnLimit.get() * 3 / 5;
            bool forceLogicalAssignment =
                currentMemberKind == SyntaxKind::ContinuousAssign && rhsNode && binaryLeft &&
                rhsNode->kind == SyntaxKind::LogicalOrExpression && config.columnLimit.get() &&
                formattedFlatWidth(*binaryLeft) > config.columnLimit.get() * 3 / 4;
            bool forceTernaryTable = false;
            if (rhsNode && rhsNode->kind == SyntaxKind::ConditionalExpression &&
                ternaryUsesTable(*rhsNode)) {
                auto parts = ternaryParts(*rhsNode);
                bool rhsHasForcingComment = childHasForcingComment(child);
                bool questionHasComment = false;
                if (parts.question < rhsNode->children.size()) {
                    if (auto questionToken =
                            std::get_if<size_t>(&rhsNode->children[parts.question].value)) {
                        questionHasComment = tokenHasComment(*questionToken);
                    }
                }
                bool leftHasComment = false;
                for (size_t i = parts.question + 1; i < parts.colon; i++)
                    leftHasComment = leftHasComment || childHasForcingComment(rhsNode->children[i]);
                bool cleanTernaryChain = parts.right &&
                                         parts.right->kind == SyntaxKind::ConditionalExpression &&
                                         !rhsHasForcingComment && !childContainsMacro(child);
                forceTernaryTable = !cleanTernaryChain && !ternaryUsesLeadingColonBreak(*rhsNode) &&
                                    (!rhsHasForcingComment || questionHasComment || leftHasComment);
            }
            bool forcingInvocation = directInvocation && (childHasForcingComment(child) ||
                                                          (config.columnLimit.get() &&
                                                           formattedFlatWidth(*rhsNode) >
                                                               config.columnLimit.get() * 3 / 5));
            bool hugeRhs = rhsNode && config.columnLimit.get() &&
                           formattedFlatWidth(*rhsNode) > config.columnLimit.get() * 3;
            int assignmentPriority = 100;
            if (compareDataDeclarationBinaryBreaks)
                assignmentPriority = expressionBreakPriority + 1;
            else if (preferAssignmentBreak || hugeRhs)
                assignmentPriority = 1;
            else if (rhsNode && isBinaryKind(rhsNode->kind))
                assignmentPriority = SyntaxFacts::getPrecedence(rhsNode->kind) + 1;
            bool hardAssignmentLine = forcingInvocation || forceTernaryTable ||
                                      forceOversizedFirstOperand || forceLogicalAssignment;
            bool keepMulticoncatHeader = parameterAssignment && rhsNode &&
                                         rhsNode->kind ==
                                             SyntaxKind::MultipleConcatenationExpression;
            bool assignmentAlreadyBroken = lineStart;
            DocId assignmentLine = assignmentAlreadyBroken ? builder.empty()
                                   : keepMulticoncatHeader ||
                                           summarize(child).containsInlineConditional
                                       ? builder.text(" ")
                                   : hardAssignmentLine ? builder.hardLine()
                                                        : builder.softLine(assignmentPriority);
            append(builder.indent(static_cast<int>(config.indentWidth.get()), assignmentLine));
            spacingProvided = true;

            size_t rhsBegin = mark();
            const NormalizedNode* savedAssignmentRhs = assignmentRhsRoot;
            DocId savedAssignmentRhsLine = assignmentRhsLine;
            bool savedBinaryContinuationScope = binaryContinuationScope;
            assignmentRhsRoot = rhsNode;
            assignmentRhsLine = assignmentLine;
            binaryContinuationScope = false;
            lowerChild(child);
            assignmentRhsRoot = savedAssignmentRhs;
            assignmentRhsLine = savedAssignmentRhsLine;
            binaryContinuationScope = savedBinaryContinuationScope;
            auto rhs = capture(rhsBegin);
            if (rhsNode && isBinaryKind(rhsNode->kind)) {
                rhs = builder.relativeAnchor(static_cast<int>(config.indentWidth.get()), rhs);
            }
            else if (rhsNode && rhsNode->kind != SyntaxKind::ParenthesizedExpression &&
                     !isListHandledExpression(rhsNode->kind) &&
                     !castRequiresOperandBreak(*rhsNode)) {
                rhs = builder.relativeAnchor(0, rhs);
            }
            if (parameterAssignment && rhsNode &&
                rhsNode->kind == SyntaxKind::MultipleConcatenationExpression) {
                rhs = builder.relativeAnchor(
                    0, rhs, config.indentWidth.get() + maxMulticoncatItemWidth(*rhsNode)
                );
            }
            if (!rhsNode || !isListHandledExpression(rhsNode->kind)) {
                rhs = builder.indent(static_cast<int>(config.indentWidth.get()), rhs);
            }
            append(rhs);
            bool anchorCommentContinuation = !hardAssignmentLine &&
                                             (!rhsNode || !config.columnLimit.get() ||
                                              formattedFlatWidth(*rhsNode) <=
                                                  config.columnLimit.get() * 3 / 5);
            if (trailingCommentAnchor) {
                auto anchored = capture(*trailingCommentAnchor);
                if (anchorCommentContinuation) {
                    append(builder.relativeAnchor(trailingCommentAnchorOffset, anchored));
                }
                else {
                    append(builder.indent(static_cast<int>(config.indentWidth.get()), anchored));
                }
            }
            trailingCommentAnchor.reset();
        }
    }

    void lowerHierarchyInstantiation(const NormalizedNode& node) {
        std::vector<const NormalizedToken*> hierarchyTokens;
        for (const auto& child : node.children)
            collectTokens(child, hierarchyTokens);
        bool macroProvidesType = false;
        for (const auto& child : node.children) {
            if (!std::holds_alternative<size_t>(child.value))
                continue;
            macroProvidesType = childContainsMacro(child);
            break;
        }
        if (macroProvidesType || !containsRealKind(node, SyntaxKind::InstanceName)) {
            bool savedNoNameInstantiation = noNameInstantiation;
            bool savedNoNameCallStyle = noNameCallStyle;
            auto firstReal = std::ranges::find_if(hierarchyTokens, [](const auto* token) {
                return token->token && !token->token.isMissing() && !token->fromMacroExpansion &&
                       !token->token.rawText().empty();
            });
            auto containsRecoveredComma = [](const auto& triviaList) {
                return std::ranges::any_of(triviaList, [](const NormalizedTrivia& trivia) {
                    if (trivia.kind != NormalizedTriviaKind::Verbatim)
                        return false;
                    auto text = std::string_view(trivia.text);
                    while (!text.empty()) {
                        auto lineEnd = findNewline(text);
                        auto line = text.substr(0, lineEnd);
                        while (!line.empty() && slang::isTabOrSpace(line.front()))
                            line.remove_prefix(1);
                        if (line.starts_with(','))
                            return true;
                        if (lineEnd == std::string_view::npos)
                            break;
                        text.remove_prefix(skipNewline(text, lineEnd));
                    }
                    return false;
                });
            };
            bool startsWithRecoveredComma = containsRecoveredComma(node.leading) ||
                                            (firstReal != hierarchyTokens.end() &&
                                             containsRecoveredComma((*firstReal)->leading));
            noNameInstantiation = true;
            noNameCallStyle = node.syntax &&
                              node.syntax->getFirstToken().kind == TokenKind::Identifier &&
                              !startsWithRecoveredComma;
            for (const auto& child : node.children)
                lowerChild(child);
            noNameInstantiation = savedNoNameInstantiation;
            noNameCallStyle = savedNoNameCallStyle;
            return;
        }

        bool hasParameters = false;
        size_t parameterCount = 0;
        size_t parameterWidth = 0;
        bool parameterHasComment = false;
        bool connectionsVertical = false;
        size_t maxConnectionCount = 0;
        bool allConnectionsShorthand = true;
        bool allInstancesEmpty = true;
        size_t maxInstanceNameWidth = 0;
        size_t hierarchyInstanceCount = 0;
        for (const auto& child : node.children) {
            if (auto nested = childNode(child);
                nested && nested->kind == SyntaxKind::ParameterValueAssignment) {
                hasParameters = true;
                parameterWidth = std::max(parameterWidth, formattedFlatWidth(*nested));
                parameterHasComment = parameterHasComment || childHasForcingComment(child);
                for (const auto& parameterChild : nested->children) {
                    if (auto parameterList =
                            std::get_if<std::unique_ptr<NormalizedList>>(&parameterChild.value)) {
                        parameterCount += std::ranges::count_if(
                            (*parameterList)->children, [](const NormalizedChild& item) {
                                return std::holds_alternative<std::unique_ptr<NormalizedNode>>(
                                    item.value
                                );
                            }
                        );
                    }
                }
            }

            auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
            bool instanceList =
                list && std::ranges::any_of((*list)->children, [](const NormalizedChild& nested) {
                    auto node = childNode(nested);
                    return node && node->kind == SyntaxKind::HierarchicalInstance;
                });
            if (instanceList) {
                for (const auto& instance : (*list)->children) {
                    auto instanceNode = childNode(instance);
                    if (!instanceNode)
                        continue;
                    hierarchyInstanceCount++;
                    for (const auto& instanceChild : instanceNode->children) {
                        if (auto name = childNode(instanceChild);
                            name && name->kind == SyntaxKind::InstanceName) {
                            maxInstanceNameWidth =
                                std::max(maxInstanceNameWidth, formattedFlatWidth(*name));
                        }
                        auto connections =
                            std::get_if<std::unique_ptr<NormalizedList>>(&instanceChild.value);
                        if (!connections)
                            continue;
                        size_t count = listItemCount(**connections);
                        maxConnectionCount = std::max(maxConnectionCount, count);
                        allInstancesEmpty = allInstancesEmpty && count == 0;
                        for (const auto& connection : (*connections)->children) {
                            auto connectionNode = childNode(connection);
                            if (!connectionNode)
                                continue;
                            bool implicitNamed =
                                connectionNode->kind == SyntaxKind::NamedPortConnection &&
                                !containsTokenKind(*connectionNode, TokenKind::OpenParenthesis);
                            if (!implicitNamed &&
                                connectionNode->kind != SyntaxKind::WildcardPortConnection) {
                                allConnectionsShorthand = false;
                            }
                        }
                        connectionsVertical = connectionsVertical ||
                                              count > constants::maxInlineInstanceConnections ||
                                              childHasForcingComment(instanceChild) ||
                                              childContainsMacro(instanceChild) ||
                                              childHasEscapedIdentifier(instanceChild);
                    }
                }
            }
        }

        size_t typeWidth = node.syntax ? node.syntax->getFirstToken().rawText().size() : 0;
        bool hierarchyWide = config.columnLimit.get() && hierarchyInstanceCount == 1 &&
                             hasParameters && typeWidth >= 10;
        bool connectionShapeVertical =
            maxConnectionCount > constants::maxInlineInstanceConnections ||
            (hierarchyWide && maxConnectionCount > 1) ||
            (parameterCount > 1 && maxConnectionCount > 1 && !allConnectionsShorthand);
        connectionsVertical = connectionsVertical || connectionShapeVertical;
        bool parametersVertical = parameterCount > 1 || parameterHasComment ||
                                  connectionShapeVertical ||
                                  (hasParameters && config.columnLimit.get() &&
                                   parameterWidth > config.columnLimit.get() * 3 / 5);
        bool savedForceParameters = forceParameterListVertical;
        bool savedForceConnections = forceConnectionListVertical;
        forceParameterListVertical = parametersVertical;
        forceConnectionListVertical = connectionsVertical;
        for (const auto& child : node.children) {
            auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value);
            bool instanceList =
                list && std::ranges::any_of((*list)->children, [](const NormalizedChild& nested) {
                    auto instanceNode = childNode(nested);
                    return instanceNode && instanceNode->kind == SyntaxKind::HierarchicalInstance;
                });
            if (!instanceList) {
                lowerChild(child);
                continue;
            }

            size_t i = 0;
            bool firstInstance = true;
            while (i < (*list)->children.size()) {
                if (!childNode((*list)->children[i])) {
                    lowerChild((*list)->children[i]);
                    i++;
                    continue;
                }
                lineStart = !(firstInstance && parametersVertical && hasParameters);
                spacingProvided = false;
                size_t begin = mark();
                size_t savedInstancePadding = instanceOpenParenPadding;
                size_t instanceNameWidth = 0;
                if (auto instanceNode = childNode((*list)->children[i])) {
                    for (const auto& instanceChild : instanceNode->children) {
                        if (auto name = childNode(instanceChild);
                            name && name->kind == SyntaxKind::InstanceName) {
                            instanceNameWidth = formattedFlatWidth(*name);
                            break;
                        }
                    }
                }
                instanceOpenParenPadding = allInstancesEmpty
                                               ? 0
                                               : maxInstanceNameWidth - instanceNameWidth;
                lowerChild((*list)->children[i]);
                instanceOpenParenPadding = savedInstancePadding;
                i++;
                if (i < (*list)->children.size()) {
                    auto separator = std::get_if<size_t>(&(*list)->children[i].value);
                    if (separator &&
                        normalized.tokens().at(*separator).token.kind == TokenKind::Comma) {
                        lowerChild((*list)->children[i]);
                        i++;
                    }
                }
                auto contents = capture(begin);
                if (firstInstance && parametersVertical && hasParameters) {
                    append(contents);
                }
                else if (!firstInstance && allInstancesEmpty) {
                    append(builder.text(" "));
                    append(contents);
                }
                else {
                    append(builder.indent(2, builder.hardLine()));
                    append(contents);
                }
                lineStart = false;
                firstInstance = false;
            }
        }
        forceParameterListVertical = savedForceParameters;
        forceConnectionListVertical = savedForceConnections;
    }

    void lowerLongImplication(const NormalizedNode& node) {
        size_t opIndex = SIZE_MAX;
        for (size_t i = 0; i < node.children.size(); i++) {
            if (auto tokenIndex = std::get_if<size_t>(&node.children[i].value)) {
                auto token = normalized.tokens().at(*tokenIndex).token;
                if (token && token.kind == TokenKind::OrMinusArrow) {
                    opIndex = i;
                    break;
                }
            }
        }
        if (opIndex == SIZE_MAX) {
            for (const auto& child : node.children)
                lowerChild(child, node.kind);
            return;
        }

        for (size_t i = 0; i < opIndex; i++)
            lowerChild(node.children[i], node.kind);
        bool savedAllowCurrentBinaryBreak = allowCurrentBinaryBreak;
        allowCurrentBinaryBreak = false;
        lowerChild(node.children[opIndex], node.kind);
        allowCurrentBinaryBreak = savedAllowCurrentBinaryBreak;

        size_t rightBegin = mark();
        hardLine();
        for (size_t i = opIndex + 1; i < node.children.size(); i++)
            lowerChild(node.children[i], node.kind);
        append(builder.indent(static_cast<int>(config.indentWidth.get()), capture(rightBegin)));
    }

    void lowerConcurrentAssertion(const NormalizedNode& node) {
        auto findImplication = [&](const auto& self,
                                   const NormalizedNode& current) -> const NormalizedNode* {
            if (current.kind == SyntaxKind::ImplicationPropertyExpr)
                return &current;
            for (const auto& child : current.children) {
                if (auto nested = childNode(child)) {
                    if (auto result = self(self, *nested))
                        return result;
                }
                else if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                    for (const auto& item : (*list)->children) {
                        if (auto nested = childNode(item)) {
                            if (auto result = self(self, *nested))
                                return result;
                        }
                    }
                }
            }
            return nullptr;
        };
        auto implication = findImplication(findImplication, node);
        if (!implication || !config.columnLimit.get() ||
            formattedFlatWidth(node) <= config.columnLimit.get()) {
            for (const auto& child : node.children)
                lowerChild(child);
            return;
        }

        size_t openParen = SIZE_MAX;
        size_t closeParen = SIZE_MAX;
        for (size_t i = 0; i < node.children.size(); i++) {
            auto tokenIndex = std::get_if<size_t>(&node.children[i].value);
            if (!tokenIndex)
                continue;
            auto token = normalized.tokens().at(*tokenIndex).token;
            if (!token)
                continue;
            if (token.kind == TokenKind::OpenParenthesis)
                openParen = i;
            else if (token.kind == TokenKind::CloseParenthesis)
                closeParen = i;
        }
        if (openParen == SIZE_MAX || closeParen == SIZE_MAX || openParen >= closeParen) {
            for (const auto& child : node.children)
                lowerChild(child);
            return;
        }

        lowerChildren(node, 0, openParen + 1);
        size_t bodyBegin = mark();
        hardLine();
        const NormalizedNode* savedWrappedImplication = verticallyWrappedImplication;
        verticallyWrappedImplication = implication;
        lowerChildren(node, openParen + 1, closeParen);
        verticallyWrappedImplication = savedWrappedImplication;
        append(builder.indent(static_cast<int>(config.indentWidth.get()), capture(bodyBegin)));
        hardLine();
        lowerChildren(node, closeParen, node.children.size());
    }

    bool caseItemNeedsClauseBreak(const NormalizedNode& item) const {
        const NormalizedNode* clause = nullptr;
        size_t labelWidth = 0;
        size_t labelCount = 0;
        for (const auto& child : item.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                labelWidth = std::max(labelWidth, flatWidth(child));
                labelCount = std::max(labelCount, listItemCount(**list));
            }
            if (auto node = childNode(child); node && StatementSyntax::isKind(node->kind))
                clause = node;
        }
        if (!clause || isBlock(*clause) || clause->kind == SyntaxKind::EmptyStatement)
            return false;
        size_t available = config.columnLimit.get() > 16 ? config.columnLimit.get() - 16 : 0;
        return labelWidth > constants::maxInlineCaseItemLabelWidth ||
               (labelCount > 1 && labelWidth > constants::maxInlineMultiLabelWidth) || [&]() {
                   if (!config.columnLimit.get())
                       return false;
                   size_t width = 0;
                   for (const auto& child : item.children)
                       width += flatWidth(child) + (width ? 1 : 0);
                   return width > available;
               }();
    }

    void lowerCaseItem(const NormalizedNode& node) {
        bool savedSuppressCaseAlignment = suppressCaseAlignment;
        suppressCaseAlignment = std::ranges::any_of(node.children, [&](const auto& child) {
            auto nested = childNode(child);
            return nested && (nested->kind == SyntaxKind::GenerateBlock ||
                              (StatementSyntax::isKind(nested->kind) &&
                               (isBlock(*nested) ||
                                (caseBreakClauses && nested->kind == SyntaxKind::EmptyStatement))));
        });
        for (const auto& child : node.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                bool splitLabels = listItemCount(**list) > 1 &&
                                   flatWidth(child) > constants::maxInlineMultiLabelWidth;
                if (!splitLabels) {
                    lowerChild(child);
                    continue;
                }
                for (size_t i = 0; i < (*list)->children.size(); i++) {
                    lowerChild((*list)->children[i]);
                    if (i + 1 < (*list)->children.size() &&
                        std::holds_alternative<size_t>((*list)->children[i].value)) {
                        hardLine();
                    }
                }
                continue;
            }

            auto nested = childNode(child);
            if (nested && StatementSyntax::isKind(nested->kind) && !isBlock(*nested) &&
                nested->kind != SyntaxKind::EmptyStatement && caseBreakClauses) {
                auto savedAlignmentGroup = currentAlignmentGroup;
                currentAlignmentGroup = builder.createAlignmentGroup();
                lowerIndentedChild(child);
                currentAlignmentGroup = savedAlignmentGroup;
            }
            else {
                if (nested && StatementSyntax::isKind(nested->kind)) {
                    auto savedAlignmentGroup = currentAlignmentGroup;
                    currentAlignmentGroup = builder.createAlignmentGroup();
                    lowerChild(child);
                    currentAlignmentGroup = savedAlignmentGroup;
                }
                else {
                    lowerChild(child);
                }
            }
        }
        suppressCaseAlignment = savedSuppressCaseAlignment;
    }

    void lowerCaseStatement(const NormalizedNode& node) {
        bool saved = caseBreakClauses;
        caseBreakClauses = false;
        for (const auto& child : node.children) {
            if (auto list = std::get_if<std::unique_ptr<NormalizedList>>(&child.value)) {
                for (const auto& item : (*list)->children) {
                    auto itemNode = childNode(item);
                    if (itemNode && (itemNode->kind == SyntaxKind::StandardCaseItem ||
                                     itemNode->kind == SyntaxKind::DefaultCaseItem)) {
                        caseBreakClauses = caseBreakClauses || caseItemNeedsClauseBreak(*itemNode);
                    }
                }
            }
        }
        for (const auto& child : node.children)
            lowerChild(child);
        caseBreakClauses = saved;
    }

    void lowerVerticalList(const NormalizedList& list, bool rootList) {
        if (list.children.empty()) {
            if (list.style == ListStyle::Body && !rootList) {
                hardLine();
                awaitingClosingTrivia = true;
            }
            return;
        }

        size_t i = 0;
        bool first = true;
        size_t listConditionalDepth = conditionalDepth;
        bool separated = std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
            if (auto token = std::get_if<size_t>(&child.value)) {
                auto parsed = normalized.tokens().at(*token).token;
                return parsed && parsed.kind == TokenKind::Comma;
            }
            return false;
        });
        bool previousMacroRecovery = false;
        bool dedentAfterAssignmentConditional = false;
        bool previousItemSkipped = false;
        while (i < list.children.size()) {
            size_t finalConditionalDepth =
                conditionalDepthAfter(list.children[i], conditionalDepth);
            if (i + 1 < list.children.size() &&
                std::holds_alternative<size_t>(list.children[i + 1].value)) {
                auto separator =
                    normalized.tokens().at(std::get<size_t>(list.children[i + 1].value)).token;
                if (separator && separator.kind == TokenKind::Comma) {
                    finalConditionalDepth =
                        conditionalDepthAfter(list.children[i + 1], finalConditionalDepth);
                }
            }
            bool closesAssignmentConditional = finalConditionalDepth < conditionalDepth &&
                                               !conditionalAssignmentStack.empty() &&
                                               conditionalAssignmentStack.back();
            bool groupedMacro = previousMacroRecovery || closesAssignmentConditional;
            bool currentMacroOnly = childIsMacroOnly(list.children[i]);
            bool macroJoinsFollowing = childMacroJoinsFollowing(list.children[i]);
            bool containsMacro = childContainsMacro(list.children[i]);
            bool hasSemicolon = childHasSemicolon(list.children[i]);
            auto itemNode = childNode(list.children[i]);
            bool noNameHierarchy = itemNode &&
                                   itemNode->kind == SyntaxKind::HierarchyInstantiation &&
                                   !containsRealKind(*itemNode, SyntaxKind::InstanceName);
            bool groupedPort = itemNode && itemNode->kind == SyntaxKind::ImplicitAnsiPort &&
                               containsKind(*itemNode, SyntaxKind::ImplicitType) &&
                               !containsTokenKind(*itemNode, TokenKind::InputKeyword) &&
                               !containsTokenKind(*itemNode, TokenKind::OutputKeyword) &&
                               !containsTokenKind(*itemNode, TokenKind::InOutKeyword) &&
                               !containsMacro && !first;
            bool attributedPort = itemNode &&
                                  (itemNode->kind == SyntaxKind::ImplicitAnsiPort ||
                                   itemNode->kind == SyntaxKind::ExplicitAnsiPort) &&
                                  containsKind(*itemNode, SyntaxKind::AttributeInstance);
            bool incompatibleAlignment = attributedPort || groupedMacro ||
                                         childStartsWithMacroUsage(list.children[i]) ||
                                         childStartsWithInlineVerbatim(list.children[i]) ||
                                         (classDepth && itemNode &&
                                          itemNode->kind == SyntaxKind::ClassPropertyDeclaration);
            lineStart = !groupedMacro;
            if (lineStart)
                inlineBlockCommentOnLine = false;
            if (!groupedMacro)
                spacingProvided = false;
            if (!groupedMacro)
                pendingAssignmentBreak = false;
            if (!groupedMacro)
                pendingRecoveredAssignmentBreak = false;
            size_t itemBegin = mark();
            auto oldMemberKind = currentMemberKind;
            bool oldMemberContainsMacro = currentMemberContainsMacro;
            if (auto node = std::get_if<std::unique_ptr<NormalizedNode>>(&list.children[i].value))
                currentMemberKind = (*node)->kind;
            currentMemberContainsMacro = containsMacro;

            size_t savedItemFinalDepth = itemFinalConditionalDepth;
            bool savedCurrentItemMacroOnly = currentItemMacroOnly;
            bool savedDedentListDirective = dedentListConditionalDirective;
            bool savedFollowsSkippedMember = followsSkippedMember;
            bool savedSuppressAlignment = suppressAlignment;
            bool savedSuppressEscapedAlignment = suppressEscapedAlignment;
            bool savedPortDirectionAnchorEmitted = portDirectionAnchorEmitted;
            bool savedPackedDimensionAnchorEmitted = packedDimensionAnchorEmitted;
            uint32_t savedNextDeclaratorAlignmentColumn = nextDeclaratorAlignmentColumn;
            nextDeclaratorAlignmentColumn = 1;
            itemFinalConditionalDepth = dedentAfterAssignmentConditional ? listConditionalDepth
                                                                         : finalConditionalDepth;
            currentItemMacroOnly = currentMacroOnly;
            dedentListConditionalDirective = separated;
            followsSkippedMember = previousItemSkipped;
            suppressAlignment = savedSuppressAlignment || groupedPort || incompatibleAlignment;
            suppressEscapedAlignment = childHasEscapedIdentifier(list.children[i]);
            portDirectionAnchorEmitted = false;
            packedDimensionAnchorEmitted = false;
            lowerChild(list.children[i]);
            packedDimensionAnchorEmitted = savedPackedDimensionAnchorEmitted;
            portDirectionAnchorEmitted = savedPortDirectionAnchorEmitted;
            suppressEscapedAlignment = savedSuppressEscapedAlignment;
            suppressAlignment = savedSuppressAlignment;
            nextDeclaratorAlignmentColumn = savedNextDeclaratorAlignmentColumn;
            followsSkippedMember = savedFollowsSkippedMember;
            i++;
            bool hadSeparator = false;
            if (i < list.children.size() &&
                std::holds_alternative<size_t>(list.children[i].value)) {
                auto tokenIndex = std::get<size_t>(list.children[i].value);
                auto token = normalized.tokens().at(tokenIndex).token;
                if (token && token.kind == TokenKind::Comma) {
                    lowerChild(list.children[i]);
                    i++;
                    hadSeparator = true;
                }
            }
            dedentListConditionalDirective = savedDedentListDirective;
            currentItemMacroOnly = savedCurrentItemMacroOnly;
            itemFinalConditionalDepth = savedItemFinalDepth;
            auto item = capture(itemBegin);
            item = builder.member(item, currentMemberKind);
            currentMemberKind = oldMemberKind;
            currentMemberContainsMacro = oldMemberContainsMacro;
            if (!item)
                continue;

            size_t relativeConditionalDepth = conditionalDepth > listConditionalDepth
                                                  ? conditionalDepth - listConditionalDepth
                                                  : 0;
            int itemIndent = 0;
            if (!rootList) {
                if (separated)
                    itemIndent = static_cast<int>(
                        std::max<size_t>(relativeConditionalDepth, 1) * config.indentWidth.get()
                    );
                else
                    itemIndent =
                        static_cast<int>((relativeConditionalDepth + 1) * config.indentWidth.get());
            }
            else {
                itemIndent = static_cast<int>(relativeConditionalDepth * config.indentWidth.get());
            }
            if (dedentAfterAssignmentConditional || closesAssignmentConditional)
                itemIndent = 0;

            if (groupedPort) {
                append(builder.text(" "));
                append(item);
                lineStart = false;
            }
            else if (groupedMacro) {
                append(builder.indent(itemIndent, item));
                lineStart = false;
            }
            else if (rootList && first) {
                append(builder.indent(itemIndent, item));
                lineStart = false;
            }
            else if (rootList) {
                append(builder.indent(itemIndent, builder.concat({builder.hardLine(), item})));
                lineStart = false;
            }
            else {
                auto lineAndItem = builder.concat({builder.hardLine(), item});
                append(builder.indent(itemIndent, lineAndItem));
                lineStart = false;
            }
            first = false;
            previousItemSkipped = itemNode && itemNode->verbatim;
            dedentAfterAssignmentConditional = dedentAfterAssignmentConditional ||
                                               closesAssignmentConditional;
            bool nextContinuesLine = false;
            if (i < list.children.size()) {
                if (auto startsLine = firstRealTokenStartsLine(list.children[i]))
                    nextContinuesLine = !*startsLine;
            }
            bool nextStartsRecovery = i < list.children.size() &&
                                      (childContainsMacro(list.children[i]) ||
                                       childStartsWithInlineVerbatim(list.children[i]));
            previousMacroRecovery =
                !hadSeparator && !hasSemicolon &&
                ((currentMacroOnly && macroJoinsFollowing) ||
                 ((containsMacro || groupedMacro || noNameHierarchy) && nextContinuesLine) ||
                 (nextStartsRecovery && nextContinuesLine) ||
                 (i < list.children.size() && childStartsWithInlineVerbatim(list.children[i])));
        }

        if (!rootList) {
            hardLine();
            awaitingClosingTrivia = true;
        }
        else {
            lineStart = false;
        }
        spacingProvided = false;
    }

    void lowerDynamicList(const NormalizedList& list) {
        if (forceInlineLists) {
            for (const auto& child : list.children)
                lowerChild(child);
            return;
        }
        size_t itemCount = std::ranges::count_if(list.children, [](const NormalizedChild& child) {
            return std::holds_alternative<std::unique_ptr<NormalizedNode>>(child.value);
        });
        bool hasComment = std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
            return childHasForcingComment(child);
        });
        bool hasEscapedIdentifier =
            std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
                return childHasEscapedIdentifier(child);
            });
        bool hasMacro = std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
            return childContainsMacro(child);
        });
        bool hasVerticalMacro =
            hasMacro && (list.parentKind != SyntaxKind::ArgumentList ||
                         std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
                             return childHasVerticalMacro(child);
                         }));
        bool forceVertical = hasComment || hasVerticalMacro ||
                             (inMultipleConcatenation && itemCount > 1);
        size_t listWidth = 0;
        for (const auto& child : list.children)
            listWidth += flatWidth(child) + (listWidth ? 1 : 0);
        if ((itemCount <= 1 && !hasComment && !hasEscapedIdentifier && !hasMacro) ||
            (noNameInstantiation && list.parentKind == SyntaxKind::HierarchicalInstance &&
             itemCount <= 1)) {
            for (const auto& child : list.children)
                lowerChild(child);
            return;
        }

        size_t begin = mark();
        GroupId group = builder.createConsistentGroup();
        int listPriority = isListHandledExpression(list.parentKind) ? expressionBreakPriority + 10
                                                                    : 1;
        bool prefixComment = !list.children.empty() &&
                             childStartsWithInlineBlockComment(list.children.front());
        if (hasEscapedIdentifier || forceVertical)
            append(builder.hardLine(1, true));
        else
            append(builder.softLine(listPriority, prefixComment ? " " : "", group));
        spacingProvided = true;
        bool savedDynamicList = inDynamicList;
        GroupId savedDynamicGroup = currentDynamicGroup;
        bool savedDynamicListForceVertical = dynamicListForceVertical;
        bool savedDynamicListLikelyVertical = dynamicListLikelyVertical;
        inDynamicList = true;
        currentDynamicGroup = group;
        dynamicListForceVertical = forceVertical || hasEscapedIdentifier;
        dynamicListLikelyVertical = forceVertical || hasEscapedIdentifier ||
                                    (config.columnLimit.get() &&
                                     listWidth > config.columnLimit.get() * 3 / 5);
        bool emittedListContent = false;
        bool previousSeparatorCarriedMacro = false;
        for (const auto& child : list.children) {
            bool commaWithMacro = false;
            if (auto tokenIndex = std::get_if<size_t>(&child.value)) {
                commaWithMacro = normalized.tokens().at(*tokenIndex).token.kind ==
                                     TokenKind::Comma &&
                                 childContainsMacro(child);
            }
            if (forceVertical && emittedListContent && childContainsMacro(child) &&
                !commaWithMacro && !previousSeparatorCarriedMacro) {
                append(builder.hardLine(1, true));
            }
            previousSeparatorCarriedMacro = false;
            lowerChild(child);
            emittedListContent = true;
            if (auto tokenIndex = std::get_if<size_t>(&child.value)) {
                auto token = normalized.tokens().at(*tokenIndex).token;
                if (token && token.kind == TokenKind::Comma) {
                    bool carriesFollowingMacro = std::ranges::any_of(
                        normalized.tokens().at(*tokenIndex).trailing,
                        [](const NormalizedTrivia& trivia) {
                            return trivia.kind == NormalizedTriviaKind::MacroUsage;
                        }
                    );
                    previousSeparatorCarriedMacro = carriesFollowingMacro;
                    if (!carriesFollowingMacro) {
                        if (forceVertical)
                            append(builder.hardLine(1, true));
                        else
                            append(builder.softLine(listPriority, " ", group));
                        spacingProvided = true;
                    }
                }
            }
        }
        inDynamicList = savedDynamicList;
        currentDynamicGroup = savedDynamicGroup;
        dynamicListForceVertical = savedDynamicListForceVertical;
        dynamicListLikelyVertical = savedDynamicListLikelyVertical;
        auto contents = capture(begin);
        int listIndent = static_cast<int>(config.indentWidth.get());
        if (hasComment && list.parentKind == SyntaxKind::HierarchicalInstance)
            listIndent += 2;
        auto indented = builder.indent(listIndent, contents);
        append(builder.concat(
            {indented,
             forceVertical ? builder.hardLine(1, true) : builder.softLine(listPriority, "", group)}
        ));
    }

    void lowerList(const NormalizedList& list) {
        if (!list.children.empty() && nodeNeedsOwnLine(list.parentKind, list.listIndex)) {
            for (const auto& child : list.children)
                lowerChild(child);
            hardLine();
            return;
        }
        auto savedAlignmentGroup = currentAlignmentGroup;
        bool parameterPortList = list.parentKind == SyntaxKind::ParameterPortList;
        if (parameterPortList)
            parameterPortDepth++;
        bool nestedParameterList = parameterPortDepth && !parameterPortList;
        if (!nestedParameterList && (list.style != ListStyle::Inline ||
                                     (list.parentKind == SyntaxKind::HierarchicalInstance &&
                                      forceConnectionListVertical))) {
            currentAlignmentGroup = builder.createAlignmentGroup();
        }
        bool rootList = list.parentKind == SyntaxKind::CompilationUnit;
        if (list.parentKind == SyntaxKind::HierarchicalInstance && forceConnectionListVertical) {
            bool hasComment = std::ranges::any_of(list.children, [&](const NormalizedChild& child) {
                return childHasForcingComment(child);
            });
            bool startsWithStandaloneComment = false;
            if (!list.children.empty()) {
                if (auto token = firstTokenIndex(list.children.front())) {
                    startsWithStandaloneComment = std::ranges::any_of(
                        normalized.tokens().at(*token).leading, [](const NormalizedTrivia& trivia) {
                            return trivia.kind == NormalizedTriviaKind::Comment &&
                                   trivia.placement == TriviaPlacement::Standalone;
                        }
                    );
                }
            }
            if (!hasComment || !startsWithStandaloneComment) {
                lowerVerticalList(list, false);
            }
            else {
                size_t begin = mark();
                lowerVerticalList(list, false);
                append(builder.indent(2, capture(begin)));
            }
            currentAlignmentGroup = savedAlignmentGroup;
            if (parameterPortList)
                parameterPortDepth--;
            return;
        }
        switch (list.style) {
            case ListStyle::Body:
            case ListStyle::Vertical:
                if (list.parentKind == SyntaxKind::StructuredAssignmentPattern &&
                    list.children.size() == 1 && childNode(list.children.front()) &&
                    !childNode(list.children.front())->verbatimFirstToken && !lineStart &&
                    !tokenHasComment(list.endTokenIndex)) {
                    lowerDynamicList(list);
                }
                else {
                    lowerVerticalList(list, rootList);
                }
                break;
            case ListStyle::Dynamic:
                if (std::ranges::any_of(list.children, [](const auto& child) {
                        auto node = childNode(child);
                        return node && node->verbatimFirstToken.has_value();
                    }))
                    lowerVerticalList(list, false);
                else
                    lowerDynamicList(list);
                break;
            case ListStyle::Inline:
                for (const auto& child : list.children)
                    lowerChild(child);
                break;
        }
        currentAlignmentGroup = savedAlignmentGroup;
        if (parameterPortList)
            parameterPortDepth--;
    }

    void lowerNode(const NormalizedNode& node, SyntaxKind parentExpressionKind, bool isRoot) {
        for (const auto& trivia : node.leading)
            emitTrivia(trivia, false, true);
        if (node.verbatim) {
            size_t begin = mark();
            if (auto token = node.verbatimFirstToken
                                 ? node.verbatimFirstToken
                                 : firstTokenIndex(node, 0, node.children.size())) {
                for (const auto& trivia : normalized.tokens().at(*token).leading)
                    emitTrivia(trivia, false);
            }
            append(builder.preservedText(node.verbatimText));
            lineStart = node.verbatimText.ends_with('\n');
            if (node.verbatimLastToken) {
                lastToken = &normalized.tokens().at(*node.verbatimLastToken);
                for (const auto& trivia : normalized.tokens().at(*node.verbatimLastToken).trailing)
                    emitTrivia(trivia, true);
            }
            spacingProvided = false;
            auto contents = capture(begin);
            if (!isRoot &&
                (MemberSyntax::isKind(node.kind) || StatementSyntax::isKind(node.kind))) {
                contents = builder.member(contents, node.kind);
            }
            append(contents);
            return;
        }
        bool entersClass = node.kind == SyntaxKind::ClassDeclaration;
        bool entersSubroutine = node.kind == SyntaxKind::FunctionDeclaration ||
                                node.kind == SyntaxKind::TaskDeclaration;
        if (entersClass)
            classDepth++;
        if (entersSubroutine)
            subroutineDepth++;
        bool expression = isExpressionKind(node.kind);
        bool savedSuppressTypedefAlignment = suppressTypedefAlignment;
        if (node.kind == SyntaxKind::TypedefDeclaration &&
            (containsKind(node, SyntaxKind::StructType) ||
             containsKind(node, SyntaxKind::UnionType) ||
             containsKind(node, SyntaxKind::EnumType))) {
            suppressTypedefAlignment = true;
        }
        auto savedCurrentDeclaratorAlignmentColumn = currentDeclaratorAlignmentColumn;
        if (node.kind == SyntaxKind::Declarator) {
            currentDeclaratorAlignmentColumn = nextDeclaratorAlignmentColumn;
            nextDeclaratorAlignmentColumn += 2;
        }
        bool assignment = SyntaxFacts::isAssignmentOperator(node.kind) ||
                          node.kind == SyntaxKind::EqualsValueClause;
        auto savedMemberKind = currentMemberKind;
        bool savedMacroVariableDimension = macroVariableDimension;
        bool savedSpaceBeforeMacroVariableDimension = spaceBeforeMacroVariableDimension;
        bool savedBreakMacroDimensionAfterPlus = breakMacroDimensionAfterPlus;
        bool savedAllowComparisonBreak = allowComparisonBreak;
        bool savedAllowCurrentBinaryBreak = allowCurrentBinaryBreak;
        bool savedHardBreakCurrentBinary = hardBreakCurrentBinary;
        int savedExpressionBreakPriority = expressionBreakPriority;
        bool savedBinaryContinuationScope = binaryContinuationScope;
        int savedBinaryContinuationIndent = binaryContinuationIndent;
        bool savedMultipleConcatenation = inMultipleConcatenation;
        bool savedForceTernaryBranches = forceTernaryBranches;
        bool savedForceInlineLists = forceInlineLists;
        if (node.kind == SyntaxKind::ArgumentList) {
            // Breaking before an inline directive changes its classification
            // when the formatted text is parsed again.
            for (const auto& child : node.children) {
                if (auto index = std::get_if<size_t>(&child.value)) {
                    forceInlineLists =
                        forceInlineLists ||
                        std::ranges::any_of(
                            normalized.tokens().at(*index).leading,
                            [](const NormalizedTrivia& trivia) {
                                return trivia.kind == NormalizedTriviaKind::ConditionalDirective &&
                                       trivia.placement == TriviaPlacement::Inline;
                            }
                        );
                }
            }
        }
        bool commentedPatternConditional =
            node.kind == SyntaxKind::AssignmentPatternItem &&
            containsKind(node, SyntaxKind::ConditionalExpression) &&
            std::ranges::any_of(node.children, [&](const NormalizedChild& child) {
                return childHasForcingComment(child);
            });
        if (commentedPatternConditional)
            forceTernaryBranches = true;
        if (node.kind == SyntaxKind::VariableDimension) {
            bool containsMacro =
                std::ranges::any_of(node.children, [&](const NormalizedChild& child) {
                    return childContainsMacro(child);
                });
            macroVariableDimension =
                !hasMacroDefinitions &&
                std::ranges::any_of(node.children, [&](const NormalizedChild& child) {
                    return childHasMacroPlaceholder(child);
                });
            spaceBeforeMacroVariableDimension = containsMacro && !macroVariableDimension;
            size_t macroCount = 0;
            for (const auto& child : node.children)
                macroCount += childMacroCount(child);
            breakMacroDimensionAfterPlus = macroVariableDimension && macroCount > 1;
        }
        if (node.kind == SyntaxKind::MultipleConcatenationExpression)
            inMultipleConcatenation = true;
        if (currentMemberKind == SyntaxKind::Unknown &&
            (MemberSyntax::isKind(node.kind) || StatementSyntax::isKind(node.kind))) {
            currentMemberKind = node.kind;
        }
        size_t begin = mark();
        bool ternaryTable = false;
        bool castWithOperandBreak = false;
        bool castAssignmentRhs = false;
        bool addBinaryContinuationIndent = false;
        bool anchorBinaryContinuation = false;
        int binaryContinuationAnchorOffset = static_cast<int>(config.indentWidth.get());
        bool parenthesizedBinary = isBinaryKind(node.kind) &&
                                   (parentExpressionKind == SyntaxKind::ParenthesizedExpression ||
                                    parentExpressionKind == SyntaxKind::ParenthesizedPropertyExpr ||
                                    parentExpressionKind == SyntaxKind::ParenthesizedSequenceExpr);
        if (isBinaryKind(node.kind) && !assignment) {
            bool sameChain = isBinaryKind(parentExpressionKind) &&
                             SyntaxFacts::getPrecedence(node.kind) > 0 &&
                             SyntaxFacts::getPrecedence(node.kind) ==
                                 SyntaxFacts::getPrecedence(parentExpressionKind);
            if (!sameChain)
                expressionBreakPriority++;
            if (assignmentRhsRoot) {
                bool rootChain = isBinaryKind(assignmentRhsRoot->kind) && sameChain;
                if (assignmentRhsRoot->kind == SyntaxKind::ConditionalExpression) {
                    bool branchRoot = ternaryTrueBranch == &node || ternaryFalseBranch == &node;
                    addBinaryContinuationIndent = branchRoot && !binaryContinuationScope;
                    if (addBinaryContinuationIndent) {
                        anchorBinaryContinuation = true;
                        binaryContinuationScope = true;
                    }
                }
                else if (isBinaryKind(assignmentRhsRoot->kind)) {
                    addBinaryContinuationIndent = assignmentRhsRoot != &node && !rootChain;
                    if (addBinaryContinuationIndent && !isBinaryKind(parentExpressionKind)) {
                        anchorBinaryContinuation = true;
                        binaryContinuationScope = true;
                    }
                }
                else if (!binaryContinuationScope) {
                    addBinaryContinuationIndent = assignmentRhsRoot != &node;
                    if (addBinaryContinuationIndent) {
                        anchorBinaryContinuation = true;
                        binaryContinuationScope = true;
                    }
                }
                else if (!sameChain) {
                    addBinaryContinuationIndent = true;
                    anchorBinaryContinuation = !isBinaryKind(parentExpressionKind);
                }
            }
            else if (!binaryContinuationScope) {
                addBinaryContinuationIndent = !sameChain;
                if (addBinaryContinuationIndent) {
                    anchorBinaryContinuation = true;
                    binaryContinuationScope = true;
                }
            }
            else if (!sameChain) {
                addBinaryContinuationIndent = isBinaryKind(parentExpressionKind);
            }
            if (parenthesizedBinary) {
                addBinaryContinuationIndent = true;
                anchorBinaryContinuation = true;
                binaryContinuationScope = true;
            }
            if (inAssignmentCastOperand) {
                addBinaryContinuationIndent = false;
                anchorBinaryContinuation = false;
            }
            if (!node.children.empty() && childContainsMacro(node.children.front()))
                addBinaryContinuationIndent = false;
            if (anchorBinaryContinuation) {
                auto first = firstTokenIndex(node, 0, node.children.size());
                if (first &&
                    std::ranges::any_of(
                        normalized.tokens().at(*first).leading, [](const NormalizedTrivia& trivia) {
                            return trivia.kind == NormalizedTriviaKind::Comment &&
                                   trivia.placement == TriviaPlacement::Standalone;
                        }
                    )) {
                    anchorBinaryContinuation = false;
                }
            }
            if (anchorBinaryContinuation && parenthesizedBinary) {
                int precedence = SyntaxFacts::getPrecedence(node.kind);
                auto containsNestedPrecedenceGroup = [&](auto&& self,
                                                         const NormalizedNode& current) -> bool {
                    for (const auto& child : current.children) {
                        auto nested = childNode(child);
                        if (!nested || !isBinaryKind(nested->kind))
                            continue;
                        int nestedPrecedence = SyntaxFacts::getPrecedence(nested->kind);
                        if (nestedPrecedence != precedence) {
                            if (!isComparisonKind(nested->kind))
                                return true;
                            continue;
                        }
                        if (self(self, *nested))
                            return true;
                    }
                    return false;
                };
                if (containsNestedPrecedenceGroup(containsNestedPrecedenceGroup, node))
                    binaryContinuationAnchorOffset = 0;
            }
            if (addBinaryContinuationIndent && !anchorBinaryContinuation &&
                !PropertyExprSyntax::isKind(node.kind) && !SequenceExprSyntax::isKind(node.kind)) {
                binaryContinuationIndent += static_cast<int>(config.indentWidth.get());
            }
            allowComparisonBreak = !isComparisonKind(node.kind);
            allowCurrentBinaryBreak = true;
            const NormalizedNode* left = nullptr;
            const NormalizedNode* right = nullptr;
            for (const auto& child : node.children) {
                if (auto nested = childNode(child); nested && isExpressionKind(nested->kind)) {
                    if (!left)
                        left = nested;
                    else {
                        right = nested;
                        break;
                    }
                }
            }
            if (left)
                allowCurrentBinaryBreak = flatWidth(node.children.front()) >= 4;
            if (isComparisonKind(node.kind)) {
                allowComparisonBreak = (left && isParenthesizedBinary(*left)) ||
                                       (right && isParenthesizedBinary(*right));
            }
            hardBreakCurrentBinary = right && containsMultilineList(*right) &&
                                     config.columnLimit.get() &&
                                     formattedFlatWidth(*right) > config.columnLimit.get() * 3 / 5;
        }
        if (node.kind == SyntaxKind::ConditionalStatement)
            lowerConditional(node);
        else if (node.kind == SyntaxKind::PropertyDeclaration)
            lowerAssertionDeclaration(node, TokenKind::EndPropertyKeyword);
        else if (node.kind == SyntaxKind::SequenceDeclaration)
            lowerAssertionDeclaration(node, TokenKind::EndSequenceKeyword);
        else if (node.kind == SyntaxKind::CaseStatement)
            lowerCaseStatement(node);
        else if (node.kind == SyntaxKind::StandardCaseItem ||
                 node.kind == SyntaxKind::DefaultCaseItem)
            lowerCaseItem(node);
        else if (ProceduralBlockSyntax::isKind(node.kind))
            lowerProceduralBlock(node);
        else if (node.kind == SyntaxKind::TimingControlStatement)
            lowerTimingControlStatement(node);
        else if (node.kind == SyntaxKind::ActionBlock &&
                 std::ranges::any_of(node.children, [&](const auto& child) {
                     auto statement = childNode(child);
                     return statement && statement->kind == SyntaxKind::EmptyStatement &&
                            childContainsMacro(child);
                 })) {
            for (const auto& child : node.children) {
                auto statement = childNode(child);
                if (statement && statement->kind == SyntaxKind::EmptyStatement)
                    lowerIndentedChild(child);
                else {
                    hardLine();
                    lowerChild(child);
                }
            }
        }
        else if (node.kind == SyntaxKind::AssertPropertyStatement ||
                 node.kind == SyntaxKind::AssumePropertyStatement)
            lowerConcurrentAssertion(node);
        else if (node.kind == SyntaxKind::LoopStatement ||
                 node.kind == SyntaxKind::ForLoopStatement ||
                 node.kind == SyntaxKind::ForeachLoopStatement ||
                 node.kind == SyntaxKind::ForeverStatement)
            lowerControlledStatement(node);
        else if (node.kind == SyntaxKind::HierarchyInstantiation)
            lowerHierarchyInstantiation(node);
        else if (node.kind == SyntaxKind::ParameterValueAssignment)
            lowerParameterValueAssignment(node);
        else if (assignment)
            lowerAssignment(node);
        else if (node.kind == SyntaxKind::ConditionalExpression)
            ternaryTable = lowerTernary(node);
        else if (node.kind == SyntaxKind::CastExpression &&
                 (castRequiresOperandBreak(node) ||
                  currentMemberKind == SyntaxKind::NamedPortConnection ||
                  currentMemberKind == SyntaxKind::NamedParamAssignment ||
                  currentMemberKind == SyntaxKind::NamedArgument)) {
            castAssignmentRhs = assignmentRhsRoot == &node;
            for (const auto& child : node.children) {
                auto nested = childNode(child);
                if (!nested || nested->kind != SyntaxKind::ParenthesizedExpression) {
                    lowerChild(child, node.kind);
                    continue;
                }

                castWithOperandBreak = true;
                size_t operandBegin = SIZE_MAX;
                bool savedAssignmentCastOperand = inAssignmentCastOperand;
                for (size_t i = 0; i < nested->children.size(); i++) {
                    lowerChild(nested->children[i], nested->kind);
                    if (i == 0) {
                        if (castRequiresOperandBreak(node)) {
                            operandBegin = mark();
                            hardLine(1, !castAssignmentRhs);
                            inAssignmentCastOperand = castAssignmentRhs;
                        }
                        else {
                            append(builder.softLine(0, ""));
                            spacingProvided = true;
                        }
                    }
                }
                inAssignmentCastOperand = savedAssignmentCastOperand;
                if (operandBegin != SIZE_MAX && castAssignmentRhs) {
                    auto operand = capture(operandBegin);
                    append(builder.indentIfBreak(
                        assignmentRhsLine, static_cast<int>(config.indentWidth.get()), operand
                    ));
                }
            }
        }
        else if (node.kind == SyntaxKind::ImplicationPropertyExpr && config.columnLimit.get() &&
                 formattedFlatWidth(node) > config.columnLimit.get())
            lowerLongImplication(node);
        else
            for (const auto& child : node.children)
                lowerChild(child, expression ? node.kind : SyntaxKind::Unknown);
        auto contents = capture(begin);

        if (node.kind == SyntaxKind::VariableDimension && macroVariableDimension) {
            contents = builder.relativeAnchor(breakMacroDimensionAfterPlus ? 2 : -2, contents);
        }

        if (castWithOperandBreak && !castAssignmentRhs)
            contents = builder.relativeAnchor(static_cast<int>(config.indentWidth.get()), contents);

        if (node.kind == SyntaxKind::AssignmentPatternExpression &&
            currentMemberKind == SyntaxKind::NamedPortConnection) {
            contents = builder.relativeAnchor(0, contents);
        }

        if (isBinaryKind(node.kind) && !assignment) {
            if (addBinaryContinuationIndent && !inTernaryTableValue) {
                int width = static_cast<int>(config.indentWidth.get());
                if ((PropertyExprSyntax::isKind(node.kind) ||
                     SequenceExprSyntax::isKind(node.kind)) &&
                    !parenthesizedBinary && verticallyWrappedImplication != &node) {
                    contents = builder.indent(width, contents);
                }
                else if (anchorBinaryContinuation) {
                    contents = builder.relativeAnchor(binaryContinuationAnchorOffset, contents);
                }
            }
        }
        else if (node.kind == SyntaxKind::ConditionalExpression && !ternaryTable) {
            contents = builder.relativeAnchor(static_cast<int>(config.indentWidth.get()), contents);
        }

        if (!isRoot && (MemberSyntax::isKind(node.kind) || StatementSyntax::isKind(node.kind)))
            contents = builder.member(contents, node.kind);
        append(contents);
        currentMemberKind = savedMemberKind;
        macroVariableDimension = savedMacroVariableDimension;
        spaceBeforeMacroVariableDimension = savedSpaceBeforeMacroVariableDimension;
        breakMacroDimensionAfterPlus = savedBreakMacroDimensionAfterPlus;
        allowComparisonBreak = savedAllowComparisonBreak;
        allowCurrentBinaryBreak = savedAllowCurrentBinaryBreak;
        hardBreakCurrentBinary = savedHardBreakCurrentBinary;
        expressionBreakPriority = savedExpressionBreakPriority;
        binaryContinuationScope = savedBinaryContinuationScope;
        binaryContinuationIndent = savedBinaryContinuationIndent;
        inMultipleConcatenation = savedMultipleConcatenation;
        forceTernaryBranches = savedForceTernaryBranches;
        forceInlineLists = savedForceInlineLists;
        currentDeclaratorAlignmentColumn = savedCurrentDeclaratorAlignmentColumn;
        suppressTypedefAlignment = savedSuppressTypedefAlignment;
        if (entersClass)
            classDepth--;
        if (entersSubroutine)
            subroutineDepth--;
    }

    const NormalizedFormatDocument& normalized;
    const Config& config;
    FormatStage stage;
    DocumentBuilder builder;
    // Normalized children are immutable for the lifetime of this lowering pass.
    mutable std::unordered_map<const NormalizedChild*, SubtreeSummary> subtreeSummaries;
    std::vector<DocId> output;
    const NormalizedToken* lastToken = nullptr;
    SyntaxKind currentMemberKind = SyntaxKind::Unknown;
    bool currentMemberContainsMacro = false;
    size_t bracketDepth = 0;
    bool lineStart = true;
    bool spacingProvided = false;
    bool pendingAssignmentBreak = false;
    bool pendingRecoveredAssignmentBreak = false;
    bool emittingAssignmentOperator = false;
    bool assignmentOperatorCommentUseAnchor = true;
    bool caseBreakClauses = false;
    bool suppressCaseAlignment = false;
    bool awaitingClosingTrivia = false;
    bool inDynamicList = false;
    GroupId currentDynamicGroup = 0;
    AlignmentGroupId currentAlignmentGroup = 0;
    bool dynamicListForceVertical = false;
    bool dynamicListLikelyVertical = false;
    size_t conditionalDepth = 0;
    size_t classDepth = 0;
    size_t subroutineDepth = 0;
    size_t parameterPortDepth = 0;
    bool dedentConditionalContent = false;
    bool conditionalAssignmentRhs = false;
    std::vector<bool> conditionalAssignmentStack;
    bool followsSkippedMember = false;
    uint32_t nextDeclaratorAlignmentColumn = 1;
    std::optional<uint32_t> currentDeclaratorAlignmentColumn;
    bool suppressAlignment = false;
    bool suppressEscapedAlignment = false;
    bool portDirectionAnchorEmitted = false;
    bool packedDimensionAnchorEmitted = false;
    bool suppressTypedefAlignment = false;
    bool lastWasMacro = false;
    bool inlineBlockCommentOnLine = false;
    size_t itemFinalConditionalDepth = 0;
    bool currentItemMacroOnly = false;
    bool macroVariableDimension = false;
    bool spaceBeforeMacroVariableDimension = false;
    bool breakMacroDimensionAfterPlus = false;
    bool hasMacroDefinitions = false;
    bool inlineConditionalClosed = false;
    bool dedentListConditionalDirective = false;
    bool forceParameterListVertical = false;
    bool forceConnectionListVertical = false;
    bool noNameInstantiation = false;
    bool noNameCallStyle = false;
    size_t instanceOpenParenPadding = 0;
    const NormalizedNode* assignmentRhsRoot = nullptr;
    DocId assignmentRhsLine = 0;
    bool inAssignmentCastOperand = false;
    const NormalizedNode* verticallyWrappedImplication = nullptr;
    bool binaryContinuationScope = false;
    int binaryContinuationIndent = 0;
    bool inMultipleConcatenation = false;
    bool allowComparisonBreak = true;
    bool allowCurrentBinaryBreak = true;
    bool hardBreakCurrentBinary = false;
    bool manualExpressionBreaks = false;
    bool forceInlineLists = false;
    bool forceTernaryBranches = false;
    bool inTernaryTableValue = false;
    const NormalizedNode* ternaryTrueBranch = nullptr;
    const NormalizedNode* ternaryFalseBranch = nullptr;
    size_t pendingTokenIndent = 0;
    std::optional<size_t> skipLeadingCommentsToken;
    int ternaryBreakPriority = 2;
    int expressionBreakPriority = 1;
};

} // namespace

FormatDocument buildLayoutDocument(
    const NormalizedFormatDocument& normalized,
    const Config& config,
    FormatStage stage
) {
    validateConfig(config);
    return Lowerer(normalized, config, stage).build();
}

} // namespace format

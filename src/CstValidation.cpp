//------------------------------------------------------------------------------
// CstValidation.cpp
// CST equivalence and diff helpers for the SystemVerilog formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatValidation.h"
#include <algorithm>
#include <fmt/format.h>
#include <ranges>

#include "slang/parsing/Token.h"
#include "slang/syntax/SyntaxNode.h"
#include "slang/text/CharInfo.h"

using namespace slang;
using namespace slang::parsing;
using namespace slang::syntax;

namespace format {
namespace {

/// Extended equivalence check for syntax nodes that also compares preprocessor
/// trivia (directives, skipped syntax, skipped tokens, disabled text) and
/// optionally comments.
template<bool CheckComments>
bool isEquivalentTo(const SyntaxNode& node, const SyntaxNode& other);

template<bool CheckComments>
bool isRelevantTrivia(const Trivia& t) {
    if (t.kind == TriviaKind::Directive || t.kind == TriviaKind::SkippedSyntax ||
        t.kind == TriviaKind::SkippedTokens) {
        return true;
    }
    if (t.kind == TriviaKind::DisabledText) {
        return !std::ranges::all_of(t.getRawText(), isWhitespace);
    }
    if constexpr (CheckComments) {
        if (t.kind == TriviaKind::LineComment || t.kind == TriviaKind::BlockComment)
            return true;
    }
    return false;
}

template<bool CheckComments>
bool isTriviaEqual(const Trivia& a, const Trivia& b);

std::string canonicalizeComment(std::string_view text, bool blockComment) {
    std::string result;
    result.reserve(text.size());
    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t lineEnd = text.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string_view::npos)
            lineEnd = text.size();
        size_t contentStart = lineStart;
        if (blockComment && lineStart > 0) {
            while (contentStart < lineEnd && isTabOrSpace(text[contentStart]))
                contentStart++;
        }
        size_t contentEnd = lineEnd;
        while (contentEnd > contentStart && isTabOrSpace(text[contentEnd - 1]))
            contentEnd--;
        result.append(text.substr(contentStart, contentEnd - contentStart));
        if (lineEnd == text.size())
            break;
        result.push_back('\n');
        lineStart = lineEnd + 1;
        if (text[lineEnd] == '\r' && lineStart < text.size() && text[lineStart] == '\n')
            lineStart++;
    }
    return result;
}

template<bool CheckComments>
bool areTokensEquivalent(const Token& lt, const Token& rt) {
    if (lt.kind != rt.kind || lt.valueText() != rt.valueText())
        return false;

    auto lTrivia = lt.trivia() | std::views::filter(isRelevantTrivia<CheckComments>);
    auto rTrivia = rt.trivia() | std::views::filter(isRelevantTrivia<CheckComments>);
    return std::ranges::equal(lTrivia, rTrivia, isTriviaEqual<CheckComments>);
}

template<bool CheckComments>
bool isTriviaEqual(const Trivia& a, const Trivia& b) {
    if (a.kind != b.kind)
        return false;

    if constexpr (CheckComments) {
        if (a.kind == TriviaKind::LineComment || a.kind == TriviaKind::BlockComment) {
            bool blockComment = a.kind == TriviaKind::BlockComment;
            return canonicalizeComment(a.getRawText(), blockComment) ==
                   canonicalizeComment(b.getRawText(), blockComment);
        }
        if (a.kind == TriviaKind::DisabledText)
            return a.getRawText() == b.getRawText();
    }

    if (a.kind == TriviaKind::Directive || a.kind == TriviaKind::SkippedSyntax)
        return isEquivalentTo<CheckComments>(*a.syntax(), *b.syntax());

    auto aTokens = a.getSkippedTokens();
    auto bTokens = b.getSkippedTokens();
    return std::ranges::equal(aTokens, bTokens, areTokensEquivalent<CheckComments>);
}

template<bool CheckComments>
bool isEquivalentTo(const SyntaxNode& node, const SyntaxNode& other) {
    size_t childCount = node.getChildCount();
    if (node.kind != other.kind || childCount != other.getChildCount())
        return false;

    for (size_t i = 0; i < childCount; i++) {
        auto ln = node.childNode(i);
        auto rn = other.childNode(i);
        if (bool(ln) != bool(rn))
            return false;

        if (ln) {
            if (!isEquivalentTo<CheckComments>(*ln, *rn))
                return false;
        }
        else {
            Token lt = node.childToken(i);
            Token rt = other.childToken(i);
            if (bool(lt) != bool(rt))
                return false;

            if (lt) {
                if (!areTokensEquivalent<CheckComments>(lt, rt))
                    return false;
            }
        }
    }
    return true;
}

/// Truncate a string to maxLen, appending "..." if truncated.
std::string truncate(std::string s, size_t maxLen = 200) {
    if (s.size() > maxLen)
        s = s.substr(0, maxLen) + "...";
    return s;
}

/// Render a token with its relevant trivia for diagnostic output.
template<bool CheckComments>
std::string describeToken(const Token& tok) {
    std::string s = std::string(toString(tok.kind)) + " '" + std::string(tok.valueText()) + "'";
    bool first = true;
    for (auto& t : tok.trivia()) {
        if (!isRelevantTrivia<CheckComments>(t))
            continue;
        if (first) {
            s += "\n      trivia:";
            first = false;
        }
        s += "\n        " + std::string(toString(t.kind));
        if (t.kind == TriviaKind::Directive || t.kind == TriviaKind::SkippedSyntax) {
            s += " " + truncate(t.syntax()->toString(), 80);
        }
        else {
            auto text = t.getRawText();
            if (!text.empty())
                s += " '" + truncate(std::string(text), 80) + "'";
        }
    }
    return s;
}

template<bool CheckComments>
std::string describeCstDiffImpl(
    const SyntaxNode& node,
    const SyntaxNode& other,
    std::string path = "root"
) {
    if (node.kind != other.kind)
        return path + ": node kind differs: " + std::string(toString(other.kind)) + " vs " +
               std::string(toString(node.kind)) + "\n  original:  " + truncate(other.toString()) +
               "\n  formatted: " + truncate(node.toString());

    size_t lCount = node.getChildCount();
    size_t rCount = other.getChildCount();
    size_t minCount = std::min(lCount, rCount);

    // Walk shared children to find the first divergence point.
    for (size_t i = 0; i < minCount; i++) {
        auto childPath = path + "/" + std::string(toString(node.kind)) + "[" + std::to_string(i) +
                         "]";
        auto ln = node.childNode(i);
        auto rn = other.childNode(i);
        if (bool(ln) != bool(rn)) {
            auto describe = [&](const char* label, bool isNode, size_t idx,
                                const SyntaxNode& parent) -> std::string {
                if (isNode)
                    return std::string(label) + " is node kind " +
                           std::string(toString(parent.childNode(idx)->kind)) + ": " +
                           truncate(parent.childNode(idx)->toString());
                else if (auto t = parent.childToken(idx))
                    return std::string(label) + " is token kind " + std::string(toString(t.kind)) +
                           ": " + truncate(std::string(t.rawText()));
                else
                    return std::string(label) + " is empty";
            };
            return childPath + ": child type differs (node vs token)\n  " +
                   describe("formatted", bool(ln), i, node) + "\n  " +
                   describe("original", bool(rn), i, other);
        }

        if (ln) {
            if (!isEquivalentTo<CheckComments>(*ln, *rn)) {
                auto diff = describeCstDiffImpl<CheckComments>(*ln, *rn, childPath);
                if (!diff.empty())
                    return diff;
            }
        }
        else {
            Token lt = node.childToken(i);
            Token rt = other.childToken(i);
            if (bool(lt) != bool(rt))
                return childPath + ": child presence differs";

            if (lt && !areTokensEquivalent<CheckComments>(lt, rt)) {
                std::string reason;
                if (lt.kind != rt.kind)
                    reason = "token kind differs";
                else if (lt.valueText() != rt.valueText())
                    reason = "token text differs";
                else
                    reason = "token trivia differs";
                std::string result = childPath;
                result.append(": ").append(reason);
                result.append("\n    original:  ").append(describeToken<CheckComments>(rt));
                result.append("\n    formatted: ").append(describeToken<CheckComments>(lt));
                return result;
            }
        }
    }

    // Shared prefix was equal but child counts differ; show the extra children.
    if (lCount != rCount) {
        auto extraPath = path + "/" + std::string(toString(node.kind)) + "[" +
                         std::to_string(minCount) + "]";
        const auto& longer = lCount > rCount ? node : other;
        auto label = lCount > rCount ? "formatted" : "original";
        std::string extra;
        if (auto* child = longer.childNode(minCount))
            extra = truncate(child->toString());
        else if (auto tok = longer.childToken(minCount); tok)
            extra = std::string(tok.valueText());
        return extraPath + ": " + label + " has " +
               std::to_string(std::max(lCount, rCount) - minCount) +
               " extra children, first: " + extra;
    }

    return "";
}

} // namespace

bool isPreprocessorEquivalentTo(const SyntaxNode& a, const SyntaxNode& b) {
    return isEquivalentTo<false>(a, b);
}

bool isCommentEquivalentTo(const SyntaxNode& a, const SyntaxNode& b) {
    return isEquivalentTo<true>(a, b);
}

std::string describeCstDiff(const SyntaxNode& node, const SyntaxNode& other) {
    return describeCstDiffImpl<true>(node, other);
}

namespace {

/// Returns true if a token contributes a "real" token to the source stream —
/// i.e., it has actual raw text. Missing/synthetic placeholder tokens are
/// skipped because they're artifacts of the CST shape, not source content.
bool isRealToken(const Token& t) {
    return t && !t.rawText().empty();
}

/// Iterate all real tokens in a subtree via tokens_begin()/end(), skipping
/// placeholders. Invokes the visitor with each token.
template<typename F>
void forEachRealToken(const SyntaxNode& node, F&& f) {
    for (auto it = node.tokens_begin(); it != node.tokens_end(); ++it) {
        Token t = *it;
        if (isRealToken(t))
            f(t);
    }
}

} // namespace

bool isTokenEquivalentTo(const SyntaxNode& a, const SyntaxNode& b) {
    enum class ItemKind { Token, LineComment, BlockComment, DisabledText };
    struct Item {
        ItemKind kind;
        TokenKind tokenKind = TokenKind::Unknown;
        std::string text;

        bool operator==(const Item&) const = default;
    };

    auto flatten = [](const SyntaxNode& root) {
        std::vector<Item> result;
        auto appendNode = [&](const auto& self, const SyntaxNode& node) -> void {
            auto appendToken = [&](const auto& tokenSelf, const Token& token) -> void {
                if (!token)
                    return;
                for (const auto& trivia : token.trivia()) {
                    switch (trivia.kind) {
                        case TriviaKind::LineComment:
                            result.push_back(
                                {ItemKind::LineComment, TokenKind::Unknown,
                                 canonicalizeComment(trivia.getRawText(), false)}
                            );
                            break;
                        case TriviaKind::BlockComment:
                            result.push_back(
                                {ItemKind::BlockComment, TokenKind::Unknown,
                                 canonicalizeComment(trivia.getRawText(), true)}
                            );
                            break;
                        case TriviaKind::DisabledText:
                            if (!std::ranges::all_of(trivia.getRawText(), isWhitespace)) {
                                result.push_back(
                                    {ItemKind::DisabledText, TokenKind::Unknown,
                                     std::string(trivia.getRawText())}
                                );
                            }
                            break;
                        case TriviaKind::Directive:
                        case TriviaKind::SkippedSyntax:
                            self(self, *trivia.syntax());
                            break;
                        case TriviaKind::SkippedTokens:
                            for (const auto& skipped : trivia.getSkippedTokens())
                                tokenSelf(tokenSelf, skipped);
                            break;
                        default:
                            break;
                    }
                }
                if (isRealToken(token)) {
                    result.push_back({ItemKind::Token, token.kind, std::string(token.valueText())});
                }
            };

            for (auto it = node.tokens_begin(); it != node.tokens_end(); ++it)
                appendToken(appendToken, *it);
        };
        appendNode(appendNode, root);
        return result;
    };

    return flatten(a) == flatten(b);
}

// Short single-line summary of a trivia item for the side-by-side view.
static std::string describeTriviaShort(const Trivia& tr) {
    std::string kindName(toString(tr.kind));
    auto truncate1 = [](std::string s, size_t n = 60) {
        // Collapse newlines so multi-line content stays on one row.
        for (auto& c : s)
            if (c == '\n')
                c = ' ';
        if (s.size() > n)
            s = s.substr(0, n) + "...";
        return s;
    };
    if (tr.kind == TriviaKind::Directive || tr.kind == TriviaKind::SkippedSyntax) {
        return kindName + " " + truncate1(tr.syntax()->toString());
    }
    auto text = tr.getRawText();
    if (text.empty())
        return kindName;
    return kindName + " '" + truncate1(std::string(text)) + "'";
}

// Render a side-by-side diff with leading shared rows, the divergence row,
// and trailing rows. The left column width is sized to the widest entry.
static std::string renderSideBySide(
    const std::vector<std::string>& leadOrig,
    const std::vector<std::string>& leadFmt,
    const std::string& divergeOrig,
    const std::string& divergeFmt,
    const std::vector<std::string>& trailOrig,
    const std::vector<std::string>& trailFmt
) {
    size_t leftWidth = std::string("original").size();
    auto widen = [&](const std::vector<std::string>& v) {
        for (auto& s : v)
            leftWidth = std::max(leftWidth, s.size());
    };
    widen(leadOrig);
    widen({std::string(">>> ") + divergeOrig});
    widen(trailOrig);
    leftWidth = std::min<size_t>(leftWidth, 120);

    auto line = [&](std::string_view left, std::string_view right) {
        std::string out(left);
        if (out.size() < leftWidth)
            out.append(leftWidth - out.size(), ' ');
        out += "  | ";
        out += right;
        out += '\n';
        return out;
    };

    std::string out;
    out += line("original", "formatted");
    out += line(std::string(leftWidth, '-'), std::string(40, '-'));
    size_t leadRows = std::max(leadOrig.size(), leadFmt.size());
    for (size_t i = 0; i < leadRows; i++) {
        out += line(i < leadOrig.size() ? leadOrig[i] : "", i < leadFmt.size() ? leadFmt[i] : "");
    }
    out += line(">>> " + divergeOrig, ">>> " + divergeFmt);
    size_t trailRows = std::max(trailOrig.size(), trailFmt.size());
    for (size_t i = 0; i < trailRows; i++) {
        out +=
            line(i < trailOrig.size() ? trailOrig[i] : "", i < trailFmt.size() ? trailFmt[i] : "");
    }
    return out;
}

// Flatten relevant trivia across a subtree into a flat sequence (matching
// the comparison order in isTokenEquivalentTo).
static std::vector<Trivia> flattenRelevantTriviaForDiff(const SyntaxNode& root) {
    std::vector<Trivia> out;
    for (auto it = root.tokens_begin(); it != root.tokens_end(); ++it) {
        Token t = *it;
        if (!t)
            continue;
        for (const auto& tr : t.trivia()) {
            if (isRelevantTrivia<true>(tr))
                out.push_back(tr);
        }
    }
    return out;
}

std::string describeTokenDiff(const SyntaxNode& a, const SyntaxNode& b) {
    constexpr size_t leadSize = 4;
    constexpr size_t trailSize = 4;

    // Phase 1: flat trivia comparison. Find the first divergence and report
    // it with side-by-side context. This catches dropped/changed/inserted
    // comments and directives — including ones on empty placeholder tokens
    // that the per-real-token loop would skip.
    {
        auto aTrivia = flattenRelevantTriviaForDiff(b); // original
        auto bTrivia = flattenRelevantTriviaForDiff(a); // formatted
        size_t i = 0;
        size_t maxLen = std::max(aTrivia.size(), bTrivia.size());
        while (i < maxLen) {
            bool aHas = i < aTrivia.size();
            bool fHas = i < bTrivia.size();
            if (aHas && fHas && isTriviaEqual<true>(aTrivia[i], bTrivia[i])) {
                i++;
                continue;
            }
            // Build context rows.
            std::vector<std::string> leadOrig, leadFmt, trailOrig, trailFmt;
            for (size_t k = i >= leadSize ? i - leadSize : 0; k < i; k++) {
                leadOrig.push_back(describeTriviaShort(aTrivia[k]));
                leadFmt.push_back(describeTriviaShort(bTrivia[k]));
            }
            std::string divOrig = aHas ? describeTriviaShort(aTrivia[i]) : "<missing>";
            std::string divFmt = fHas ? describeTriviaShort(bTrivia[i]) : "<missing>";
            for (size_t k = i + 1; k < i + 1 + trailSize && k < aTrivia.size(); k++)
                trailOrig.push_back(describeTriviaShort(aTrivia[k]));
            for (size_t k = i + 1; k < i + 1 + trailSize && k < bTrivia.size(); k++)
                trailFmt.push_back(describeTriviaShort(bTrivia[k]));

            std::string reason;
            if (!aHas)
                reason = "formatted has extra trivia (original has only " +
                         std::to_string(aTrivia.size()) + " items)";
            else if (!fHas)
                reason = "formatted dropped trivia (original has " +
                         std::to_string(aTrivia.size()) + " items, formatted " +
                         std::to_string(bTrivia.size()) + ")";
            else
                reason = "trivia differs";

            return fmt::format(
                "trivia[{}]: {}\n{}", i, reason,
                renderSideBySide(leadOrig, leadFmt, divOrig, divFmt, trailOrig, trailFmt)
            );
        }
    }

    // Phase 2: real-token comparison. Trivia already validated above, so
    // any divergence here is a kind/text mismatch on a real source token.
    auto rawOf = [](const Token& t) -> std::string_view { return t.rawText(); };
    auto joinTokens = [&](const std::vector<Token>& toks) {
        std::string out;
        for (auto& t : toks) {
            auto s = rawOf(t);
            if (s.empty())
                continue;
            if (!out.empty())
                out += ' ';
            out += s;
        }
        return out;
    };
    std::vector<Token> lead;
    auto pushLead = [&](const Token& t) {
        if (lead.size() >= leadSize)
            lead.erase(lead.begin());
        lead.push_back(t);
    };
    auto pullAfter = [&](auto& it, auto end, size_t n) {
        std::vector<Token> out;
        while (n > 0 && it != end) {
            if (isRealToken(*it)) {
                out.push_back(*it);
                --n;
            }
            ++it;
        }
        return out;
    };

    auto aIt = a.tokens_begin(), aEnd = a.tokens_end();
    auto bIt = b.tokens_begin(), bEnd = b.tokens_end();
    size_t index = 0;
    while (true) {
        while (aIt != aEnd && !isRealToken(*aIt))
            ++aIt;
        while (bIt != bEnd && !isRealToken(*bIt))
            ++bIt;
        bool aDone = aIt == aEnd;
        bool bDone = bIt == bEnd;
        if (aDone && bDone)
            return "";

        std::vector<std::string> leadRows;
        for (auto& t : lead)
            leadRows.push_back(std::string(rawOf(t)));

        auto pullText = [&](auto it, auto end) {
            std::vector<std::string> rows;
            auto v = pullAfter(it, end, trailSize);
            for (auto& t : v)
                rows.push_back(std::string(rawOf(t)));
            return rows;
        };

        if (aDone) {
            auto trailRows = pullText(bIt, bEnd);
            return fmt::format(
                "real-token[{}]: formatted ended early\n{}", index,
                renderSideBySide(
                    leadRows, leadRows, joinTokens({*bIt}), "<end of stream>", trailRows, {}
                )
            );
        }
        if (bDone) {
            auto trailRows = pullText(aIt, aEnd);
            return fmt::format(
                "real-token[{}]: original ended early\n{}", index,
                renderSideBySide(
                    leadRows, leadRows, "<end of stream>", joinTokens({*aIt}), {}, trailRows
                )
            );
        }
        if ((*aIt).kind != (*bIt).kind || (*aIt).valueText() != (*bIt).valueText()) {
            std::string reason = (*aIt).kind != (*bIt).kind ? "token kind differs"
                                                            : "token text differs";
            Token aDiverge = *aIt;
            Token bDiverge = *bIt;
            ++aIt;
            ++bIt;
            auto origTrailRows = pullText(bIt, bEnd);
            auto fmtTrailRows = pullText(aIt, aEnd);
            return fmt::format(
                "real-token[{}]: {}\n    original:  {}\n    formatted: {}\n\n{}", index, reason,
                describeToken<true>(bDiverge), describeToken<true>(aDiverge),
                renderSideBySide(
                    leadRows, leadRows, std::string(rawOf(bDiverge)), std::string(rawOf(aDiverge)),
                    origTrailRows, fmtTrailRows
                )
            );
        }
        pushLead(*aIt);
        ++aIt;
        ++bIt;
        ++index;
    }
}

} // namespace format

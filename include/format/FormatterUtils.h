//------------------------------------------------------------------------------
// FormatterUtils.h
// Shared utility helpers for the formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <string_view>
#include <unordered_set>
#include <vector>

#include "slang/parsing/Token.h"
#include "slang/text/CharInfo.h"

namespace format {

/// Find the next newline character, or return string_view::npos if none remains.
size_t findNewline(std::string_view text, size_t offset = 0);

/// Advance past one logical newline, treating CRLF as a single line ending.
inline size_t skipNewline(std::string_view text, size_t offset) {
    if (offset >= text.size() || !slang::isNewline(text[offset]))
        return offset;
    return offset + (text.substr(offset).starts_with("\r\n") ? 2 : 1);
}

/// Byte interval occupied by a macro or a literal whose internal text is opaque.
struct ProtectedTextRange {
    /// First byte of the source token or macro.
    size_t begin;
    /// One past the last byte, excluding surrounding trivia.
    size_t end;
};

/// Lex source without preprocessing so inactive macros receive the same protection.
std::vector<ProtectedTextRange> protectedTextRanges(std::string_view text);

/// Line-start offsets inside tokens, macros, or block comments with preserved whitespace.
std::unordered_set<size_t> protectedLineStarts(std::string_view text);

// The slang preprocessor rewrites LineComment/BlockComment trivia to DisabledText
// on directive tokens in untaken branches. These helpers detect comment text
// regardless of whether the trivia kind was rewritten.
[[maybe_unused]] static bool isCommentTrivia(const slang::parsing::Trivia& t) {
    if (t.kind == slang::parsing::TriviaKind::LineComment ||
        t.kind == slang::parsing::TriviaKind::BlockComment)
        return true;
    if (t.kind == slang::parsing::TriviaKind::DisabledText) {
        auto text = t.getRawText();
        return text.starts_with("//") || text.starts_with("/*");
    }
    return false;
}

[[maybe_unused]] static bool isLineCommentTrivia(const slang::parsing::Trivia& t) {
    return t.kind == slang::parsing::TriviaKind::LineComment ||
           (t.kind == slang::parsing::TriviaKind::DisabledText && t.getRawText().starts_with("//"));
}

} // namespace format

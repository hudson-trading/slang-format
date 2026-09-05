//------------------------------------------------------------------------------
// FormatterUtils.h
// Shared utility helpers for the formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "slang/parsing/Token.h"

namespace format {

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

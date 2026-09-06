//------------------------------------------------------------------------------
// FormatStyle.h
// Style decision functions for the SystemVerilog formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "slang/parsing/TokenKind.h"
#include "slang/syntax/SyntaxKind.h"

namespace format {

/// How a syntax list should be formatted.
enum class ListStyle {
    Inline,   ///< Always inline (default for non-vertical lists).
    Dynamic,  ///< Vertical when list has comments or exceeds column limit.
    Vertical, ///< Always vertical when non-empty (port lists, case items, etc.).
    Body,     ///< Always vertical, even when empty (module bodies, class bodies, etc.).
};

/// Returns the list style for a list-typed child slot of a node with the given kind.
ListStyle getListStyle(slang::syntax::SyntaxKind parentKind, size_t listIndex);

/// Returns true if the list slot of the given parent kind is an attributes list
/// that should be emitted on its own line.
bool nodeNeedsOwnLine(slang::syntax::SyntaxKind parentKind, size_t listIndex);

/// Generated: returns true for non-standard types (not Member/Statement/Expression)
/// that have an attributes list at child 0.
bool isNonstandardAttributeListSyntax(slang::syntax::SyntaxKind parentKind);

/// Determines whether whitespace should be inserted between two adjacent tokens.
/// Used to make formatting decisions like `foo(x)` vs `foo ( x )`.
/// The parent SyntaxKind of each token provides context for ambiguous tokens
/// like colons (ternary vs bit-select vs case label).
bool shouldInsertWhitespace(
    slang::parsing::TokenKind left,
    slang::parsing::TokenKind right,
    slang::syntax::SyntaxKind leftParent = slang::syntax::SyntaxKind::Unknown,
    slang::syntax::SyntaxKind rightParent = slang::syntax::SyntaxKind::Unknown,
    bool rightInDataType = false
);

} // namespace format

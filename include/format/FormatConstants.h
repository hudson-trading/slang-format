//------------------------------------------------------------------------------
// FormatConstants.h
// Formatting policy constants
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>

#include "slang/syntax/SyntaxKind.h"

namespace format::constants {

/// Maximum number of instance connections to keep on a single line.
/// Above this threshold, connections go vertical.
constexpr size_t maxInlineInstanceConnections = 2;

/// Minimum space remaining (as a multiple of indentWidth) after '=' for
/// RHS alignment to be worthwhile. If the '=' is too far right, fall back
/// to continuation-indent style.
constexpr size_t rhsAlignMinRemainingIndents = 2;

/// Break priority value for "worst break" — used to demote assignment
/// operators when equal-alignment is active.
constexpr int worstBreakPriority = 100;

/// Minimum inline width of an argument list before it can go vertical.
/// Prevents short argument lists like func(a, b, c) from splitting
/// just because the function name or LHS is long.
constexpr size_t argListMinWidthForVertical = 40;

/// Maximum width (chars) of a case-item label list before the clause is
/// forced onto its own line. Even if the full `LABELS: clause` fits in
/// the column limit, a long-label case is hard to read with the body
/// tacked on at the end; pushing the body down one indent makes the
/// label/body relationship obvious.
constexpr size_t maxInlineCaseItemLabelWidth = 30;

/// Maximum combined width (chars) of a multi-label case item before the
/// labels are split one-per-line. A case item with several labels
/// (`A, B, C:`) reads better stacked vertically once the combined label
/// text gets wide, even when it would still fit in the column limit. A
/// single label is never split by this rule.
constexpr size_t maxInlineMultiLabelWidth = 12;

/// Default separator-line count required to split an alignment group. Empty
/// lines count once; a standalone comment region contributes all but its first
/// physical line. A threshold of two means a single blank line between e.g.
/// ports, case items, or struct fields does NOT split the group. Body code
/// overrides this with `AlignConfig::groupSeparatorLines` (default 1).
constexpr int defaultGroupSeparatorLines = 2;

/// True for row kinds that represent "body" code — statements and local
/// declarations inside a function/always/initial block. These honor the user's
/// `AlignConfig::groupSeparatorLines`; all other kinds use
/// `defaultGroupSeparatorLines`.
constexpr bool isBodyAlignKind(slang::syntax::SyntaxKind kind) {
    switch (kind) {
        case slang::syntax::SyntaxKind::ExpressionStatement:
        case slang::syntax::SyntaxKind::ParameterDeclarationStatement:
        case slang::syntax::SyntaxKind::DataDeclaration:
            return true;
        default:
            return false;
    }
}

/// True for module-level procedural blocks whose direct statement owns the row.
constexpr bool isProceduralBlockAlignKind(slang::syntax::SyntaxKind kind) {
    switch (kind) {
        case slang::syntax::SyntaxKind::InitialBlock:
        case slang::syntax::SyntaxKind::FinalBlock:
        case slang::syntax::SyntaxKind::AlwaysBlock:
        case slang::syntax::SyntaxKind::AlwaysCombBlock:
        case slang::syntax::SyntaxKind::AlwaysFFBlock:
        case slang::syntax::SyntaxKind::AlwaysLatchBlock:
            return true;
        default:
            return false;
    }
}

} // namespace format::constants

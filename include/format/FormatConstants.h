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

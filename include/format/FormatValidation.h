

//------------------------------------------------------------------------------
// FormatValidation.h
// Validation utilities for the SystemVerilog formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatStage.h"
#include "format/Formatter.h"
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slang::syntax {
class SyntaxNode;
} // namespace slang::syntax

namespace format {

/// The failure or warning described by a formatter diagnostic.
enum class FormatDiagnosticKind {
    InternalError,
    StructuralImbalance,
    FailedReparse,
    CstMismatch,
    NotIdempotent,
    MergeConflict,
    UnmatchedFormatOn,
};

/// A formatter diagnostic and its eagerly rendered details.
struct FormatDiagnostic {
    /// Determines output policy and how the diagnostic is displayed.
    FormatDiagnosticKind kind;
    /// Human-readable explanation, including any syntax or text differences.
    std::string message;
    /// One-based input line, when the diagnostic identifies a specific line.
    std::optional<size_t> line;
};

/// How a caller should handle output after validation.
enum class FormatOutputAction {
    UseFormatted,
    KeepOriginal,
    Abort,
};

/// Formatted source and the diagnostics that determine whether it can be applied.
struct FormatResult {
    /// Formatted source, or unchanged input for generated files or a rendering failure.
    std::string formatted;
    /// The leading comments mark this file as @generated.
    bool generated = false;
    /// Failures and warnings, each owning its message and optional location.
    std::vector<FormatDiagnostic> diagnostics;
    /// Parser errors after filtering; recovered errors do not necessarily prevent formatting.
    size_t parseErrorCount = 0;

    /// Returns whether validation reported the given failure.
    bool hasDiagnostic(FormatDiagnosticKind kind) const;

    /// Returns true if the formatted output is safe to use without forcing.
    bool isUsable() const;

    /// Chooses output handling; force overrides all validation failures, preserving generated
    /// files.
    FormatOutputAction outputAction(bool force = false) const;
};

/// Primary formatting method; Performs extra validation
FormatResult format(
    std::string_view filename,
    std::string_view input,
    const format::Config& config,
    FormatStage stage = FormatStage::Aligned
);

/// Checks that two syntax nodes are equivalent including preprocessor directives
/// and skipped syntax.
bool isPreprocessorEquivalentTo(
    const slang::syntax::SyntaxNode& a,
    const slang::syntax::SyntaxNode& b
);

/// Checks that two syntax nodes are equivalent including preprocessor directives
/// and comments.
bool isCommentEquivalentTo(const slang::syntax::SyntaxNode& a, const slang::syntax::SyntaxNode& b);

/// Flat token-sequence equivalence: walks both trees with tokens_begin()/end()
/// and compares each real token (kind + valueText + relevant trivia) in order.
/// Insensitive to differences in CST tree shape — only the token stream matters.
/// Use this when comparing trees parsed with dontExpandMacros, because the
/// presence/positioning of unexpanded macros can change the recovered CST shape
/// without changing the real source tokens that would be compiled.
bool isTokenEquivalentTo(const slang::syntax::SyntaxNode& a, const slang::syntax::SyntaxNode& b);

/// Describes the first token-level difference between two trees. Returns an
/// empty string if equivalent.
std::string describeTokenDiff(
    const slang::syntax::SyntaxNode& a,
    const slang::syntax::SyntaxNode& b
);

/// Describe the first line that differs between two strings.
/// Returns a human-readable message with the line number and both versions.
std::string describeTextDiff(std::string_view a, std::string_view b);

/// Walks two syntax trees in parallel and returns a description of the first difference found,
/// printing the actual before/after node text. Returns an empty string if equivalent.
std::string describeCstDiff(
    const slang::syntax::SyntaxNode& node,
    const slang::syntax::SyntaxNode& other
);

} // namespace format

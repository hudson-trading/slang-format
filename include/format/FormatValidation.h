

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
#include <fmt/format.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slang::syntax {
class SyntaxNode;
} // namespace slang::syntax

namespace format {

enum class FormatDiagnosticKind {
    InternalError,
    StructuralImbalance,
    FailedReparse,
    CstMismatch,
    NotIdempotent,
};

struct FormatDiagnostic {
    FormatDiagnosticKind kind;
    std::string message;
};

struct FormatResult {
    std::string formatted;

    // Pre-rendered diagnostic messages from the parse, one per diagnostic
    // (the SourceManager is local to format(), so we render them eagerly).
    // Parse errors cascade, so display callers typically print only the
    // first few entries.
    std::vector<std::string> diagnosticMessages;

    // Orange flags based on just the original tree that tell us we maybe shouldn't use the
    // formatted output.

    size_t errorCount = 0;
    bool structuralImbalance = false;         // unclosed begin/end, {/}, etc.
    std::vector<std::string> unmatchedDelims; // location + text of each unmatched opener

    // quite rare
    bool failedReparse = false;

    // Very bad - the CST was changed and the code will get interpreted differently
    bool cstMismatch = false;

    // Description of what differed in the CST (populated when cstMismatch is true)
    std::string cstDiffMessage;

    // Idempotency failure: format(format(x)) != format(x)
    bool notIdempotent = false;

    // Description of the first difference between format(x) and format(format(x))
    std::string idempotencyDiff;

    // Internal error (e.g. exception) during formatting
    std::string internalError;

    // File was excluded from formatting (eg, a generated-file marker (`@generated`).
    bool excluded = false;

    /// Returns true if the formatted output is safe to use.
    bool isUsable() const {
        return internalError.empty() && !structuralImbalance && !failedReparse && !cstMismatch &&
               !notIdempotent;
    }

    /// Build structured diagnostics from the result fields.
    std::vector<FormatDiagnostic> diagnostics() const {
        std::vector<FormatDiagnostic> diags;
        if (!internalError.empty()) {
            diags.push_back({FormatDiagnosticKind::InternalError,
                             fmt::format("internal error: {}", internalError)});
        }
        if (structuralImbalance) {
            // Parse errors cascade — print only the first few. The
            // "unclosed delimiter" entries that the parser reports here
            // are usually a downstream symptom of these errors, not the
            // real issue, so we omit them entirely.
            constexpr size_t maxShown = 3;
            std::string msg = "parse errors (formatted output may be unsafe; skipping)";
            size_t shown = std::min(diagnosticMessages.size(), maxShown);
            for (size_t i = 0; i < shown; i++)
                msg += "\n" + diagnosticMessages[i];
            if (diagnosticMessages.size() > shown) {
                msg += fmt::format("\n... and {} more error{}", diagnosticMessages.size() - shown,
                                   diagnosticMessages.size() - shown == 1 ? "" : "s");
            }
            diags.push_back({FormatDiagnosticKind::StructuralImbalance, std::move(msg)});
        }
        if (failedReparse) {
            diags.push_back(
                {FormatDiagnosticKind::FailedReparse, "formatted output failed to reparse"});
        }
        if (cstMismatch) {
            std::string msg = "formatting changed the syntax tree";
            if (!cstDiffMessage.empty())
                msg += "\n  diff: " + cstDiffMessage;
            diags.push_back({FormatDiagnosticKind::CstMismatch, std::move(msg)});
        }
        if (notIdempotent) {
            std::string msg = "formatting is not idempotent: format(format(x)) != format(x)";
            if (!idempotencyDiff.empty())
                msg += "\n  " + idempotencyDiff;
            diags.push_back({FormatDiagnosticKind::NotIdempotent, std::move(msg)});
        }
        return diags;
    }
};

/// Primary formatting method; Performs extra validation
FormatResult format(std::string_view filename, std::string_view input, const format::Config& config,
                    FormatStage stage = FormatStage::Aligned);

/// Checks that two syntax nodes are equivalent including preprocessor directives
/// and skipped syntax.
bool isPreprocessorEquivalentTo(const slang::syntax::SyntaxNode& a,
                                const slang::syntax::SyntaxNode& b);

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
std::string describeTokenDiff(const slang::syntax::SyntaxNode& a,
                              const slang::syntax::SyntaxNode& b);

/// Describe the first line that differs between two strings.
/// Returns a human-readable message with the line number and both versions.
std::string describeTextDiff(std::string_view a, std::string_view b);

/// Walks two syntax trees in parallel and returns a description of the first difference found,
/// printing the actual before/after node text. Returns an empty string if equivalent.
std::string describeCstDiff(const slang::syntax::SyntaxNode& node,
                            const slang::syntax::SyntaxNode& other);

} // namespace format

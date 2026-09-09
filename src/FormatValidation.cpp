

//------------------------------------------------------------------------------
// FormatValidation.cpp
// SystemVerilog source code formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatValidation.h"

#include "format/Formatter.h"
#include <algorithm>
#include <fmt/format.h>

#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/diagnostics/Diagnostics.h"
#include "slang/diagnostics/ParserDiags.h"
#include "slang/diagnostics/PreprocessorDiags.h"
#include "slang/parsing/Parser.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/CharInfo.h"
#include "slang/text/SourceManager.h"

using namespace slang;
using namespace slang::parsing;
using namespace slang::syntax;

namespace format {

namespace {

struct PipelineRender {
    std::string text;
    std::string layoutText;
};

PipelineRender runPipeline(
    const SyntaxNode& root,
    const Config& config,
    FormatStage stage,
    SourceManager& sourceManager
) {
    // Both public stages start from the same normalized syntax. The aligned
    // formatter internally consumes the layout document and its selected
    // breaks; it does not reparse layout text as a formatting input.
    Formatter layoutPass(config, &sourceManager, FormatStage::Layout);
    PipelineRender rendered;
    rendered.layoutText = layoutPass.format(root);
    rendered.text = rendered.layoutText;
    if (stage == FormatStage::Layout)
        return rendered;

    Formatter alignmentPass(config, &sourceManager, FormatStage::Aligned);
    rendered.text = alignmentPass.format(root);
    return rendered;
}

} // namespace

bool FormatResult::hasDiagnostic(FormatDiagnosticKind kind) const {
    return std::ranges::any_of(diagnostics, [kind](const auto& diag) { return diag.kind == kind; });
}

FormatOutputAction FormatResult::outputAction(bool force) const {
    auto action = generated ? FormatOutputAction::KeepOriginal : FormatOutputAction::UseFormatted;
    if (force)
        return action;
    for (const auto& diag : diagnostics) {
        switch (diag.kind) {
            case FormatDiagnosticKind::MergeConflict:
            case FormatDiagnosticKind::InternalError:
            case FormatDiagnosticKind::FailedReparse:
                return FormatOutputAction::Abort;
            case FormatDiagnosticKind::StructuralImbalance:
            case FormatDiagnosticKind::CstMismatch:
            case FormatDiagnosticKind::NotIdempotent:
                action = FormatOutputAction::KeepOriginal;
                break;
        }
    }
    return action;
}

std::string describeTextDiff(std::string_view a, std::string_view b) {
    size_t lineNum = 1;
    size_t posA = 0, posB = 0;
    while (posA < a.size() && posB < b.size()) {
        auto endA = a.find('\n', posA);
        auto endB = b.find('\n', posB);
        if (endA == std::string_view::npos)
            endA = a.size();
        if (endB == std::string_view::npos)
            endB = b.size();

        auto lineA = a.substr(posA, endA - posA);
        auto lineB = b.substr(posB, endB - posB);
        if (lineA != lineB) {
            return fmt::format(
                "first difference at line {}:\n    pass 1: '{}'\n    pass 2: '{}'", lineNum,
                lineA.size() > 120 ? fmt::format("{}...", lineA.substr(0, 120)) : lineA,
                lineB.size() > 120 ? fmt::format("{}...", lineB.substr(0, 120)) : lineB
            );
        }
        posA = endA + 1;
        posB = endB + 1;
        lineNum++;
    }
    if (posA < a.size())
        return fmt::format(
            "pass 2 is shorter: pass 1 has extra content starting at line {}", lineNum
        );
    if (posB < b.size())
        return fmt::format("pass 2 is longer: extra content starting at line {}", lineNum);
    return "";
}

/// Primary formatting method; Performs extra validation
FormatResult format(
    std::string_view filename,
    std::string_view input,
    const format::Config& config,
    FormatStage stage
) {

    FormatResult result = {};

    // Git inserts markers at the start of a line, even inside comments and
    // inactive preprocessor branches. Check raw source before parsing recovery
    // can treat them as ordinary tokens or trivia.
    auto remaining = input;
    if (remaining.starts_with("\xef\xbb\xbf"))
        remaining.remove_prefix(3);
    for (size_t lineNumber = 1; !remaining.empty(); lineNumber++) {
        auto end = remaining.find_first_of("\r\n");
        auto line = remaining.substr(0, end);
        if (line.size() >= 7 &&
            (line[0] == '<' || line[0] == '=' || line[0] == '>' || line[0] == '|')) {
            auto markerEnd = line.find_first_not_of(line[0]);
            if (markerEnd == std::string_view::npos)
                markerEnd = line.size();
            auto suffix = line.substr(markerEnd);
            bool validSuffix = suffix.empty() ||
                               (line[0] == '='
                                    ? suffix.find_first_not_of(" \t") == std::string_view::npos
                                    : isTabOrSpace(suffix.front()));
            if (markerEnd >= 7 && validSuffix) {
                result.diagnostics.push_back(
                    {FormatDiagnosticKind::MergeConflict,
                     "Git merge conflict marker; resolve conflicts before formatting", lineNumber}
                );
                break;
            }
        }
        if (end == std::string_view::npos)
            break;
        size_t newlineSize = remaining[end] == '\r' && end + 1 < remaining.size() &&
                                     remaining[end + 1] == '\n'
                                 ? 2
                                 : 1;
        remaining.remove_prefix(end + newlineSize);
    }

    // Parse the source ourselves (mirroring SyntaxTree::create) so we can
    // inspect the parser's delimiter stack before it is destroyed.
    SourceManager sm;
    auto buf = sm.assignText(filename, input);
    if (!filename.empty())
        sm.addLineDirective(SourceLocation(buf.id, 0), 2, filename, 0);

    PreprocessorOptions ppOptions;
    ppOptions.maxIncludeDepth = 0;
    ppOptions.dontExpandMacros = true;
    Bag optionsBag(ppOptions);

    BumpAllocator alloc;
    Diagnostics diagnostics;
    Preprocessor preprocessor(sm, alloc, diagnostics, optionsBag);
    preprocessor.pushSource(buf);

    Parser parser(preprocessor, optionsBag);
    auto& root = parser.parseCompilationUnit();
    auto unmatchedDelims = parser.getOpenDelims();

    // Skip generated files: if the first token's leading trivia contains
    // an `@generated` marker in a line/block comment, treat the file as
    // excluded.
    if (auto firstToken = root.getFirstToken()) {
        for (const auto& trivia : firstToken.trivia()) {
            if (trivia.kind != TriviaKind::LineComment && trivia.kind != TriviaKind::BlockComment)
                continue;
            if (trivia.getRawText().find("@generated") != std::string_view::npos) {
                result.generated = true;
                result.formatted = input;
                return result;
            }
        }
    }

    diagnostics.sort(sm);

    // Filter diagnostics: skip include depth errors (we set depth to 0),
    // warnings, and context-only compilation-unit membership errors. Header
    // fragments are parsed standalone even though they can be included inside
    // a module or interface, so an otherwise fully parsed always block can be
    // diagnosed as illegal at compilation-unit scope. That does not make its
    // CST unsafe to format. Parse errors near unexpanded macros are already
    // suppressed by the parser when ignoreAllMacros is set.
    std::vector<Diagnostic> filteredDiags;
    for (auto& diag : diagnostics) {
        if (diag.code == diag::ExceededMaxIncludeDepth || diag.code == diag::NotAllowedInCU)
            continue;
        if (!diag.isError())
            continue;
        filteredDiags.push_back(diag);
    }

    result.parseErrorCount = filteredDiags.size();
    // Only flag structural imbalance when there are actual parse errors.
    // With dontExpandMacros, macros that provide closing keywords (e.g.
    // `END_MODULE -> endmodule) leave open delims but the parser suppresses
    // the diagnostics near unexpanded macros, so parseErrorCount == 0.
    if (!unmatchedDelims.empty() && !filteredDiags.empty()) {
        // Parse errors cascade; later errors and unmatched openers are usually
        // symptoms of the first few failures. Render while the SourceManager lives.
        std::string message = "cannot reliably format source with parse errors";
        size_t shown = std::min(filteredDiags.size(), size_t(3));
        for (size_t i = 0; i < shown; i++) {
            std::span<const Diagnostic> single(&filteredDiags[i], 1);
            message += "\n" + DiagnosticEngine::reportAll(sm, single);
        }
        if (filteredDiags.size() > shown) {
            message += fmt::format(
                "\n... and {} more error{}", filteredDiags.size() - shown,
                filteredDiags.size() - shown == 1 ? "" : "s"
            );
        }
        result.diagnostics.push_back(
            {FormatDiagnosticKind::StructuralImbalance, std::move(message), std::nullopt}
        );
    }

    // Run Formatter and validate CST
    try {
        auto rendered = runPipeline(root, config, stage, sm);
        result.formatted = std::move(rendered.text);

        // The layout pass is an independently meaningful output, even when
        // the caller requested alignment. Validate the boundary itself so an
        // alignment reparse cannot hide a layout-stage token/trivia bug.
        if (stage == FormatStage::Aligned) {
            auto layoutTree = SyntaxTree::fromFileInMemory(
                rendered.layoutText, sm, fmt::format("{} (layout)", filename), "", optionsBag
            );
            if (!layoutTree) {
                result.diagnostics.push_back(
                    {FormatDiagnosticKind::FailedReparse, "formatted output failed to reparse",
                     std::nullopt}
                );
                return result;
            }
            if (!isTokenEquivalentTo(layoutTree->root(), root)) {
                result.diagnostics.push_back(
                    {FormatDiagnosticKind::CstMismatch,
                     "formatting changed the syntax tree\n  diff: layout pass: " +
                         describeTokenDiff(layoutTree->root(), root),
                     std::nullopt}
                );
                return result;
            }
        }

        // Validate CST by reparsing the formatted output
        auto newTree =
            SyntaxTree::fromFileInMemory(result.formatted, sm, "formatted source", "", optionsBag);
        if (!newTree) {
            result.diagnostics.push_back(
                {FormatDiagnosticKind::FailedReparse, "formatted output failed to reparse",
                 std::nullopt}
            );
            return result;
        }
        if (!isTokenEquivalentTo(newTree->root(), root)) {
            // Use token-level equivalence rather than CST-tree equivalence:
            // dontExpandMacros lets the unexpanded macros affect the recovered
            // CST shape in ways that don't change what the real compiler sees.
            // Token-level comparison catches the actual correctness signal
            // (kind/text/relevant-trivia preservation) without false positives
            // from CST-shape drift.
            result.diagnostics.push_back(
                {FormatDiagnosticKind::CstMismatch,
                 "formatting changed the syntax tree\n  diff: " +
                     describeTokenDiff(newTree->root(), root),
                 std::nullopt}
            );
            return result;
        }

        // Idempotency check: format(format(x)) == format(x)
        auto formatted2 = Formatter(config, &sm, stage).format(newTree->root());
        if (formatted2 != result.formatted) {
            result.diagnostics.push_back(
                {FormatDiagnosticKind::NotIdempotent,
                 "formatting is not idempotent: format(format(x)) != format(x)\n  " +
                     describeTextDiff(result.formatted, formatted2),
                 std::nullopt}
            );
        }
    }
    catch (const std::exception& e) {
        result.diagnostics.push_back({FormatDiagnosticKind::InternalError, e.what(), std::nullopt});
        // An exception before rendering produced no replacement to apply, even with force.
        if (result.formatted.empty())
            result.formatted = input;
    }

    return result;
}

} // namespace format



//------------------------------------------------------------------------------
// FormatValidation.cpp
// SystemVerilog source code formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatValidation.h"

#include "format/Formatter.h"

#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/diagnostics/Diagnostics.h"
#include "slang/diagnostics/ParserDiags.h"
#include "slang/diagnostics/PreprocessorDiags.h"
#include "slang/parsing/Parser.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/syntax/SyntaxTree.h"
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

PipelineRender runPipeline(const SyntaxNode& root, const Config& config, FormatStage stage,
                           SourceManager& sourceManager) {
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
                lineB.size() > 120 ? fmt::format("{}...", lineB.substr(0, 120)) : lineB);
        }
        posA = endA + 1;
        posB = endB + 1;
        lineNum++;
    }
    if (posA < a.size())
        return fmt::format("pass 2 is shorter: pass 1 has extra content starting at line {}",
                           lineNum);
    if (posB < b.size())
        return fmt::format("pass 2 is longer: extra content starting at line {}", lineNum);
    return "";
}

/// Primary formatting method; Performs extra validation
FormatResult format(std::string_view filename, std::string_view input, const format::Config& config,
                    FormatStage stage) {

    FormatResult result = {};

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
    // excluded (same convention as Phabricator/Prettier/Black).
    if (auto firstToken = root.getFirstToken()) {
        for (const auto& trivia : firstToken.trivia()) {
            if (trivia.kind != TriviaKind::LineComment && trivia.kind != TriviaKind::BlockComment)
                continue;
            if (trivia.getRawText().find("@generated") != std::string_view::npos) {
                result.excluded = true;
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

    result.errorCount = filteredDiags.size();
    // Only flag structural imbalance when there are actual parse errors.
    // With dontExpandMacros, macros that provide closing keywords (e.g.
    // `END_MODULE -> endmodule) leave open delims but the parser suppresses
    // the diagnostics near unexpanded macros, so errorCount == 0.
    if (!unmatchedDelims.empty() && !filteredDiags.empty()) {
        result.structuralImbalance = true;
        // Render each diagnostic separately so callers can show only the
        // first few (parse errors typically cascade — the first one is the
        // useful signal, the rest is noise).
        for (auto& diag : filteredDiags) {
            std::span<const Diagnostic> single(&diag, 1);
            result.diagnosticMessages.push_back(DiagnosticEngine::reportAll(sm, single));
        }
        for (auto& tok : unmatchedDelims) {
            auto loc = sm.getFileName(tok.location());
            auto line = sm.getLineNumber(tok.location());
            auto col = sm.getColumnNumber(tok.location());
            result.unmatchedDelims.push_back(
                fmt::format("{}:{}:{}: unmatched '{}'", loc, line, col, tok.rawText()));
        }
    }

    // Run Formatter and validate CST
    try {
        auto rendered = runPipeline(root, config, stage, sm);
        result.formatted = std::move(rendered.text);

        // The layout pass is an independently meaningful output, even when
        // the caller requested alignment. Validate the boundary itself so an
        // alignment reparse cannot hide a layout-stage token/trivia bug.
        if (stage == FormatStage::Aligned) {
            auto layoutTree = SyntaxTree::fromFileInMemory(rendered.layoutText, sm,
                                                           fmt::format("{} (layout)", filename), "",
                                                           optionsBag);
            if (!layoutTree) {
                result.failedReparse = true;
                return result;
            }
            if (!isTokenEquivalentTo(layoutTree->root(), root)) {
                result.cstMismatch = true;
                result.cstDiffMessage = "layout pass: " +
                                        describeTokenDiff(layoutTree->root(), root);
                return result;
            }
        }

        // Validate CST by reparsing the formatted output
        auto newTree = SyntaxTree::fromFileInMemory(result.formatted, sm, "formatted source", "",
                                                    optionsBag);
        if (!newTree) {
            result.failedReparse = true;
        }
        else if (!isTokenEquivalentTo(newTree->root(), root)) {
            // Use token-level equivalence rather than CST-tree equivalence:
            // dontExpandMacros lets the unexpanded macros affect the recovered
            // CST shape in ways that don't change what the real compiler sees.
            // Token-level comparison catches the actual correctness signal
            // (kind/text/relevant-trivia preservation) without false positives
            // from CST-shape drift.
            result.cstMismatch = true;
            result.cstDiffMessage = describeTokenDiff(newTree->root(), root);
        }

        // Idempotency check: format(format(x)) == format(x)
        if (!result.cstMismatch && !result.failedReparse) {
            auto formatted2 = runPipeline(newTree->root(), config, stage, sm).text;
            if (formatted2 != result.formatted) {
                result.notIdempotent = true;
                result.idempotencyDiff = describeTextDiff(result.formatted, formatted2);
            }
        }
    }
    catch (const std::exception& e) {
        result.internalError = e.what();
    }

    return result;
}

} // namespace format

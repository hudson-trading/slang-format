//------------------------------------------------------------------------------
// Formatter.cpp
// SystemVerilog formatter entry point.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/Formatter.h"

#include "format/FormatDocument.h"
#include "format/FormatValidation.h"
#include "format/Layout.h"
#include "format/NormalizedFormat.h"
#include <stdexcept>

#include "slang/syntax/SyntaxNode.h"

namespace format {

Formatter::Formatter(
    const Config& config,
    const slang::SourceManager* sourceManager,
    FormatStage stage
)
    : config_(config),
      sourceManager_(sourceManager),
      stage_(stage) {
}

std::string Formatter::format(
    const slang::syntax::SyntaxNode& root,
    std::vector<FormatDiagnostic>* diagnostics
) {
    validateConfig(config_);
    auto normalized = NormalizedFormatDocument::build(root, sourceManager_);
    if (diagnostics) {
        for (size_t i = 0; i < normalized.unmatchedFormatOnCount(); i++) {
            diagnostics->push_back(
                {FormatDiagnosticKind::UnmatchedFormatOn,
                 "slang-format: on has no preceding off in the same list scope. "
                 "Did you put the off directive in the wrong scope?",
                 std::nullopt}
            );
        }
    }
    auto document = buildLayoutDocument(normalized, config_, stage_);
    DocumentRenderer renderer(config_);
    auto layout = renderer.renderLayout(document);
    if (stage_ == FormatStage::Layout)
        return layout.text;

    auto aligned = renderer.renderAligned(document, layout);
    for (auto breakId : layout.breaks) {
        if (!aligned.breaks.contains(breakId))
            throw std::runtime_error("alignment removed a layout-stage line break");
    }
    return aligned.text;
}

} // namespace format

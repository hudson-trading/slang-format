//------------------------------------------------------------------------------
// Formatter.h
// SystemVerilog formatter entry point.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatConfig.h"
#include "format/FormatStage.h"
#include <string>
#include <vector>

namespace slang {
class SourceManager;

namespace syntax {
class SyntaxNode;
}
} // namespace slang

namespace format {

struct FormatDiagnostic;

class Formatter {
public:
    explicit Formatter(
        const Config& config,
        const slang::SourceManager* sourceManager = nullptr,
        FormatStage stage = FormatStage::Aligned
    );

    /// Render source and optionally collect formatting directive warnings.
    std::string format(
        const slang::syntax::SyntaxNode& root,
        std::vector<FormatDiagnostic>* diagnostics = nullptr
    );

private:
    Config config_;
    const slang::SourceManager* sourceManager_;
    FormatStage stage_;
};

} // namespace format

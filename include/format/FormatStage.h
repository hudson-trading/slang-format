//------------------------------------------------------------------------------
// FormatStage.h
// Selects the last formatter pipeline stage to execute.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

namespace format {

enum class FormatStage {
    /// Apply spacing and line breaking, but do not align columns across rows.
    Layout,
    /// Apply the full formatter pipeline, including column alignment.
    Aligned,
};

} // namespace format

//------------------------------------------------------------------------------
// Layout.h
// Lower normalized syntax into formatter document IR.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatConfig.h"
#include "format/FormatDocument.h"
#include "format/NormalizedFormat.h"

namespace format {

FormatDocument buildLayoutDocument(const NormalizedFormatDocument& normalized,
                                   const Config& config);

} // namespace format

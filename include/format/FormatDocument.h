//------------------------------------------------------------------------------
// FormatDocument.h
// Formatter layout IR and pure rendering interfaces.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatConfig.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "slang/syntax/SyntaxKind.h"

namespace format {

using DocId = uint32_t;
using BreakId = uint32_t;
using GroupId = uint32_t;
using AnchorId = uint32_t;
using AlignmentId = uint32_t;
using AlignmentGroupId = uint32_t;
using MemberId = uint32_t;

enum class DocKind {
    Empty,
    Text,
    Verbatim,
    MemberVerbatim,
    AbsoluteText,
    SoftLine,
    HardLine,
    Concat,
    Indent,
    IndentIfBreak,
    RelativeAnchor,
    ConsistentGroup,
    AlignmentAnchor,
    Member,
};

struct DocNode {
    DocKind kind = DocKind::Empty;
    std::string text;
    std::vector<DocId> children;
    int value = 0;
    uint32_t id = 0;
    GroupId group = 0;
    AlignmentGroupId alignmentGroup = 0;
    uint32_t width = 0;
    bool global = false;
    slang::syntax::SyntaxKind syntaxKind = slang::syntax::SyntaxKind::Unknown;
};

struct FormatDocument {
    std::vector<DocNode> nodes;
    DocId root = 0;
};

class DocumentBuilder {
public:
    DocumentBuilder();

    DocId empty();
    DocId text(std::string_view value,
               slang::syntax::SyntaxKind kind = slang::syntax::SyntaxKind::Unknown);
    DocId verbatim(std::string_view value);
    DocId memberVerbatim(std::string_view value, int indent = 0);
    DocId absoluteText(std::string_view value);
    DocId softLine(int priority, std::string_view flatText = " ", GroupId group = 0,
                   bool global = false);
    DocId hardLine(int count = 1, bool useAnchor = false);
    DocId concat(std::vector<DocId> children);
    DocId indent(int columns, DocId child);
    DocId indentIfBreak(DocId line, int columns, DocId child);
    DocId relativeAnchor(int offset, DocId child, size_t fitWidth = 0);
    DocId consistentGroup(DocId child);
    GroupId createConsistentGroup();
    AlignmentGroupId createAlignmentGroup();
    DocId alignmentAnchor(uint32_t column, slang::syntax::SyntaxKind rowKind,
                          AlignmentGroupId group, uint32_t minimumPadding = 0);
    DocId member(DocId child, slang::syntax::SyntaxKind kind);

    FormatDocument finish(DocId root) &&;

private:
    DocId add(DocNode node);

    FormatDocument document_;
    BreakId nextBreak_ = 1;
    GroupId nextGroup_ = 1;
    AnchorId nextAnchor_ = 1;
    AlignmentId nextAlignment_ = 1;
    AlignmentGroupId nextAlignmentGroup_ = 1;
    MemberId nextMember_ = 1;
};

struct RenderedBreak {
    BreakId id = 0;
    size_t outputOffset = 0;
};

struct RenderedAlignmentAnchor {
    AlignmentId id = 0;
    uint32_t columnIndex = 0;
    slang::syntax::SyntaxKind rowKind = slang::syntax::SyntaxKind::Unknown;
    AlignmentGroupId group = 0;
    MemberId member = 0;
    size_t line = 0;
    size_t column = 0;
    size_t outputOffset = 0;
    size_t minimumPadding = 0;
};

struct RenderedDocument {
    std::string text;
    std::unordered_set<BreakId> breaks;
    std::vector<RenderedBreak> renderedBreaks;
    std::vector<RenderedAlignmentAnchor> alignmentAnchors;
};

class DocumentRenderer {
public:
    explicit DocumentRenderer(const Config& config) : config_(config) {}

    RenderedDocument renderLayout(const FormatDocument& document) const;
    RenderedDocument renderAligned(const FormatDocument& document,
                                   const RenderedDocument& layout) const;

private:
    const Config& config_;
};

} // namespace format

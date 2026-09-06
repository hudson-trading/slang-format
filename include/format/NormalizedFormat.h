//------------------------------------------------------------------------------
// NormalizedFormat.h
// Lossless formatter-owned representation of a slang syntax tree.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatStyle.h"
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "slang/parsing/Token.h"
#include "slang/syntax/SyntaxKind.h"

namespace slang {
class SourceManager;

namespace syntax {
class SyntaxNode;
}
} // namespace slang

namespace format {

class NormalizedDocumentBuilder;

enum class NormalizedTriviaKind {
    BlankLine,
    Comment,
    Directive,
    ConditionalDirective,
    ConditionalBranch,
    MacroUsage,
    Verbatim,
};

enum class TriviaPlacement {
    Standalone,
    Trailing,
    Inline,
};

struct NormalizedTrivia {
    NormalizedTriviaKind kind = NormalizedTriviaKind::Verbatim;
    TriviaPlacement placement = TriviaPlacement::Standalone;
    std::string text;
    const slang::syntax::SyntaxNode* syntax = nullptr;
    bool lineComment = false;
    bool joinsFollowingToken = false;
    bool preserveSingleSpace = false;
    bool endsLine = false;
    int conditionalDepthChange = 0;
    size_t lineBreakCount = 2;
};

struct NormalizedToken {
    slang::parsing::Token token;
    slang::syntax::SyntaxKind parentKind = slang::syntax::SyntaxKind::Unknown;
    size_t syntaxDepth = 0;
    bool inDataType = false;
    bool fromMacroExpansion = false;
    bool startsListItem = false;
    bool preservesBlankBefore = false;
    bool followsPreservedList = false;
    bool lineBreakBefore = false;
    std::vector<NormalizedTrivia> leading;
    std::vector<NormalizedTrivia> trailing;
};

struct NormalizedNode;
struct NormalizedList;

struct NormalizedChild {
    using Value =
        std::variant<size_t, std::unique_ptr<NormalizedNode>, std::unique_ptr<NormalizedList>>;
    Value value;

    explicit NormalizedChild(size_t tokenIndex)
        : value(tokenIndex) {}
    explicit NormalizedChild(std::unique_ptr<NormalizedNode> node)
        : value(std::move(node)) {}
    explicit NormalizedChild(std::unique_ptr<NormalizedList> list)
        : value(std::move(list)) {}

    NormalizedChild(NormalizedChild&&) noexcept = default;
    NormalizedChild& operator=(NormalizedChild&&) noexcept = default;
    NormalizedChild(const NormalizedChild&) = delete;
    NormalizedChild& operator=(const NormalizedChild&) = delete;
};

struct NormalizedNode {
    slang::syntax::SyntaxKind kind = slang::syntax::SyntaxKind::Unknown;
    const slang::syntax::SyntaxNode* syntax = nullptr;
    size_t depth = 0;
    bool verbatim = false;
    std::string verbatimText;
    std::vector<NormalizedTrivia> leading;
    std::vector<NormalizedChild> children;
};

struct NormalizedList {
    slang::syntax::SyntaxKind parentKind = slang::syntax::SyntaxKind::Unknown;
    size_t listIndex = 0;
    ListStyle style = ListStyle::Inline;
    std::vector<NormalizedChild> children;
};

struct NormalizedConditional;

struct NormalizedConditionalBranch {
    const slang::syntax::SyntaxNode* directive = nullptr;
    std::vector<std::variant<size_t, std::unique_ptr<NormalizedConditional>>> contents;
};

struct NormalizedConditional {
    std::vector<NormalizedConditionalBranch> branches;
    bool terminated = false;
};

class NormalizedFormatDocument {
public:
    static NormalizedFormatDocument build(
        const slang::syntax::SyntaxNode& root,
        const slang::SourceManager* sourceManager
    );

    NormalizedFormatDocument(NormalizedFormatDocument&&) noexcept = default;
    NormalizedFormatDocument& operator=(NormalizedFormatDocument&&) noexcept = default;
    NormalizedFormatDocument(const NormalizedFormatDocument&) = delete;
    NormalizedFormatDocument& operator=(const NormalizedFormatDocument&) = delete;

    const NormalizedNode& root() const { return *root_; }
    const std::vector<NormalizedToken>& tokens() const { return tokens_; }
    const std::vector<std::unique_ptr<NormalizedConditional>>& conditionals() const {
        return conditionals_;
    }

private:
    friend class NormalizedDocumentBuilder;
    NormalizedFormatDocument() = default;

    std::unique_ptr<NormalizedNode> root_;
    std::vector<NormalizedToken> tokens_;
    std::vector<std::unique_ptr<NormalizedConditional>> conditionals_;
};

} // namespace format

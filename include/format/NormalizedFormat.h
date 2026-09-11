//------------------------------------------------------------------------------
// NormalizedFormat.h
// Lossless formatter-owned representation of a slang syntax tree.
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "format/FormatConfig.h"
#include "format/FormatStyle.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
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

/// Signals that formatting must preserve the input instead of entering deep recursion.
class FormatDepthLimitError : public std::runtime_error {
public:
    /// Describe the resource limit without implying malformed source.
    explicit FormatDepthLimitError(size_t limit)
        : std::runtime_error(
              "syntax depth limit (" + std::to_string(limit) + ") exceeded; skipping formatting"
          ) {}
};

/// Check syntax depth iteratively before any recursive formatter traversal.
void checkSyntaxDepth(const slang::syntax::SyntaxNode& root, size_t limit);

class NormalizedDocumentBuilder;

enum class NormalizedTriviaKind {
    BlankLine,
    Comment,
    Directive,
    ConditionalDirective,
    ConditionalBranch,
    MacroUsage,
    Verbatim,
    /// Source inside an off region that must not be reparsed or reformatted.
    Unformatted,
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
    /// First real token of a preserved off/on region, bypassing empty attributes.
    std::optional<size_t> verbatimFirstToken;
    /// Last token whose trailing trivia follows a preserved off/on region.
    std::optional<size_t> verbatimLastToken;
    std::vector<NormalizedTrivia> leading;
    std::vector<NormalizedChild> children;
};

struct NormalizedList {
    slang::syntax::SyntaxKind parentKind = slang::syntax::SyntaxKind::Unknown;
    size_t listIndex = 0;
    ListStyle style = ListStyle::Inline;
    /// Token immediately after this list; its leading comments can close an off region.
    size_t endTokenIndex = 0;
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
    /// Normalize a syntax tree after checking it against the recursion budget.
    static NormalizedFormatDocument build(
        const slang::syntax::SyntaxNode& root,
        const slang::SourceManager* sourceManager,
        size_t maxSyntaxDepth = defaultMaxSyntaxDepth
    );

    NormalizedFormatDocument(NormalizedFormatDocument&&) noexcept = default;
    NormalizedFormatDocument& operator=(NormalizedFormatDocument&&) noexcept = default;
    NormalizedFormatDocument(const NormalizedFormatDocument&) = delete;
    NormalizedFormatDocument& operator=(const NormalizedFormatDocument&) = delete;

    const NormalizedNode& root() const { return *root_; }
    const std::vector<NormalizedToken>& tokens() const { return tokens_; }
    /// Number of on markers without a preceding off in the same list scope.
    size_t unmatchedFormatOnCount() const { return unmatchedFormatOnCount_; }
    const std::vector<std::unique_ptr<NormalizedConditional>>& conditionals() const {
        return conditionals_;
    }

private:
    friend class NormalizedDocumentBuilder;
    NormalizedFormatDocument() = default;

    std::unique_ptr<NormalizedNode> root_;
    std::vector<NormalizedToken> tokens_;
    /// Unmatched on markers found while applying list-scoped formatting regions.
    size_t unmatchedFormatOnCount_ = 0;
    std::vector<std::unique_ptr<NormalizedConditional>> conditionals_;
};

} // namespace format

// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT

#include "format/FormatterUtils.h"
#include "format/NormalizedFormat.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

using namespace slang;
using namespace slang::syntax;

namespace {

SourceManager sourceManager;

std::shared_ptr<SyntaxTree> parse(std::string_view text) {
    parsing::PreprocessorOptions options;
    options.maxIncludeDepth = 0;
    options.dontExpandMacros = true;
    return SyntaxTree::fromText(text, sourceManager, "normalized_test", "", options);
}

const format::NormalizedTrivia* findTrivia(
    const format::NormalizedFormatDocument& document,
    std::string_view text,
    bool trailing
) {
    for (const auto& token : document.tokens()) {
        const auto& trivia = trailing ? token.trailing : token.leading;
        auto it = std::ranges::find_if(trivia, [&](const auto& item) {
            return item.text.find(text) != std::string::npos;
        });
        if (it != trivia.end())
            return &*it;
    }
    return nullptr;
}

bool hasToken(const format::NormalizedFormatDocument& document, std::string_view text) {
    return std::ranges::any_of(document.tokens(), [&](const auto& token) {
        return token.token && token.token.rawText() == text;
    });
}

bool hasVerbatimNode(const format::NormalizedNode& node, std::string_view text) {
    if (node.verbatim && node.verbatimText.find(text) != std::string::npos)
        return true;
    for (const auto& child : node.children) {
        if (auto nested = std::get_if<std::unique_ptr<format::NormalizedNode>>(&child.value)) {
            if (hasVerbatimNode(**nested, text))
                return true;
        }
        else if (auto list = std::get_if<std::unique_ptr<format::NormalizedList>>(&child.value)) {
            for (const auto& item : (*list)->children) {
                if (auto nested =
                        std::get_if<std::unique_ptr<format::NormalizedNode>>(&item.value)) {
                    if (hasVerbatimNode(**nested, text))
                        return true;
                }
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE("protected line starts include block comment trivia at EOF") {
    for (std::string_view suffix : {"", "\nlogic a;\n"}) {
        std::string input = "logic b;\r\n/* first\r\n  second\n*/";
        input += suffix;
        auto offsets = format::protectedLineStarts(input);
        CHECK(offsets.size() == 2);
        CHECK(offsets.contains(input.find("  second")));
        CHECK(offsets.contains(input.find("*/")));
    }
}

TEST_CASE("normalized comments have explicit trailing and standalone owners") {
    auto tree = parse(R"(
module m;
    logic a; // trailing comment
    // standalone comment
    logic b;
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto trailing = findTrivia(document, "trailing comment", true);
    REQUIRE(trailing);
    CHECK(trailing->kind == format::NormalizedTriviaKind::Comment);
    CHECK(trailing->placement == format::TriviaPlacement::Trailing);

    auto standalone = findTrivia(document, "standalone comment", false);
    REQUIRE(standalone);
    CHECK(standalone->kind == format::NormalizedTriviaKind::Comment);
    CHECK(standalone->placement == format::TriviaPlacement::Standalone);
}

TEST_CASE("normalized conditionals nest active branches and preserve inactive text") {
    auto tree = parse(R"(
module m;
`ifdef OUTER
    logic hidden;
`else
    logic visible;
`ifdef INNER
    logic inner_hidden;
`else
    logic inner_visible;
`endif
`endif
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    REQUIRE(document.conditionals().size() == 1);
    const auto& outer = *document.conditionals().front();
    REQUIRE(outer.branches.size() == 2);
    CHECK(outer.terminated);

    const format::NormalizedConditional* nested = nullptr;
    for (const auto& content : outer.branches[1].contents) {
        if (auto conditional =
                std::get_if<std::unique_ptr<format::NormalizedConditional>>(&content)) {
            nested = conditional->get();
            break;
        }
    }
    REQUIRE(nested);
    CHECK(nested->branches.size() == 2);
    CHECK(nested->terminated);

    CHECK(hasToken(document, "visible"));
    CHECK(hasToken(document, "inner_visible"));
    CHECK(!hasToken(document, "hidden"));
    auto opaque = findTrivia(document, "logic hidden", false);
    REQUIRE(opaque);
    CHECK(opaque->kind == format::NormalizedTriviaKind::ConditionalBranch);
}

TEST_CASE("normalized recovery text has a verbatim owner") {
    auto tree = parse(R"(
module m;
    CUSTOM_ASSIGN(data_t, value) = source;
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto verbatim = findTrivia(document, "=", false);
    if (!verbatim)
        verbatim = findTrivia(document, "=", true);
    REQUIRE(verbatim);
    CHECK(verbatim->kind == format::NormalizedTriviaKind::Verbatim);
}

TEST_CASE("normalized recovery retains its boundary with the parsed token") {
    auto tree = parse(R"(virtual task run();
endtask
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto verbatim = findTrivia(document, "virtual", false);
    REQUIRE(verbatim);
    CHECK(verbatim->kind == format::NormalizedTriviaKind::Verbatim);
    CHECK(verbatim->preserveSingleSpace);
}

TEST_CASE("normalized foreign template documents are verbatim") {
    auto tree = parse(R"(parameter int WIDTH=8;
% if feature_enabled:
logic enabled;
% else:
logic disabled;
% endif
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    CHECK(document.root().verbatim);
    CHECK(document.root().verbatimText.find("% if feature_enabled:") != std::string::npos);
}

TEST_CASE("a macro followed by member access stays joined") {
    auto tree = parse(R"(
class c;
    `define HANDLE state
    task run();
        if (enabled) `HANDLE.value = '0;
    endtask
endclass
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto macro = findTrivia(document, "`HANDLE", false);
    if (!macro)
        macro = findTrivia(document, "`HANDLE", true);
    REQUIRE(macro);
    CHECK(macro->kind == format::NormalizedTriviaKind::MacroUsage);
    CHECK(macro->joinsFollowingToken);
}

TEST_CASE("adjacent macro hierarchy components stay joined") {
    auto tree = parse(R"(
`define ROOT top
`define BRANCH unit
module m;
    initial $sample(0, `ROOT.`BRANCH.state);
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    for (auto text : {"`ROOT", "`BRANCH"}) {
        auto macro = findTrivia(document, text, false);
        if (!macro)
            macro = findTrivia(document, text, true);
        REQUIRE(macro);
        CHECK(macro->kind == format::NormalizedTriviaKind::MacroUsage);
        CHECK(macro->joinsFollowingToken);
    }
}

TEST_CASE("a macro argument retains its following comma") {
    auto tree = parse(R"(
`define READ(VALUE) VALUE
module example;
    function void update();
        result = calculate(current, `READ(saved), replacement);
    endfunction
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto macro = findTrivia(document, "`READ", false);
    if (!macro)
        macro = findTrivia(document, "`READ", true);
    REQUIRE(macro);
    CHECK(macro->kind == format::NormalizedTriviaKind::MacroUsage);
    CHECK(macro->placement == format::TriviaPlacement::Inline);
    CHECK(macro->joinsFollowingToken);
}

TEST_CASE("standalone macro text excludes source indentation") {
    auto tree = parse(R"(class example;
   item_t items[ITEM_COUNT];
   config_t cfg;

      `REGISTER_TYPE(example)

   function new(string name = "");
      super.new(name);
   endfunction
endclass
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    auto macro = findTrivia(document, "`REGISTER_TYPE(example)", false);
    REQUIRE(macro);
    CHECK(macro->text == "`REGISTER_TYPE(example)");
    CHECK(macro->kind == format::NormalizedTriviaKind::MacroUsage);
    CHECK(macro->placement == format::TriviaPlacement::Standalone);
}

TEST_CASE("slang-format skip marks one normalized member verbatim") {
    auto tree = parse(R"(
module m;
    // slang-format: skip
    localparam int VALUE = source + offset;
    localparam int OTHER = 1;
endmodule
)");
    auto document = format::NormalizedFormatDocument::build(tree->root(), &sourceManager);

    CHECK(hasVerbatimNode(document.root(), "localparam int VALUE = source + offset;"));
    CHECK(!hasVerbatimNode(document.root(), "localparam int OTHER = 1;"));
}

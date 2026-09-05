// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT

#include "format/FormatValidation.h"
#include <catch2/catch_test_macros.hpp>

#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

using namespace slang;
using namespace slang::syntax;

namespace {

SourceManager sm;

std::shared_ptr<SyntaxTree> parse(std::string_view text) {
    parsing::PreprocessorOptions ppOpts;
    ppOpts.maxIncludeDepth = 0;
    return SyntaxTree::fromText(text, sm, "test", "", ppOpts);
}

} // namespace

TEST_CASE("isPreprocessorEquivalentTo detects directive differences") {
    auto tree1 = parse(R"(
`define FOO
module m;
    int i = 1;
endmodule
)");
    auto tree2 = parse(R"(
`define BAR
module m;
    int i = 1;
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(!format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isPreprocessorEquivalentTo matches identical directives") {
    auto tree1 = parse(R"(
`define FOO
module m;
    int i = 1;
endmodule
)");
    auto tree2 = parse(R"(
`define FOO
module m;
    int i = 1;
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isPreprocessorEquivalentTo detects ifdef differences") {
    auto tree1 = parse(R"(
`define FOO
module m;
`ifdef FOO
    int i = 1;
`endif
endmodule
)");
    auto tree2 = parse(R"(
`define BAR
module m;
`ifdef BAR
    int i = 1;
`endif
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(!format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isPreprocessorEquivalentTo ignores whitespace and comment differences") {
    auto tree1 = parse(R"(
`define FOO
module m;
    int i = 1; // a comment
    int j = 2;
endmodule
)");
    auto tree2 = parse(R"(
`define FOO
module m;
    int   i   =   1;
    int j=2; /* different comment style */
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isPreprocessorEquivalentTo detects different include filenames") {
    auto tree1 = parse(R"(
module m;
`include "foo.svh"
    int i = 1;
endmodule
)");
    auto tree2 = parse(R"(
module m;
`include "bar.svh"
    int i = 1;
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(!format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isPreprocessorEquivalentTo matches same include filenames") {
    auto tree1 = parse(R"(
module m;
`include "foo.svh"
    int i = 1;
endmodule
)");
    auto tree2 = parse(R"(
module m;
`include "foo.svh"
    int i = 1;
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo detects different line comments") {
    auto tree1 = parse(R"(
module m;
    int i = 1; // comment A
endmodule
)");
    auto tree2 = parse(R"(
module m;
    int i = 1; // comment B
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
    CHECK(!format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo detects different block comments") {
    auto tree1 = parse(R"(
module m;
    int i = 1; /* block A */
endmodule
)");
    auto tree2 = parse(R"(
module m;
    int i = 1; /* block B */
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
    CHECK(!format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo matches identical comments") {
    auto tree1 = parse(R"(
module m;
    int i = 1; // same comment
endmodule
)");
    auto tree2 = parse(R"(
module m;
    int i = 1; // same comment
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
    CHECK(format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo detects added comment") {
    auto tree1 = parse(R"(
module m;
    int i = 1;
endmodule
)");
    auto tree2 = parse(R"(
module m;
    int i = 1; // new comment
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
    CHECK(!format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo ignores whitespace differences") {
    auto tree1 = parse(R"(
module m;
    int i = 1; // same
    int   j  =  2;
endmodule
)");
    auto tree2 = parse(R"(
module m;
    int   i   =   1; // same
    int j=2;
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("isCommentEquivalentTo also checks preprocessor directives") {
    auto tree1 = parse(R"(
`define FOO
module m;
    int i = 1; // same comment
endmodule
)");
    auto tree2 = parse(R"(
`define BAR
module m;
    int i = 1; // same comment
endmodule
)");

    CHECK(tree1->root().isEquivalentTo(tree2->root()));
    CHECK(!format::isPreprocessorEquivalentTo(tree1->root(), tree2->root()));
    CHECK(!format::isCommentEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("format accepts parsed members that need an include context") {
    format::Config config;
    auto result = format::format("fragment.svh", R"(
always_comb begin
lhs=rhs;
end

always_ff @(posedge clk) begin
registered<=lhs;
end
)",
                                 config);

    CHECK(result.errorCount == 0);
    CHECK(!result.structuralImbalance);
    CHECK(result.isUsable());
    CHECK(result.formatted.find("    lhs = rhs;") != std::string::npos);
    CHECK(result.formatted.find("    registered <= lhs;") != std::string::npos);
}

TEST_CASE("format retains parser errors unrelated to compilation-unit context") {
    format::Config config;
    auto result = format::format("broken_fragment.svh", R"(
always_comb begin
lhs = ;
)",
                                 config);

    CHECK(result.errorCount > 0);
}

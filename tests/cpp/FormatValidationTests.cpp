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

TEST_CASE("comment equivalence ignores formatter-owned horizontal whitespace") {
    auto tree1 = parse(
        "module m;\n"
        "    // heading   \n"
        "    //   \n"
        "    /* first line  \n"
        "     * second line\t\n"
        "     */\n"
        "endmodule\n"
    );
    auto tree2 = parse(
        "module m;\n"
        "    // heading\n"
        "    //\n"
        "    /* first line\n"
        "         * second line\n"
        "         */\n"
        "endmodule\n"
    );

    CHECK(format::isCommentEquivalentTo(tree1->root(), tree2->root()));
    CHECK(format::isTokenEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("comment equivalence preserves non-layout comment content") {
    auto tree1 = parse(R"(
module m;
    // heading A
    /* body A */
endmodule
)");
    auto tree2 = parse(R"(
module m;
    // heading B
    /* body B */
endmodule
)");

    CHECK(!format::isCommentEquivalentTo(tree1->root(), tree2->root()));
    CHECK(!format::isTokenEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("token equivalence ignores disabled branch indentation") {
    auto tree1 = parse(R"(
module m;
`ifdef FEATURE
logic selected;
`else
        logic fallback;
`endif
endmodule
)");
    auto tree2 = parse(R"(
module m;
`ifdef FEATURE
    logic selected;
`else
    logic fallback;
`endif
endmodule
)");

    CHECK(format::isTokenEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("token equivalence detects disabled branch content changes") {
    auto tree1 = parse(R"(
module m;
`ifdef FEATURE
    logic selected;
`else
    logic fallback_a;
`endif
endmodule
)");
    auto tree2 = parse(R"(
module m;
`ifdef FEATURE
    logic selected;
`else
    logic fallback_b;
`endif
endmodule
)");

    CHECK(!format::isTokenEquivalentTo(tree1->root(), tree2->root()));
}

TEST_CASE("token equivalence preserves comment order around recovered macro items") {
    auto tree1 = parse(R"(
package p;
    typedef enum {
        first,
        `INCLUDE_ITEMS("items.svh")
        last // final item
    } item_t;
endpackage
)");
    auto tree2 = parse(R"(
package p;
    typedef enum {
        first,
        `INCLUDE_ITEMS("items.svh") // final item
        last
    } item_t;
endpackage
)");

    CHECK(!format::isTokenEquivalentTo(tree1->root(), tree2->root()));
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
    auto result = format::format(
        "fragment.svh",
        R"(
always_comb begin
lhs=rhs;
end

always_ff @(posedge clk) begin
registered<=lhs;
end
)",
        config
    );

    CHECK(result.errorCount == 0);
    CHECK(!result.structuralImbalance);
    CHECK(result.isUsable());
    CHECK(result.formatted.find("    lhs = rhs;") != std::string::npos);
    CHECK(result.formatted.find("    registered <= lhs;") != std::string::npos);
}

TEST_CASE("format retains parser errors unrelated to compilation-unit context") {
    format::Config config;
    auto result = format::format(
        "broken_fragment.svh",
        R"(
always_comb begin
lhs = ;
)",
        config
    );

    CHECK(result.errorCount > 0);
}

TEST_CASE("generated source remains safe to apply unchanged") {
    std::string input = "// @generated\r\nmodule foo; endmodule";
    auto result = format::format("generated.sv", input, {});
    CHECK(result.excluded);
    CHECK(result.isUsable());
    CHECK(result.formatted == input);
}

TEST_CASE("multiline string whitespace survives formatting in either branch") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (bool inactive : {false, true}) {
            std::string literal = "\"\"\"first  \n  second\t\nthird\"\"\"";
            std::string input = "module foo;\n";
            if (inactive)
                input += "`ifdef FEATURE\n";
            input += "string s = " + literal + ";\n";
            if (inactive)
                input += "`endif\n";
            input += "endmodule\n";
            auto result = format::format("literal.sv", input, {}, stage);
            INFO(result.cstDiffMessage);
            INFO(result.idempotencyDiff);
            CHECK(result.isUsable());
            CHECK(result.formatted.find(literal) != std::string::npos);
        }
    }
}

TEST_CASE("macro token equivalence preserves internal whitespace") {
    parsing::PreprocessorOptions options;
    options.maxIncludeDepth = 0;
    options.dontExpandMacros = true;
    for (auto prefix : {std::string(""), std::string("`ifdef FEATURE\n")}) {
        INFO(prefix);
        std::string suffix = prefix.empty() ? "" : "`endif\n";
        for (auto body :
             {std::string("`define TEXT `\"a  b`\"\n"), std::string("`STRINGIFY(a  b)\n")}) {
            std::string original = prefix + body + suffix;
            body.replace(body.find("  "), 2, " ");
            auto a = SyntaxTree::fromText(original, sm, "original", "", options);
            auto b = SyntaxTree::fromText(prefix + body + suffix, sm, "changed", "", options);
            CHECK_FALSE(format::isTokenEquivalentTo(a->root(), b->root()));
            CHECK(
                format::describeTokenDiff(b->root(), a->root()).find("protected region") !=
                std::string::npos
            );
        }
    }
}

TEST_CASE("macro token equivalence preserves blank lines within arguments") {
    parsing::PreprocessorOptions options;
    options.dontExpandMacros = true;
    std::string original = "module foo; initial `STRINGIFY(first\n\nsecond); endmodule\n";
    auto a = SyntaxTree::fromText(original, sm, "original", "", options);
    original.erase(original.find("\n\n"), 1);
    auto b = SyntaxTree::fromText(original, sm, "changed", "", options);
    CHECK_FALSE(format::isTokenEquivalentTo(a->root(), b->root()));
    CHECK_FALSE(format::describeTokenDiff(b->root(), a->root()).empty());
}

TEST_CASE("formatting preserves expanded macro string values") {
    std::string input =
        "`define TEXT `\"first \\\n                     second`\"\n"
        "`define STRINGIFY(x) `\"x`\"\n"
        "module foo; initial $display(`TEXT);\n"
        "`ifdef FEATURE\ninitial $display(`STRINGIFY(first\n                      second));\n"
        "`endif\nendmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("macro.sv", input, {}, stage);
        REQUIRE(result.isUsable());
        for (bool enabled : {false, true}) {
            parsing::PreprocessorOptions options;
            if (enabled)
                options.predefines.emplace_back("FEATURE");
            auto original = SyntaxTree::fromText(input, sm, "original", "", options);
            auto formatted = SyntaxTree::fromText(result.formatted, sm, "formatted", "", options);
            REQUIRE(original->diagnostics().empty());
            REQUIRE(formatted->diagnostics().empty());
            CHECK(original->root().isEquivalentTo(formatted->root()));
        }
    }
}

TEST_CASE("alignment padding cap starts a new group for later short rows") {
    format::Config config;
    config.alignment.value().maxSpaces = 4;
    auto result = format::format(
        "alignment.sv", "module foo; initial begin long_name = 1; aa = 2; b = 3; end endmodule\n",
        config
    );
    INFO(result.idempotencyDiff);
    CHECK(result.isUsable());
    CHECK(result.formatted.find("aa = 2;\n        b  = 3;") != std::string::npos);
    config.alignment.value().maxSpaces = std::nullopt;
    result = format::format(
        "alignment.sv", "module foo; initial begin long_name = 1; aa = 2; b = 3; end endmodule\n",
        config
    );
    CHECK(result.isUsable());
    CHECK(result.formatted.find("aa        = 2;\n        b         = 3;") != std::string::npos);
}

TEST_CASE("malformed source keeps parser diagnostics") {
    for (auto [invalid, valid] :
         {std::pair{
              "module m; initial if (i inside [0:3]) a = 1; endmodule",
              "module m; initial if (i inside {[0:3]}) a = 1; endmodule"
          },
          std::pair{
              "class c; constraint bounds {i inside [0:3];} endclass",
              "class c; constraint bounds {i inside {[0:3]};} endclass"
          },
          std::pair{
              "module m; initial assert(std::randomize(i) with {i inside [0:3];}); endmodule",
              "module m; initial assert(std::randomize(i) with {i inside {[0:3]};}); endmodule"
          },
          std::pair{
              "module m; logic [1:0][3:0] a = {2{4{1'b1}}}; endmodule",
              "module m; logic [1:0][3:0] a = {2{{4{1'b1}}}}; endmodule"
          },
          std::pair{
              "module m; assert property (!|a); endmodule",
              "module m; assert property (!(|a)); endmodule"
          }}) {
        INFO(invalid);
        for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
            auto malformed = format::format("invalid.sv", invalid, {}, stage);
            CHECK(malformed.errorCount > 0);
            auto accepted = format::format("valid.sv", valid, {}, stage);
            INFO(accepted.cstDiffMessage);
            CHECK(accepted.errorCount == 0);
            CHECK(accepted.isUsable());
        }
    }
}

TEST_CASE("named packed array after a class macro") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format(
            "named.sv",
            "class foo extends base;\n`REGISTER(foo)\nbyte_t [6:0] data;\nbyte_t next;\nendclass\n",
            {}, stage
        );
        CHECK(result.errorCount == 0);
        CHECK(result.isUsable());
    }
}

TEST_CASE("complete configs remain formattable") {
    std::string input = "config foo;design work.top;instance top.dut liblist impl;endconfig\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("config.sv", input, {}, stage);
        CHECK(result.errorCount == 0);
        CHECK(result.isUsable());
        CHECK(result.formatted != input);
    }
}

TEST_CASE("source item diagnostics identify changed equality tokens") {
    auto original = parse("module foo; initial a = b == c; endmodule");
    auto changed = parse("module foo; initial a = b = c; endmodule");
    CHECK_FALSE(format::isTokenEquivalentTo(original->root(), changed->root()));
    auto diff = format::describeTokenDiff(changed->root(), original->root());
    CHECK(diff.find("source item[") != std::string::npos);
    CHECK(diff.find("original: DoubleEquals '=='") != std::string::npos);
    CHECK(diff.find("formatted: Equals '='") != std::string::npos);
}

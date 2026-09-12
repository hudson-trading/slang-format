// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT

#include "format/FormatValidation.h"
#include "format/NormalizedFormat.h"
#include <catch2/catch_test_macros.hpp>
#include <exception>

#ifndef _WIN32
#    include <pthread.h>
#endif

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

TEST_CASE("comment equivalence normalizes line endings without dropping blank lines") {
    auto expected = parse("module m; /* first\n\n  last */ endmodule");
    auto missingBlank = parse("module m; /* first\n  last */ endmodule");
    for (std::string_view newline : {"\n", "\r\n", "\r"}) {
        auto tree = parse(
            "module m; /* first" + std::string(newline) + std::string(newline) +
            "  last */ endmodule"
        );
        CHECK(format::isCommentEquivalentTo(tree->root(), expected->root()));
        CHECK(format::isTokenEquivalentTo(tree->root(), expected->root()));
        CHECK_FALSE(format::isCommentEquivalentTo(tree->root(), missingBlank->root()));
        CHECK_FALSE(format::isTokenEquivalentTo(tree->root(), missingBlank->root()));
    }
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

    CHECK(result.parseErrorCount == 0);
    CHECK_FALSE(result.hasDiagnostic(format::FormatDiagnosticKind::StructuralImbalance));
    CHECK(result.isUsable());
    CHECK(result.formatted.find("    lhs = rhs;") != std::string::npos);
    CHECK(result.formatted.find("    registered <= lhs;") != std::string::npos);
}

TEST_CASE("custom indent width preserves the two-space instance name offset") {
    format::Config config;
    config.indentWidth = 3;
    auto result = format::format(
        "indent_width.sv",
        "module top; always_comb begin foo=bar; end child u_child(.a(foo)); endmodule\n", config
    );

    for (const auto& diagnostic : result.diagnostics)
        UNSCOPED_INFO(diagnostic.message);
    REQUIRE(result.isUsable());
    CHECK(result.formatted == R"(module top;
   always_comb begin
      foo = bar;
   end
   child
     u_child (.a(foo));
endmodule
)");
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

    CHECK(result.parseErrorCount > 0);
}

TEST_CASE("generated source remains safe to apply unchanged") {
    std::string input = "// @generated\r\nmodule foo; endmodule";
    auto result = format::format("generated.sv", input, {});
    CHECK(result.generated);
    CHECK(result.isUsable());
    CHECK(result.formatted == input);
    CHECK(result.diagnostics.empty());
    CHECK(result.outputAction() == format::FormatOutputAction::KeepOriginal);
    CHECK(result.outputAction(true) == format::FormatOutputAction::KeepOriginal);
}

TEST_CASE("a top-level skip preserves the first module and formats later modules") {
    std::string input = "// slang-format: skip\nmodule first;logic a;endmodule\n"
                        "module second;logic b;endmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("skip.sv", input, {}, stage);
        REQUIRE(result.isUsable());
        CHECK_FALSE(result.generated);
        CHECK(
            result.formatted.starts_with("// slang-format: skip\nmodule first;logic a;endmodule\n")
        );
        CHECK(
            result.formatted.find("module second;\n    logic b;\nendmodule") != std::string::npos
        );
    }
}

TEST_CASE("off at file scope preserves all declarations without warnings") {
    std::string input = "// slang-format: off\nmodule first;logic a;endmodule\n"
                        "module second;logic b;endmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("off.sv", input, {}, stage);
        CHECK(result.formatted == input);
        CHECK(result.isUsable());
        CHECK(result.outputAction() == format::FormatOutputAction::UseFormatted);
        CHECK(result.outputAction(true) == format::FormatOutputAction::UseFormatted);
        CHECK(result.diagnostics.empty());
    }
}

TEST_CASE("off and on must match in the same list") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (bool matched : {false, true}) {
            std::string input = "module foo; initial begin\n// slang-format: off\na=1;b=2;\n";
            if (matched)
                input += "// slang-format: on\n";
            input += "end\n// slang-format: on\nlogic c;endmodule\n";
            auto result = format::format("scope.sv", input, {}, stage);
            REQUIRE(result.isUsable());
            CHECK(result.formatted.find("a=1;b=2;") != std::string::npos);
            CHECK(result.formatted.find("    logic c;\nendmodule") != std::string::npos);
            REQUIRE(result.diagnostics.size() == 1);
            CHECK(result.diagnostics[0].kind == format::FormatDiagnosticKind::UnmatchedFormatOn);
        }
    }
}

TEST_CASE("off and on handle empty lists, port lists, and unmatched off markers") {
    for (std::string_view input :
         {"module foo;\n// slang-format: off\n// slang-format: on\nendmodule\n",
          "module foo;\n// slang-format: off\nlogic x;endmodule\n",
          "module foo;\n// slang-format: off\nendmodule\n",
          "module foo(\n// slang-format: off\ninput a,input b,\n// slang-format: on\noutput "
          "c);endmodule\n",
          "module foo;\n/* slang-format: off */\nlogic a;logic b;\n/* slang-format: on "
          "*/\nendmodule\n",
          "// slang-format: off\nmodule foo;endmodule\n// slang-format: on\n",
          "module foo;\n// slang-format: office\nlogic a;endmodule\n"}) {
        INFO(input);
        for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
            auto result = format::format("markers.sv", input, {}, stage);
            for (const auto& diag : result.diagnostics)
                UNSCOPED_INFO(diag.message);
            CHECK(result.diagnostics.empty());
            CHECK(result.isUsable());
        }
    }
}

TEST_CASE("nested disabled regions finish before their containing lists collapse") {
    std::string preserved = "initial begin\n// slang-format: off\na=1;b=2;\n"
                            "// slang-format: on\nc=3;\nend";
    std::string input = "module foo;\n// slang-format: off\n" + preserved +
                        "\n// slang-format: on\nlogic value;\nendmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("nested_regions.sv", input, {}, stage);
        REQUIRE(result.isUsable());
        CHECK(result.diagnostics.empty());
        CHECK(result.formatted.find(preserved) != std::string::npos);
        CHECK(result.formatted.find("    logic value;\nendmodule") != std::string::npos);
    }
}

TEST_CASE("unmatched on markers warn without blocking formatting") {
    for (std::string_view input :
         {"module foo;\n// slang-format: on\nlogic x;endmodule\n",
          "module foo;\n// slang-format: on\nendmodule\n",
          "module foo;\n/* slang-format: on */\nlogic x;endmodule\n",
          "// slang-format: on\nmodule foo;endmodule\n",
          "module foo;endmodule\n// slang-format: on\n"}) {
        INFO(input);
        for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
            auto result = format::format("on.sv", input, {}, stage);
            CHECK(result.isUsable());
            CHECK(result.outputAction() == format::FormatOutputAction::UseFormatted);
            CHECK(result.outputAction(true) == format::FormatOutputAction::UseFormatted);
            REQUIRE(result.diagnostics.size() == 1);
            CHECK(result.diagnostics[0].kind == format::FormatDiagnosticKind::UnmatchedFormatOn);
            CHECK(
                result.diagnostics[0].message ==
                "slang-format: on has no preceding off in the same list scope. "
                "Did you put the off directive in the wrong scope?"
            );
            CHECK(result.formatted.find("module foo;\n") != std::string::npos);
        }
    }
}

TEST_CASE("off regions preserve inactive preprocessor content") {
    std::string input = "module foo;\n// slang-format: off\n`ifdef FEATURE\n"
                        "logic a;logic b;\n`else\nlogic c;logic d;\n`endif\n"
                        "// slang-format: on\nlogic e;endmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("conditional.sv", input, {}, stage);
        for (const auto& diag : result.diagnostics)
            UNSCOPED_INFO(diag.message);
        CHECK(result.isUsable());
        CHECK(result.formatted.find("logic a;logic b;") != std::string::npos);
        CHECK(result.formatted.find("logic c;logic d;") != std::string::npos);
    }
}

TEST_CASE("diagnostics determine output policy without separate failure flags") {
    using Kind = format::FormatDiagnosticKind;
    using Action = format::FormatOutputAction;
    format::FormatResult result;
    CHECK(result.isUsable());
    CHECK(result.outputAction() == Action::UseFormatted);

    for (auto kind :
         {Kind::StructuralImbalance, Kind::CstMismatch, Kind::NotIdempotent, Kind::InternalError,
          Kind::FailedReparse, Kind::MergeConflict}) {
        result.diagnostics = {{kind, "", std::nullopt}};
        CHECK(result.hasDiagnostic(kind));
        CHECK_FALSE(result.isUsable());
        bool skippable = kind == Kind::StructuralImbalance || kind == Kind::CstMismatch ||
                         kind == Kind::NotIdempotent;
        CHECK(result.outputAction() == (skippable ? Action::KeepOriginal : Action::Abort));
        CHECK(result.outputAction(true) == Action::UseFormatted);

        // A skippable diagnostic cannot hide an abort, regardless of insertion order.
        for (bool first : {false, true}) {
            result.diagnostics = {{kind, "", std::nullopt}};
            auto pos = first ? result.diagnostics.begin() : result.diagnostics.end();
            result.diagnostics.insert(
                pos, {Kind::StructuralImbalance, "parse errors", std::nullopt}
            );
            CHECK(result.outputAction() == (skippable ? Action::KeepOriginal : Action::Abort));
            result.diagnostics.push_back({Kind::MergeConflict, "conflict", 1});
            CHECK(result.outputAction(true) == Action::UseFormatted);
        }
    }
}

TEST_CASE("Git conflict markers reject formatting in both stages") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (char marker : {'<', '|', '>'}) {
            for (size_t width : {7, 11}) {
                for (std::string_view newline : {"\n", "\r\n", "\r", "\n\r"}) {
                    for (bool labeled : {false, true}) {
                        std::string input = "module foo;" + std::string(newline);
                        input += std::string(width, marker);
                        if (labeled)
                            input += " branch";
                        input += newline;
                        input += "endmodule";
                        auto result = format::format("conflict.sv", input, {}, stage);
                        CHECK_FALSE(result.isUsable());
                        CHECK(result.outputAction() == format::FormatOutputAction::Abort);
                        CHECK(
                            result.outputAction(true) == format::FormatOutputAction::UseFormatted
                        );
                        CHECK_FALSE(result.formatted.empty());
                        const auto& diagnostics = result.diagnostics;
                        REQUIRE_FALSE(diagnostics.empty());
                        CHECK(diagnostics[0].kind == format::FormatDiagnosticKind::MergeConflict);
                        CHECK(diagnostics[0].line == 2);
                        CHECK(
                            diagnostics[0].message.find("Git merge conflict marker") !=
                            std::string::npos
                        );
                    }
                }
            }
        }
    }
}

TEST_CASE("Git conflict detection includes trivia and incomplete conflicts") {
    for (std::string_view prefix :
         {"", "\xef\xbb\xbf", "/*\n", "`ifdef DISABLED\n", "// @generated\n",
          "`line 100 \"other.sv\" 0\n", "module foo; string s = \"\"\"\n", "`define FOO \\\n"}) {
        std::string input = std::string(prefix) + std::string(7, '<') + " HEAD";
        auto result = format::format("conflict.sv", input, {});
        CHECK_FALSE(result.isUsable());
        CHECK_FALSE(result.generated);
        REQUIRE_FALSE(result.diagnostics.empty());
        CHECK(result.diagnostics[0].kind == format::FormatDiagnosticKind::MergeConflict);
        CHECK(result.diagnostics[0].line == (prefix.find('\n') == std::string_view::npos ? 1 : 2));
        CHECK(result.outputAction() == format::FormatOutputAction::Abort);
    }
}

TEST_CASE("Git conflict detection leaves marker lookalikes formattable") {
    std::string input = "// <<<<<<< HEAD\n"
                        "/* ======= */\n"
                        "/*\n======\n=======caption\n<<<<<<<identifier\n  >>>>>>> indented\n*/\n"
                        "module foo; string s = \"<<<<<<< HEAD\";\n"
                        "assign a = b <<< 2; assign c = d >>> 1; endmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("lookalikes.sv", input, {}, stage);
        CHECK_FALSE(result.hasDiagnostic(format::FormatDiagnosticKind::MergeConflict));
        CHECK(result.parseErrorCount == 0);
        CHECK(result.isUsable());
        CHECK(result.formatted != input);
    }
}

TEST_CASE("comment heading underlines are not Git conflicts") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (size_t width : {7, 11, 31}) {
            std::string heading = "/*\nInterface signals\n" + std::string(width, '=') +
                                  " \t\nData input\n*/\n";
            for (bool inactive : {false, true}) {
                std::string input = inactive ? "`ifdef FEATURE\n" + heading + "`endif\n" : heading;
                input += "module foo;logic value;endmodule\n";
                auto result = format::format("heading.sv", input, {}, stage);
                CHECK(result.isUsable());
                CHECK(result.parseErrorCount == 0);
                CHECK(result.formatted.find("logic value;") != std::string::npos);
            }
        }
    }
}

TEST_CASE("Git separators require a corresponding closing marker") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (size_t width : {7, 11}) {
            for (std::string_view newline : {"\n", "\r\n", "\r"}) {
                for (bool matchingWidth : {false, true}) {
                    std::string input = "/*" + std::string(newline) + std::string(width, '=') +
                                        " \t" + std::string(newline) + "branch text" +
                                        std::string(newline) +
                                        std::string(matchingWidth ? width : width + 1, '>') +
                                        " branch" + std::string(newline) + "*/";
                    auto result = format::format("conflict.sv", input, {}, stage);
                    REQUIRE(result.hasDiagnostic(format::FormatDiagnosticKind::MergeConflict));
                    CHECK(result.diagnostics.front().line == (matchingWidth ? 2 : 4));
                    CHECK(result.outputAction() == format::FormatOutputAction::Abort);
                }
            }
        }
    }
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
            for (const auto& diag : result.diagnostics)
                UNSCOPED_INFO(diag.message);
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

TEST_CASE("macro token equivalence permits only argument boundary indentation") {
    parsing::PreprocessorOptions options;
    options.dontExpandMacros = true;
    for (auto prefix : {std::string(""), std::string("`ifdef FEATURE\n")}) {
        std::string suffix = prefix.empty() ? "" : "`endif\n";
        std::string body = "`CHECK(\n  first,\n  (second,\n    third)\n)\n";
        auto original = SyntaxTree::fromText(prefix + body + suffix, sm, "original", "", options);
        for (auto token : {"first", "(second", ")\n", "third"}) {
            INFO(token);
            auto changed = body;
            changed.insert(changed.rfind(token), "    ");
            auto formatted =
                SyntaxTree::fromText(prefix + changed + suffix, sm, "formatted", "", options);
            CHECK(
                format::isTokenEquivalentTo(original->root(), formatted->root()) ==
                (std::string_view(token) != "third")
            );
        }
    }
}

TEST_CASE("multiline macro indentation preserves expanded argument values") {
    std::string input = "`define STRINGS(a, b) initial $display(`\"a`\", `\"b`\");\n"
                        "module foo;\n"
                        "`ifdef FEATURE\n"
                        "`STRINGS(\n first  word\n   second word,\n {foo,  bar}\n)\n"
                        "`else\n"
                        "`STRINGS(\n third  word\n   fourth word,\n {baz,  qux}\n)\n"
                        "`endif\nendmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (unsigned width : {2u, 4u}) {
            format::Config config;
            config.indentWidth = width;
            auto result = format::format("macro.sv", input, config, stage);
            for (const auto& diagnostic : result.diagnostics)
                UNSCOPED_INFO(diagnostic.message);
            REQUIRE(result.isUsable());
            CHECK(
                result.formatted.find("\n" + std::string(width * 3, ' ') + "first  word") !=
                std::string::npos
            );
            for (bool enabled : {false, true}) {
                parsing::PreprocessorOptions options;
                if (enabled)
                    options.predefines.emplace_back("FEATURE");
                auto original = SyntaxTree::fromText(input, sm, "original", "", options);
                auto formatted =
                    SyntaxTree::fromText(result.formatted, sm, "formatted", "", options);
                REQUIRE(original->diagnostics().empty());
                REQUIRE(formatted->diagnostics().empty());
                CHECK(original->root().isEquivalentTo(formatted->root()));
            }
        }
    }
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
    config.alignment.value().paddingLimit = 4;
    auto result = format::format(
        "alignment.sv", "module foo; initial begin long_name = 1; aa = 2; b = 3; end endmodule\n",
        config
    );
    for (const auto& diag : result.diagnostics)
        UNSCOPED_INFO(diag.message);
    CHECK(result.isUsable());
    CHECK(result.formatted.find("aa = 2;\n        b  = 3;") != std::string::npos);
    config.alignment.value().paddingLimit = std::nullopt;
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
            CHECK(malformed.parseErrorCount > 0);
            auto accepted = format::format("valid.sv", valid, {}, stage);
            for (const auto& diag : accepted.diagnostics)
                UNSCOPED_INFO(diag.message);
            CHECK(accepted.parseErrorCount == 0);
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
        CHECK(result.parseErrorCount == 0);
        CHECK(result.isUsable());
    }
}

TEST_CASE("complete configs remain formattable") {
    std::string input = "config foo;design work.top;instance top.dut liblist impl;endconfig\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("config.sv", input, {}, stage);
        CHECK(result.parseErrorCount == 0);
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

TEST_CASE("source validation ignores parser-inserted tokens") {
    auto result =
        format::format("macro.sv", "`BRANCH(1) else\n`BRANCH(2) else\n`BRANCH(3) else\n", {});
    CHECK(result.isUsable());
    CHECK(result.formatted.find("`BRANCH(2)") != std::string::npos);
    auto a = parse("module foo; int value; endmodule");
    auto b = parse("module foo; int value endmodule");
    CHECK_FALSE(format::isTokenEquivalentTo(a->root(), b->root()));
}

TEST_CASE("trailing whitespace does not affect wrapping or inactive comments") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        std::string source =
            "task foo::bar();\nwhile (ready) begin\nforeach (items[item]) begin\n"
            "item.wait_for_state(STATE_EXECUTING, STATE_AFTER); // wait for the item to start\n"
            "end\nend\nendtask\n";
        auto expected = format::format("comment.sv", source, {}, stage);
        source.insert(source.find("start\n") + 5, 20, ' ');
        auto actual = format::format("comment.sv", source, {}, stage);
        CHECK(actual.isUsable());
        CHECK(actual.formatted == expected.formatted);
        auto inactive =
            format::format("comment.sv", "`ifdef OPTION\n// comment   \n`endif\n", {}, stage);
        CHECK(inactive.isUsable());
        auto directive = format::format(
            "directive.sv",
            "`ifndef OPTION        \n`else        \n            value <= data;\n`endif\n", {}, stage
        );
        CHECK(directive.isUsable());
    }
}

TEST_CASE("deep flat expressions fail safely without replacing the input") {
    std::string source = "module foo; assign value = signal_a";
    for (size_t i = 0; i < 30000; i++)
        source += "+signal_a";
    source += "; endmodule\n";
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format("deep.sv", source, {}, stage);
        CHECK(result.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit));
        CHECK(result.formatted == source);
        CHECK(result.outputAction(false) == format::FormatOutputAction::KeepOriginal);
        CHECK(result.outputAction(true) == format::FormatOutputAction::KeepOriginal);
        REQUIRE_FALSE(result.diagnostics.empty());
        CHECK(result.diagnostics.back().message.find("depth limit") != std::string::npos);
    }
}

TEST_CASE("formatting on small worker stacks handles supported and excessive depth") {
    struct Task {
        bool passed = false;
        bool parserNesting = false;
        std::exception_ptr error;
    } task;
    SECTION("flat syntax trees") {
    }
    SECTION("parser nesting") {
        task.parserNesting = true;
    }
    auto run = [](void* data) -> void* {
        auto& task = *static_cast<Task*>(data);
        try {
            format::Config config;
            config.columnLimit = 0;
            if (task.parserNesting) {
#ifndef NDEBUG
                // Slang's unoptimized parser needs more stack per nesting level.
                config.maxSyntaxDepth = 128;
#endif
                size_t depth = config.maxSyntaxDepth.get() - 8;
                std::string supported = "module foo; assign value = " + std::string(depth, '(') +
                                        "signal_a" + std::string(depth, ')') + "; endmodule\n";
                if (!format::format("nested.sv", supported, config).isUsable())
                    return nullptr;
                std::string nested = "module foo; assign value = " + std::string(1024, '(') +
                                     "signal_a" + std::string(1024, ')') + "; endmodule\n";
                auto result = format::format("nested.sv", nested, config);
                task.passed = result.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit) &&
                              result.formatted == nested;
                return nullptr;
            }
            for (size_t operands : {504u, 2040u, 30000u}) {
                std::string source = "module foo; assign value = signal_a";
                for (size_t i = 1; i < operands; i++)
                    source += "+signal_a";
                source += "; endmodule\n";
                auto result = format::format("chain.sv", source, config);
                if (operands == 504
                        ? !result.isUsable()
                        : result.formatted != source ||
                              !result.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit))
                    return nullptr;
                if (operands == 504) {
                    SourceManager sourceManager;
                    auto tree = SyntaxTree::fromText(source, sourceManager);
                    auto rendered = format::Formatter(config, &sourceManager).format(tree->root());
                    if (rendered != result.formatted)
                        return nullptr;
                }
            }
            task.passed = true;
        }
        catch (...) {
            task.error = std::current_exception();
        }
        return nullptr;
    };
#ifdef _WIN32
    // Windows' default stack already exercises the small-stack entry path.
    run(&task);
#else
    pthread_attr_t attributes;
    REQUIRE(pthread_attr_init(&attributes) == 0);
    REQUIRE(pthread_attr_setstacksize(&attributes, 512 * 1024) == 0);
    pthread_t thread;
    int error = pthread_create(&thread, &attributes, run, &task);
    pthread_attr_destroy(&attributes);
    REQUIRE(error == 0);
    REQUIRE(pthread_join(thread, nullptr) == 0);
#endif
    if (task.error)
        std::rethrow_exception(task.error);
    CHECK(task.passed);
}

TEST_CASE("parser nesting limits skip without returning partial source") {
    std::string source = "module foo; assign value = " + std::string(1024, '(') + "signal_a" +
                         std::string(1024, ')') + "; endmodule\n";
    auto result = format::format("nested.sv", source, {});
    CHECK(result.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit));
    CHECK(result.formatted == source);
    CHECK(result.outputAction(true) == format::FormatOutputAction::KeepOriginal);
}

TEST_CASE("deep inactive branches skip the complete file") {
    std::string source = "module foo;\n`ifdef OPTION\nassign value = signal_a";
    for (size_t i = 0; i < 1024; i++)
        source += "+signal_a";
    source += ";\n`endif\nendmodule\n";
    auto result = format::format("inactive.sv", source, {});
    CHECK(result.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit));
    CHECK(result.formatted == source);
    CHECK(result.outputAction(true) == format::FormatOutputAction::KeepOriginal);
}

TEST_CASE("syntax depth limits are configurable for active and inactive syntax") {
    for (bool inactive : {false, true}) {
        for (bool nested : {false, true}) {
            std::string source = "module foo;\n";
            if (inactive)
                source += "`ifdef OPTION\n";
            source += "assign value = ";
            if (nested) {
                source += std::string(300, '(') + "signal_a" + std::string(300, ')');
            }
            else {
                source += "signal_a";
                for (size_t i = 0; i < 600; i++)
                    source += "+signal_a";
            }
            source += ";\n";
            if (inactive)
                source += "`endif\n";
            source += "endmodule\n";

            for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
                format::Config config;
                config.columnLimit = 0;
                config.maxSyntaxDepth = 128;
                auto skipped = format::format("configured.sv", source, config, stage);
                REQUIRE(skipped.hasDiagnostic(format::FormatDiagnosticKind::DepthLimit));
                CHECK(skipped.formatted == source);
                CHECK(skipped.outputAction(true) == format::FormatOutputAction::KeepOriginal);
                CHECK(skipped.diagnostics.back().message.find("(128)") != std::string::npos);

                if (!nested) {
                    config.maxSyntaxDepth = 512;
                    CHECK(
                        format::format("configured.sv", source, config, stage)
                            .hasDiagnostic(format::FormatDiagnosticKind::DepthLimit)
                    );
                }
                config.maxSyntaxDepth = 768;
                CHECK(format::format("configured.sv", source, config, stage).isUsable());
                if (!inactive && !nested) {
                    SourceManager sourceManager;
                    auto tree = SyntaxTree::fromText(source, sourceManager);
                    CHECK_FALSE(
                        format::Formatter(config, &sourceManager, stage)
                            .format(tree->root())
                            .empty()
                    );
                    config.maxSyntaxDepth = 128;
                    CHECK_THROWS_AS(
                        format::Formatter(config, &sourceManager, stage).format(tree->root()),
                        format::FormatDepthLimitError
                    );
                }
            }
        }
    }
}

TEST_CASE("inline assignment conditionals stay inline when over the column limit") {
    format::Config config;
    config.columnLimit = 40;
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format(
            "inline.sv",
            "module foo;\nassign signal_a = `ifdef FOO 8'hff `else 8'h00 `endif;\nendmodule\n",
            config, stage
        );
        CHECK(result.isUsable());
        CHECK(result.formatted.find("signal_a = `ifdef FOO") != std::string::npos);
    }
}

TEST_CASE("unlimited columns keep short case clauses inline") {
    format::Config config;
    config.columnLimit = 0;
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format(
            "case.sv",
            "module foo; always_comb case (select) 0: value = 1; 1: value = 2; "
            "endcase endmodule\n",
            config, stage
        );
        CHECK(result.isUsable());
        CHECK(result.formatted.find("0: value = 1;") != std::string::npos);
        CHECK(result.formatted.find("1: value = 2;") != std::string::npos);
    }
}

TEST_CASE("off and skip regions retain internal trailing whitespace") {
    for (auto marker : {"off", "skip"}) {
        std::string preserved = "module foo;  \n  \t\n  logic value; \t\nendmodule";
        std::string source = "// slang-format: " + std::string(marker) + "\n" + preserved + "\n";
        for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
            auto result = format::format("preserved.sv", source, {}, stage);
            CHECK(result.isUsable());
            CHECK(result.formatted.find(preserved) != std::string::npos);
        }
    }
}

TEST_CASE("macro calls wrap consistently with zero indentation") {
    format::Config config;
    config.indentWidth = 0;
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format(
            "macro.sv",
            "task foo(); check_value(0, `VALUE(very_long_scope."
            "some_really_long_assertion_name_that_exceeds_the_configured_column_limit)); "
            "endtask\n",
            config, stage
        );
        CHECK(result.isUsable());
    }
}

TEST_CASE("foreign syntax detection ignores comments and strings") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        auto result = format::format(
            "text.sv",
            "// ${name} `systemc_header \\\n"
            "module foo; string value=\"${name} `systemc_interface\"; endmodule\n",
            {}, stage
        );
        CHECK(result.isUsable());
        CHECK(result.formatted.find("\n    string value =") != std::string::npos);
    }
}

TEST_CASE("unsafe directive and container recovery preserves source") {
    for (auto stage : {format::FormatStage::Layout, format::FormatStage::Aligned}) {
        for (std::string_view source :
             {"`define VALUE 1 \\  \n    + 2\nmodule foo; endmodule\n",
              "`define VALUE \\\n    // comment \\  \n    + 2\nmodule foo; endmodule\n",
              "input logic a;\nendmodule\n"}) {
            auto result = format::format("recovery.sv", source, {}, stage);
            CHECK(result.isUsable());
            CHECK(result.formatted == source);
        }
    }
}

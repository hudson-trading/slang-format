// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT

#include "format/FormatDocument.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "slang/syntax/SyntaxKind.h"

TEST_CASE("deep document wrappers preserve child order and anchor scope") {
    format::DocumentBuilder builder;
    auto root = builder.text("value");
    for (size_t i = 0; i < 4096; i++) {
        root =
            builder.concat({builder.text("a"), builder.relativeAnchor(0, root), builder.text("b")});
    }
    root = builder.concat({root, builder.hardLine(), builder.text("next")});
    auto document = std::move(builder).finish(root);
    format::Config config;
    config.columnLimit = 0;
    format::DocumentRenderer renderer(config);
    auto layout = renderer.renderLayout(document);
    CHECK(layout.text == std::string(4096, 'a') + "value" + std::string(4096, 'b') + "\nnext\n");
    CHECK(renderer.renderAligned(document, layout).text == layout.text);
}

TEST_CASE("layout solver reuses costs for many fitting members") {
    format::DocumentBuilder builder;
    std::vector<format::DocId> lines;
    std::string expected;
    constexpr size_t memberCount = 4096;

    for (size_t i = 0; i < memberCount; i++) {
        auto member = builder.concat(
            {builder.text("assign value ="), builder.softLine(1), builder.text("source;")}
        );
        lines.push_back(builder.member(member, slang::syntax::SyntaxKind::ContinuousAssign));
        lines.push_back(builder.hardLine());
        expected += "assign value = source;\n";
    }

    format::Config config;
    auto root = builder.concat(std::move(lines));
    auto document = std::move(builder).finish(root);
    auto rendered = format::DocumentRenderer(config).renderLayout(document);
    CHECK(rendered.text == expected);
}

TEST_CASE("layout solver bounds pathological member search") {
    format::DocumentBuilder builder;
    std::vector<format::DocId> expression;
    constexpr size_t breakCount = 1024;

    for (size_t i = 0; i < breakCount; i++) {
        expression.push_back(builder.text("value"));
        expression.push_back(builder.softLine(1));
    }
    expression.push_back(builder.text("value"));

    auto member = builder.member(
        builder.concat(std::move(expression)), slang::syntax::SyntaxKind::ContinuousAssign
    );
    auto document = std::move(builder).finish(member);
    format::Config config;
    auto rendered = format::DocumentRenderer(config).renderLayout(document);

    CHECK(rendered.breaks.size() < breakCount);
    CHECK(rendered.breaks.size() >= breakCount - 16);
    CHECK(std::ranges::count(rendered.text, '\n') == rendered.breaks.size() + 1);
}

TEST_CASE("alignment uses stable line coordinates after whitespace normalization") {
    format::DocumentBuilder builder;
    auto group = builder.createAlignmentGroup();
    auto anchorA =
        builder.alignmentAnchor(2, slang::syntax::SyntaxKind::ParameterDeclaration, group, 1);
    auto anchorB =
        builder.alignmentAnchor(2, slang::syntax::SyntaxKind::ParameterDeclaration, group, 1);
    auto root = builder.concat(
        {builder.verbatim(std::string("header") + std::string(1024, ' ') + "\n"), builder.text("a"),
         anchorA, builder.text(" = 1;"), builder.hardLine(), builder.text("long_name"), anchorB,
         builder.text(" = 2;")}
    );
    auto document = std::move(builder).finish(root);
    format::Config config;
    format::DocumentRenderer renderer(config);
    auto layout = renderer.renderLayout(document);

    CHECK_NOTHROW(renderer.renderAligned(document, layout));
}

TEST_CASE("adjacent hard line requirements compose by maximum") {
    format::DocumentBuilder builder;
    auto root = builder.concat(
        {builder.text("first"), builder.hardLine(2), builder.hardLine(2), builder.text("second")}
    );
    auto document = std::move(builder).finish(root);
    format::Config config;

    auto rendered = format::DocumentRenderer(config).renderLayout(document);
    CHECK(rendered.text == "first\n\nsecond\n");
}

TEST_CASE("member verbatim text resets nested indentation") {
    format::DocumentBuilder builder;
    auto nested = builder.indent(
        5, builder.concat({builder.hardLine(), builder.memberVerbatim("recovered")})
    );
    auto member = builder.member(nested, slang::syntax::SyntaxKind::FunctionDeclaration);
    auto document = std::move(builder).finish(builder.indent(8, member));
    format::Config config;

    auto rendered = format::DocumentRenderer(config).renderLayout(document);
    CHECK(rendered.text == "        recovered\n");
}

TEST_CASE("wrapping independent members preserves the following member cursor") {
    format::DocumentBuilder builder;
    std::vector<format::DocId> lines;
    std::string expected;
    for (size_t i = 0; i < 4096; i++) {
        auto member = builder.concat(
            {builder.text("assign value ="),
             builder.indent(4, builder.concat({builder.softLine(1), builder.text("source;")}))}
        );
        lines.push_back(builder.member(member, slang::syntax::SyntaxKind::ContinuousAssign));
        lines.push_back(builder.hardLine());
        expected += "assign value =\n    source;\n";
    }
    format::Config config;
    config.columnLimit = 20;
    auto root = builder.concat(std::move(lines));
    auto document = std::move(builder).finish(root);
    auto rendered = format::DocumentRenderer(config).renderLayout(document);
    CHECK(rendered.text == expected);
    CHECK(rendered.breaks.size() == 4096);
}

TEST_CASE("nested members reuse costs after wrapping at a case label") {
    format::DocumentBuilder builder;
    std::vector<format::DocId> rows;
    std::string expected;
    for (size_t i = 0; i < 4096; i++) {
        auto assignment = builder.member(
            builder.concat({builder.text("value ="), builder.softLine(1), builder.text("source;")}),
            slang::syntax::SyntaxKind::ExpressionStatement
        );
        rows.push_back(builder.member(
            builder.concat({builder.text("label: "), assignment, builder.hardLine()}),
            slang::syntax::SyntaxKind::StandardCaseItem
        ));
        expected += "label: value =\nsource;\n";
    }
    auto root =
        builder.member(builder.concat(std::move(rows)), slang::syntax::SyntaxKind::CaseStatement);
    auto document = std::move(builder).finish(root);
    format::Config config;
    config.columnLimit = 15;
    auto rendered = format::DocumentRenderer(config).renderLayout(document);
    CHECK(rendered.text == expected);
    CHECK(rendered.breaks.size() == 4096);
}

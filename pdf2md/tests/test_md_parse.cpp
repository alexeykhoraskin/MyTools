// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include <gtest/gtest.h>

#include "pdf2md/md_parse.hpp"

#include <sstream>

using namespace pdf2md;

namespace {

MdDoc parse(const std::string& text) {
    std::istringstream in(text);
    return MarkdownParser{}.parse(in);
}

} // namespace

TEST(MarkdownParser, LineClassification) {
    EXPECT_EQ(MarkdownParser::classify_line("").type, MdLineType::Empty);
    EXPECT_EQ(MarkdownParser::classify_line("# H1").type, MdLineType::Heading);
    EXPECT_EQ(MarkdownParser::classify_line("# H1").level, 1);
    EXPECT_EQ(MarkdownParser::classify_line("###### H6").level, 6);
    EXPECT_EQ(MarkdownParser::classify_line("---").type, MdLineType::Hr);
    EXPECT_EQ(MarkdownParser::classify_line("***").type, MdLineType::Hr);
    EXPECT_EQ(MarkdownParser::classify_line("```cpp").type, MdLineType::CodeFence);
    EXPECT_EQ(MarkdownParser::classify_line("```cpp").text, "cpp");
    EXPECT_EQ(MarkdownParser::classify_line("- item").type, MdLineType::Bullet);
    EXPECT_EQ(MarkdownParser::classify_line("plain").type, MdLineType::Paragraph);
}

TEST(MarkdownParser, BulletIndentLevel) {
    EXPECT_EQ(MarkdownParser::classify_line("  - nested").level, 1);
}

TEST(MarkdownParser, BlockParsing) {
    MdDoc doc = parse(
        "# Title\n"
        "\n"
        "para line 1\n"
        "para line 2\n"
        "\n"
        "- a\n"
        "- b\n"
        "\n"
        "```\n"
        "code line\n"
        "```\n"
        "\n"
        "---\n");
    ASSERT_EQ(doc.blocks.size(), 5u);
    EXPECT_EQ(doc.blocks[0].type, MdBlock::Type::Heading);
    EXPECT_EQ(doc.blocks[0].level, 1);
    EXPECT_EQ(doc.blocks[0].lines[0], "Title");
    EXPECT_EQ(doc.blocks[1].type, MdBlock::Type::Paragraph);
    EXPECT_EQ(doc.blocks[1].lines.size(), 2u);
    EXPECT_EQ(doc.blocks[2].type, MdBlock::Type::List);
    EXPECT_EQ(doc.blocks[2].lines.size(), 2u);
    EXPECT_EQ(doc.blocks[3].type, MdBlock::Type::Code);
    EXPECT_EQ(doc.blocks[3].lines.size(), 1u);
    EXPECT_EQ(doc.blocks[4].type, MdBlock::Type::Hr);
}

TEST(MarkdownParser, CodeFenceNotParsed) {
    // содержимое блока кода не парсится как markdown
    MdDoc doc = parse("```\n# not a heading\n- not a list\n```\n");
    ASSERT_EQ(doc.blocks.size(), 1u);
    EXPECT_EQ(doc.blocks[0].type, MdBlock::Type::Code);
    EXPECT_EQ(doc.blocks[0].lines.size(), 2u);
}

TEST(MarkdownParser, StripMdEmphasisAndCode) {
    EXPECT_EQ(MarkdownParser::strip_md("**bold**"), "bold");
    EXPECT_EQ(MarkdownParser::strip_md("*italic*"), "italic");
    EXPECT_EQ(MarkdownParser::strip_md("__underline__"), "underline");
    EXPECT_EQ(MarkdownParser::strip_md("`code`"), "code");
    EXPECT_EQ(MarkdownParser::strip_md("a **b** c"), "a b c");
    EXPECT_EQ(MarkdownParser::strip_md("\\*not emphasis\\*"), "*not emphasis*");
}

TEST(MarkdownParser, StripMdLinks) {
    EXPECT_EQ(MarkdownParser::strip_md("[label](http://example.com)"), "label");
    EXPECT_EQ(MarkdownParser::strip_md("see [docs](https://x.io/a) now"), "see docs now");
}

TEST(MarkdownParser, StripMdImages) {
    EXPECT_EQ(MarkdownParser::strip_md("![alt text](img.png)"), "");
    EXPECT_EQ(MarkdownParser::strip_md("before ![alt](i.png) after"), "before  after");
}

TEST(MarkdownParser, StripMdPlainText) {
    EXPECT_EQ(MarkdownParser::strip_md("plain text 123"), "plain text 123");
}

TEST(MarkdownParser, StripMdWordBoundaries) {
    // strip_md: маркеры выделения убираются только на границах слов;
    // внутрисловные остаются (идентификаторы, арифметика, snake_case)
    EXPECT_EQ(MarkdownParser::strip_md("snake_case_names"), "snake_case_names");
    EXPECT_EQ(MarkdownParser::strip_md("my_var = 5 * 3"), "my_var = 5 * 3");
    EXPECT_EQ(MarkdownParser::strip_md("_lead and trail_"), "lead and trail");
    EXPECT_EQ(MarkdownParser::strip_md("mixed **b** _b_ `c` work"), "mixed b b c work");
}
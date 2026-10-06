// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include <gtest/gtest.h>

#include "pdf2md/pdf_classify.hpp"

#include <cstddef>
#include <string>
#include <vector>

using namespace pdf2md;

namespace {

std::vector<LineKind> classify(std::vector<RawLine> raw) {
    return LineClassifier{}.classify(raw);
}

} // namespace

TEST(LineClassifier, BulletMarkers) {
    EXPECT_TRUE(LineClassifier::is_bullet_marker("- item"));
    EXPECT_TRUE(LineClassifier::is_bullet_marker("* item"));
    EXPECT_TRUE(LineClassifier::is_bullet_marker("\xE2\x80\xA2 item"));   // •
    EXPECT_TRUE(LineClassifier::is_bullet_marker("\xE2\x97\x8F item"));   // ●
    EXPECT_FALSE(LineClassifier::is_bullet_marker("plain text"));
    EXPECT_FALSE(LineClassifier::is_bullet_marker(""));
    // определение маркера проверяет только префикс
    EXPECT_TRUE(LineClassifier::is_bullet_marker("-no-space"));
}

TEST(LineClassifier, StripBulletPrefix) {
    EXPECT_EQ(LineClassifier::strip_bullet_prefix("- item"), "item");
    EXPECT_EQ(LineClassifier::strip_bullet_prefix("-item"), "item");
    EXPECT_EQ(LineClassifier::strip_bullet_prefix("* item"), "item");
    EXPECT_EQ(LineClassifier::strip_bullet_prefix("\xE2\x80\xA2 item"), "item");
    EXPECT_EQ(LineClassifier::strip_bullet_prefix("no marker"), "no marker");
}

TEST(LineClassifier, BlankThenShortLineIsHeading) {
    // классификация: пустая строка, короткая строка после пустой -> заголовок
    auto k = classify({{"Title"}, {""}, {"Some long paragraph text that goes on and on"}});
    EXPECT_EQ(k[0], LineKind::Heading);
    EXPECT_EQ(k[1], LineKind::Blank);
    EXPECT_EQ(k[2], LineKind::Para);
}

TEST(LineClassifier, ColonLineIsNotHeading) {
    // короткая строка с двоеточием — не заголовок (когда не первая строка)
    auto k = classify({{"Intro paragraph text that is long enough to stay a paragraph"},
                       {""},
                       {"Note: see below"}});
    EXPECT_EQ(k[2], LineKind::Para);
}

TEST(LineClassifier, SentenceEndIsNotHeading) {
    // строка, заканчивающаяся знаком конца предложения, — не заголовок
    auto k = classify({{"This ends a sentence."}});
    EXPECT_EQ(k[0], LineKind::Para);
}

TEST(LineClassifier, LongLineIsNotHeading) {
    // длинная строка — не заголовок
    std::string long_line(60, 'x');
    auto k = classify({{long_line}});
    EXPECT_EQ(k[0], LineKind::Para);
}

TEST(LineClassifier, BulletLines) {
    // строки с маркером — список
    auto k = classify({{"- one"}, {"- two"}});
    EXPECT_EQ(k[0], LineKind::Bullet);
    EXPECT_EQ(k[1], LineKind::Bullet);
}

TEST(LineClassifier, LookAheadForBullets) {
    // заглядывание вперёд: короткая строка, за которой идёт список -> заголовок
    auto k = classify({{"Some intro text here"}, {"Features"}, {"- a"}, {"- b"}});
    EXPECT_EQ(k[1], LineKind::Heading);
    EXPECT_EQ(k[2], LineKind::Bullet);
}

TEST(LineClassifier, PageBreakIsBlank) {
    // разрыв страницы -> пустая строка
    auto k = classify({{"Text"}, RawLine{.text = {}, .page_break = true}, {"More"}});
    EXPECT_EQ(k[0], LineKind::Heading);   // первая содержательная строка
    EXPECT_EQ(k[1], LineKind::Blank);
    EXPECT_EQ(k[2], LineKind::Heading);   // после пустой, короткая -> заголовок
}

TEST(LineClassifier, Render) {
    // рендер: заголовок, слияние абзацев, списки, разрывы страниц
    std::vector<RawLine> raw = {
        {"My Title"},
        {""},
        {"This is a long paragraph line one that stays a paragraph"},
        {"and this is line two continuing the same paragraph block"},
        {"- item"}, RawLine{.text = {}, .page_break = true}, {"After"},
    };
    auto kinds = classify(raw);
    std::string md = LineClassifier{}.render(raw, kinds, "doc", "2026");

    EXPECT_TRUE(md.starts_with("# doc\n\n_Converted from PDF on 2026_\n\n---\n\n"));
    EXPECT_NE(md.find("## My Title\n"), std::string::npos);
    EXPECT_NE(md.find("line one that stays a paragraph and this is line two"), std::string::npos);
    EXPECT_NE(md.find("- item\n"), std::string::npos);
    EXPECT_NE(md.find("\n---\n\n"), std::string::npos);
    EXPECT_NE(md.find("After"), std::string::npos);
}
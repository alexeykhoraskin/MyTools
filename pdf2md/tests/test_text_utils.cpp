// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include <gtest/gtest.h>

#include "pdf2md/text_utils.hpp"

using namespace pdf2md;

TEST(TextUtils, Trim) {
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("  abc  "), "abc");
    EXPECT_EQ(trim("\r\n\tabc\r\n"), "abc");
    EXPECT_EQ(trim("   "), "");
    EXPECT_EQ(trim("abc"), "abc");
}

TEST(TextUtils, VisualLength) {
    EXPECT_EQ(vis_len(""), 0u);
    EXPECT_EQ(vis_len("abc"), 3u);
    EXPECT_EQ(vis_len("привет"), 6u);
    EXPECT_EQ(vis_len("日本語"), 3u);
    EXPECT_EQ(vis_len("mixé 日本"), 7u);
}
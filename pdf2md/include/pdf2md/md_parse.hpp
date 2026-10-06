// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace pdf2md {

enum class MdLineType { Empty, Heading, Bullet, Paragraph, CodeFence, Hr };

struct MdLine {
    MdLineType type{MdLineType::Empty};
    int level{0};       // уровень заголовка / отступ списка
    std::string text;
};

struct MdBlock {
    enum class Type { Heading, Paragraph, List, Code, Hr };
    Type type{Type::Paragraph};
    int level{0};
    std::vector<std::string> lines;
};

struct MdDoc {
    std::vector<MdBlock> blocks;
};

// Парсер Markdown: классификация строк, разбор блоков, снятие inline-разметки.
class MarkdownParser {
public:
    static MdLine classify_line(std::string_view line);
    static std::string strip_md(std::string_view text);

    MdDoc parse(std::istream& in) const;
    MdDoc parse(const std::filesystem::path& path) const;
};

} // namespace pdf2md
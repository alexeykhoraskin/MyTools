// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace pdf2md {

struct RawLine {
    std::string text;
    bool page_break{false};
};

enum class LineKind { Blank, Heading, Bullet, Para };

// Чистая логика конвертации PDF->Markdown: классифицирует сырые
// извлечённые строки и оформляет их как Markdown. Без внешних зависимостей.
class LineClassifier {
public:
    static constexpr std::string_view kTitlePrefix = "# ";
    static constexpr std::string_view kConvertedNote = "\n\n_Converted from PDF on ";
    static constexpr std::string_view kNoteSuffix = "_\n\n---\n\n";
    static constexpr std::string_view kHeadingPrefix = "## ";
    static constexpr std::string_view kBulletPrefix = "- ";
    static constexpr char kSpace = ' ';
    static constexpr std::string_view kPageBreak = "\n---\n\n";

    // true, если обрезанная строка начинается с маркера списка
    // ('-', '*' или UTF-8 символа маркера/тире)
    static bool is_bullet_marker(std::string_view trimmed);

    // убирает ведущие маркеры списка, возвращает оставшийся текст
    static std::string strip_bullet_prefix(std::string_view text);

    std::vector<LineKind> classify(const std::vector<RawLine>& raw) const;

    std::string render(const std::vector<RawLine>& raw,
                       const std::vector<LineKind>& kinds,
                       std::string_view title,
                       std::string_view date) const;
};

} // namespace pdf2md
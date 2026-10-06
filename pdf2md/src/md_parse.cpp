// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/md_parse.hpp"

#include "pdf2md/text_utils.hpp"

#include <cctype>
#include <fstream>

namespace pdf2md {

namespace {

constexpr std::string_view kCodeFence = "```";
constexpr std::string_view kHr1 = "---";
constexpr std::string_view kHr2 = "***";
constexpr std::string_view kBullet1 = "- ";
constexpr std::string_view kBullet2 = "* ";
constexpr char kHeadingMark = '#';
constexpr int kListIndentStep = 2;

} // namespace

MdLine MarkdownParser::classify_line(std::string_view line) {
    std::string_view t = trim(line);
    if (t.empty())           return {MdLineType::Empty, 0, {}};
    if (t == kHr1 || t == kHr2) return {MdLineType::Hr, 0, {}};
    if (t.starts_with(kCodeFence)) return {MdLineType::CodeFence, 0, std::string(t.substr(kCodeFence.size()))};
    if (t.front() == kHeadingMark) {
        int lev = 0;
        while (lev < (int)t.size() && t[lev] == kHeadingMark) ++lev;
        return {MdLineType::Heading, lev, std::string(trim(t.substr(lev)))};
    }
    if (t.starts_with(kBullet1) || t.starts_with(kBullet2)) {
        int indent = 0;
        while (indent < (int)line.size() && line[indent] == ' ') ++indent;
        return {MdLineType::Bullet, indent / kListIndentStep, std::string(trim(t.substr(2)))};
    }
    return {MdLineType::Paragraph, 0, std::string(t)};
}

MdDoc MarkdownParser::parse(std::istream& in) const {
    MdDoc doc;
    MdBlock cur{MdBlock::Type::Paragraph, 0, {}};
    bool in_code = false;
    std::string line;

    auto flush = [&] {
        if (!cur.lines.empty()) doc.blocks.push_back(std::move(cur));
        cur = {MdBlock::Type::Paragraph, 0, {}};
    };

    while (std::getline(in, line)) {
        MdLine ml = classify_line(line);

        if (ml.type == MdLineType::CodeFence) {
            if (in_code) { in_code = false; flush(); }
            else         { in_code = true;  flush(); cur.type = MdBlock::Type::Code; }
            continue;
        }

        if (in_code) {
            cur.lines.push_back(line);
            continue;
        }

        switch (ml.type) {
        case MdLineType::Empty:
            flush();
            break;
        case MdLineType::Hr:
            flush();
            doc.blocks.push_back({MdBlock::Type::Hr, 0, {}});
            break;
        case MdLineType::Heading:
            flush();
            cur.type = MdBlock::Type::Heading;
            cur.level = ml.level;
            cur.lines.push_back(std::move(ml.text));
            flush();
            break;
        case MdLineType::Bullet:
            if (cur.type != MdBlock::Type::List) { flush(); cur.type = MdBlock::Type::List; }
            cur.lines.push_back(std::move(ml.text));
            break;
        case MdLineType::Paragraph:
            if (cur.type != MdBlock::Type::Paragraph) { flush(); cur.type = MdBlock::Type::Paragraph; }
            cur.lines.push_back(std::move(ml.text));
            break;
        case MdLineType::CodeFence:
            break;
        }
    }
    if (!cur.lines.empty())
        doc.blocks.push_back(std::move(cur));

    return doc;
}

MdDoc MarkdownParser::parse(const std::filesystem::path& path) const {
    std::ifstream in(path);
    if (!in) return {};
    return parse(in);
}

std::string MarkdownParser::strip_md(std::string_view text) {
    auto is_word = [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0;
    };
    std::string r;
    r.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (c == '\\' && i + 1 < text.size()) { r += text[i + 1]; i += 2; continue; }
        if (c == '*' || c == '_') {
            // убираем только маркеры выделения на границах слов;
            // внутрисловные маркеры — это обычный текст (a*b, snake_case, f(x)_y)
            std::size_t run = 0;
            while (i + run < text.size() && text[i + run] == c) ++run;
            bool prev_word = (i > 0 && is_word(text[i - 1]));
            bool next_word = (i + run < text.size() && is_word(text[i + run]));
            bool is_marker = (!prev_word && next_word) || (prev_word && !next_word);
            if (is_marker) { i += run; continue; }
            r.append(text.substr(i, run));
            i += run;
            continue;
        }
        if (c == '`') { ++i; continue; }
        if (c == '!' && i + 1 < text.size() && text[i + 1] == '[') {
            // изображение: выбросить полностью
            i += 2;
            while (i < text.size() && text[i] != ')') ++i;
            if (i < text.size()) ++i;
            continue;
        }
        if (c == '[') {
            // ссылка [label](url) -> label
            ++i;
            std::size_t label_start = i;
            while (i < text.size() && text[i] != ']') ++i;
            r.append(text.substr(label_start, i - label_start));
            if (i < text.size()) ++i;                 // пропустить ']'
            if (i < text.size() && text[i] == '(') {  // пропустить (url)
                while (i < text.size() && text[i] != ')') ++i;
                if (i < text.size()) ++i;
            }
            continue;
        }
        r += c;
        ++i;
    }
    return r;
}

} // namespace pdf2md
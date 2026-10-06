// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/pdf_classify.hpp"

#include "pdf2md/text_utils.hpp"

#include <algorithm>

namespace pdf2md {

namespace {

constexpr std::size_t kHeadingMaxLen = 40;   // визуальных символов
constexpr std::size_t kLookAhead = 5;        // сколько строк смотреть вперёд в поисках маркеров списка

constexpr std::string_view kSentenceEnd = ".!?;";
constexpr char kColon = ':';

} // namespace

bool LineClassifier::is_bullet_marker(std::string_view t) {
    if (t.empty()) return false;
    return t.front() == '-' || t.front() == '*' || t.starts_with("\xE2\x80") ||
           t.starts_with("\xE2\x97");
}

std::string LineClassifier::strip_bullet_prefix(std::string_view bt) {
    std::size_t start = 0;
    bool stripped = false;
    do {
        stripped = false;
        if (start < bt.size() && (bt[start] == '-' || bt[start] == '*')) {
            start += (start + 1 < bt.size() && bt[start + 1] == kSpace) ? 2 : 1;
            stripped = true;
        } else if (bt.substr(start).starts_with("\xE2\x80") ||
                   bt.substr(start).starts_with("\xE2\x97")) {
            start += 3;
            if (start < bt.size() && bt[start] == kSpace) ++start;
            stripped = true;
        }
    } while (stripped && start < bt.size());
    return std::string(bt.substr(start));
}

std::vector<LineKind> LineClassifier::classify(const std::vector<RawLine>& raw) const {
    std::vector<LineKind> kind(raw.size(), LineKind::Para);

    // индекс первой содержательной строки (вычисляется один раз, а не на каждой строке)
    std::size_t first_content = raw.size();
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].page_break) continue;
        if (!trim(raw[i].text).empty()) { first_content = i; break; }
    }

    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].page_break) { kind[i] = LineKind::Blank; continue; }

        std::string_view t = trim(raw[i].text);
        if (t.empty()) { kind[i] = LineKind::Blank; continue; }
        if (is_bullet_marker(t)) { kind[i] = LineKind::Bullet; continue; }

        std::size_t vlen = vis_len(t);
        bool has_colon = t.find(kColon) != std::string_view::npos;
        bool short_enough = vlen < kHeadingMaxLen;
        bool ends_sentence = !t.empty() && kSentenceEnd.find(t.back()) != std::string_view::npos;
        bool prev_blank = (i == 0 || kind[i - 1] == LineKind::Blank || raw[i - 1].page_break);
        bool prev_heading = (i > 0 && kind[i - 1] == LineKind::Heading);
        bool prev_bullet = (i > 0 && kind[i - 1] == LineKind::Bullet);

        // заглядываем вперёд: заголовок часто предваряет список
        bool next_are_bullets = false;
        for (std::size_t j = i + 1; j < raw.size() && j < i + 1 + kLookAhead; ++j) {
            if (raw[j].page_break) break;
            std::string_view nt = trim(raw[j].text);
            if (nt.empty()) break;
            if (is_bullet_marker(nt)) { next_are_bullets = true; break; }
            if (vis_len(nt) < kHeadingMaxLen) break;
        }

        bool is_heading = short_enough && !has_colon && !ends_sentence &&
            (prev_blank || prev_heading || prev_bullet || next_are_bullets);

        // первая содержательная строка — заголовок-заголовок документа
        if (!is_heading && short_enough && !ends_sentence)
            is_heading = (i == first_content);

        kind[i] = is_heading ? LineKind::Heading : LineKind::Para;
    }
    return kind;
}

std::string LineClassifier::render(const std::vector<RawLine>& raw,
                                   const std::vector<LineKind>& kinds,
                                   std::string_view title,
                                   std::string_view date) const {
    std::string out;
    out.reserve(raw.size() * 32);
    out += kTitlePrefix;
    out += title;
    out += kConvertedNote;
    out += date;
    out += kNoteSuffix;

    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].page_break) {
            out += kPageBreak;
            continue;
        }

        switch (kinds[i]) {
        case LineKind::Blank:
            out += '\n';
            break;

        case LineKind::Heading:
            out += kHeadingPrefix;
            out.append(trim(raw[i].text));
            out += "\n\n";
            break;

        case LineKind::Bullet:
            out += kBulletPrefix;
            out += strip_bullet_prefix(trim(raw[i].text));
            out += '\n';
            break;

        case LineKind::Para: {
            std::string merged(trim(raw[i].text));
            while (i + 1 < raw.size() && kinds[i + 1] == LineKind::Para) {
                ++i;
                if (!merged.empty() && merged.back() != kSpace) merged += kSpace;
                merged.append(trim(raw[i].text));
            }
            out += merged;
            out += "\n\n";
            break;
        }
        }
    }
    return out;
}

} // namespace pdf2md
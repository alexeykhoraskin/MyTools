// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <memory>
#include <cmath>

#include <poppler-document.h>
#include <poppler-page.h>
#include <hpdf.h>

namespace fs = std::filesystem;

enum class Mode { PdfToMd, MdToPdf };

// ── helpers ──

static std::string trim(std::string s) {
    auto f = s.find_first_not_of(" \t\r\n");
    if (f == std::string::npos) return {};
    auto l = s.find_last_not_of(" \t\r\n");
    return s.substr(f, l - f + 1);
}

static std::string rtrim(std::string s) {
    auto l = s.find_last_not_of(" \t\r\n");
    return l == std::string::npos ? "" : s.substr(0, l + 1);
}

// approximate visual length of a UTF-8 string
static size_t vis_len(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if ((s[i] & 0xC0) != 0x80) ++n; // count non-continuation bytes
    return n;
}

static bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

// ═══════════════════════════════════════════════
//  PDF → Markdown (poppler)
// ═══════════════════════════════════════════════

static bool pdf_to_md(const fs::path& input, const fs::path& output) {
    std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(input.string()));
    if (!doc || doc->is_locked()) {
        std::cerr << "Error: cannot open PDF: " << input << "\n";
        return false;
    }

    // collect all lines from all pages
    struct RawLine {
        std::string text;
        bool page_break{false};
    };
    std::vector<RawLine> raw;

    for (int p = 0; p < doc->pages(); ++p) {
        std::unique_ptr<poppler::page> pg(doc->create_page(p));
        if (!pg) continue;

        if (p > 0) raw.push_back({.page_break = true});

        auto u8 = pg->text().to_utf8();
        std::string text(u8.data(), u8.size());
        std::istringstream stream(text);
        std::string line;

        while (std::getline(stream, line)) {
            std::string t = trim(line);
            if (t.empty()) { raw.push_back({}); continue; }
            if (t.find_first_not_of("0123456789") == std::string::npos && t.size() <= 4)
                continue;
            raw.push_back({std::move(line), false});
        }
    }

    if (raw.empty()) {
        std::cerr << "Error: no text found in PDF\n";
        return false;
    }

    // classify each line
    enum Kind { Blank, Heading, Bullet, Para };
    std::vector<Kind> kind(raw.size(), Para);

    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].page_break) { kind[i] = Blank; continue; }

        std::string t = trim(raw[i].text);
        if (t.empty()) { kind[i] = Blank; continue; }

        bool is_bullet = (!t.empty() && (t[0] == '-' || t[0] == '*' ||
                          (t.size() >= 3 && (unsigned char)t[0] == 0xE2 &&
                           ((unsigned char)t[1] == 0x80 || (unsigned char)t[1] == 0x97))));

        if (is_bullet) { kind[i] = Bullet; continue; }

        size_t vlen = vis_len(t);
        bool has_colon = t.find(':') != std::string::npos;
        bool short_enough = vlen < 40;
        bool ends_sentence = (!t.empty() && (t.back() == '.' || t.back() == '!' ||
                              t.back() == '?' || t.back() == ';'));
        bool prev_blank = (i == 0 || kind[i - 1] == Blank || raw[i - 1].page_break);
        bool prev_heading = (i > 0 && kind[i - 1] == Heading);
        bool prev_bullet = (i > 0 && kind[i - 1] == Bullet);

        // look ahead to see if next lines are bullets
        bool next_are_bullets = false;
        for (size_t j = i + 1; j < raw.size() && j < i + 6; ++j) {
            std::string nt = trim(raw[j].text);
            if (nt.empty()) break;
            if (nt[0] == '-' || nt[0] == '*' ||
                (nt.size() >= 3 && (unsigned char)nt[0] == 0xE2 &&
                 ((unsigned char)nt[1] == 0x80 || (unsigned char)nt[1] == 0x97))) {
                next_are_bullets = true; break;
            }
            if (vis_len(nt) < 40) break;
            break;
        }

        // heading: short line, no colon, doesn't end a sentence,
        // and is surrounded by different content types
        bool is_heading = short_enough && !has_colon && !ends_sentence &&
            (prev_blank || prev_heading || prev_bullet || next_are_bullets);

        // first non-blank line is a title heading
        if (!is_heading && short_enough && !ends_sentence) {
            bool first_content = true;
            for (size_t j = 0; j < i; ++j) {
                if (raw[j].page_break) continue;
                std::string pj = trim(raw[j].text);
                if (!pj.empty()) { first_content = false; break; }
            }
            is_heading = first_content;
        }
        kind[i] = is_heading ? Heading : Para;
    }

    // output
    std::ofstream out(output);
    if (!out) {
        std::cerr << "Error: cannot write " << output << "\n";
        return false;
    }

    out << "# " << input.stem().string() << "\n\n"
        << "_Converted from PDF on " << __DATE__ << "_\n\n---\n\n";

    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].page_break) {
            out << "\n---\n\n";
            continue;
        }

        switch (kind[i]) {
        case Blank:
            out << "\n";
            break;

        case Heading:
            out << "## " << trim(raw[i].text) << "\n\n";
            break;

        case Bullet: {
            std::string bt = trim(raw[i].text);
            // strip any leading "- ", "* ", or bullet Unicode chars
            while (!bt.empty()) {
                if (bt[0] == '-' || bt[0] == '*') {
                    if (bt.size() > 1 && bt[1] == ' ') bt.erase(0, 2);
                    else bt.erase(0, 1);
                } else if ((unsigned char)bt[0] == 0xE2 && bt.size() >= 3) {
                    // skip UTF-8 bullet chars (• ● etc.)
                    bt.erase(0, 3);
                    if (!bt.empty() && bt[0] == ' ') bt.erase(0, 1);
                } else break;
            }
            out << "- " << bt << "\n";
            break;
        }

        case Para: {
            // merge consecutive paragraph lines
            std::string merged = trim(raw[i].text);
            while (i + 1 < raw.size() && kind[i + 1] == Para) {
                ++i;
                if (!merged.empty() && merged.back() != ' ') merged += ' ';
                merged += trim(raw[i].text);
            }
            out << merged << "\n\n";
            break;
        }
        }
    }

    out.close();
    auto size = fs::file_size(output);
    std::cout << "Written: " << output << " (" << size << " bytes, "
              << doc->pages() << " pages)\n";
    return true;
}

// ═══════════════════════════════════════════════
//  Markdown → PDF  (libharu)
// ═══════════════════════════════════════════════

struct MdLine {
    enum Type { Heading, Paragraph, ListItem, CodeFence, CodeLine, Hr, Empty };
    Type type;
    int level;       // heading level, list indent
    std::string text;
};

static MdLine classify(const std::string& line) {
    std::string t = trim(line);
    if (t.empty())                     return {MdLine::Empty, 0, ""};
    if (t == "---" || t == "***")      return {MdLine::Hr, 0, ""};
    if (starts_with(t, "```"))         return {MdLine::CodeFence, 0, t.substr(3)};
    if (starts_with(t, "#")) {
        int lev = 0;
        while (lev < (int)t.size() && t[lev] == '#') ++lev;
        return {MdLine::Heading, lev, trim(t.substr(lev))};
    }
    if (starts_with(t, "- ") || starts_with(t, "* ")) {
        int indent = 0;
        for (auto c : line) { if (c == ' ') ++indent; else break; }
        return {MdLine::ListItem, indent / 2, trim(t.substr(2))};
    }
    return {MdLine::Paragraph, 0, t};
}

struct MdDoc {
    struct Block {
        enum Type { Heading, Paragraph, List, Code, Hr };
        Type type;
        int level;
        std::vector<std::string> lines;
    };
    std::vector<Block> blocks;
};

static MdDoc parse_markdown(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return {};

    MdDoc doc;
    MdDoc::Block cur{MdDoc::Block::Paragraph, 0, {}};
    bool in_code = false;
    std::string line;

    auto flush = [&] {
        if (!cur.lines.empty()) doc.blocks.push_back(std::move(cur));
        cur = {MdDoc::Block::Paragraph, 0, {}};
    };

    while (std::getline(in, line)) {
        auto ml = classify(line);

        if (ml.type == MdLine::CodeFence) {
            if (in_code) { in_code = false; flush(); }
            else         { in_code = true;  flush(); cur.type = MdDoc::Block::Code; }
            continue;
        }

        if (in_code) {
            cur.lines.push_back(line);
            continue;
        }

        switch (ml.type) {
        case MdLine::Empty:
            flush();
            break;
        case MdLine::Hr:
            flush();
            cur.type = MdDoc::Block::Hr;
            flush();
            break;
        case MdLine::Heading:
            flush();
            cur.type = MdDoc::Block::Heading;
            cur.level = ml.level;
            cur.lines.push_back(ml.text);
            flush();
            break;
        case MdLine::ListItem:
            if (cur.type != MdDoc::Block::List) { flush(); cur.type = MdDoc::Block::List; }
            cur.lines.push_back(ml.text);
            break;
        default:
        case MdLine::Paragraph:
            if (cur.type != MdDoc::Block::Paragraph) { flush(); cur.type = MdDoc::Block::Paragraph; }
            cur.lines.push_back(ml.text);
            break;
        }
    }
    if (!cur.lines.empty() || cur.type == MdDoc::Block::Hr)
        doc.blocks.push_back(std::move(cur));

    return doc;
}

// libharu error callback
static void haru_error(HPDF_STATUS error, HPDF_STATUS detail, void*) {
    std::cerr << "libharu error: " << std::hex << error << "/" << detail << "\n";
}

static HPDF_Page add_page(HPDF_Doc pdf, HPDF_Font font, float size) {
    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    HPDF_Page_SetFontAndSize(page, font, size);
    HPDF_Page_SetTextLeading(page, size * 1.5f);
    return page;
}

// strip markdown formatting markers
static std::string strip_md(const std::string& text) {
    std::string r;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) { r += text[++i]; ++i; continue; }
        if (text[i] == '*' || text[i] == '`' || text[i] == '_') { ++i; continue; }
        if (text[i] == '[') { ++i; while (i < text.size() && text[i] != ']') ++i; if (i < text.size()) ++i; continue; }
        if (text[i] == '!' && i + 1 < text.size() && text[i+1] == '[') { ++i; while (i < text.size() && text[i] != ')') ++i; if (i < text.size()) ++i; continue; }
        r += text[i++];
    }
    return r;
}

struct FontSet {
    HPDF_Font regular;
    HPDF_Font bold;
    HPDF_Font mono;
};

static FontSet load_fonts(HPDF_Doc pdf) {
    HPDF_UseUTFEncodings(pdf);

    auto load = [&](const char* path) -> HPDF_Font {
        const char* name = HPDF_LoadTTFontFromFile(pdf, path, HPDF_TRUE);
        return name ? HPDF_GetFont(pdf, name, "UTF-8") : nullptr;
    };

    return {
        load("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        load("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        load("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
    };
}

static constexpr float MARGIN = 50;
static constexpr float WIDTH  = 595;
static constexpr float HEIGHT = 842;

static void render_heading(HPDF_Page& page, float& y, const std::vector<std::string>& lines,
                           int level, HPDF_Doc pdf, const FontSet& fonts) {
    float sizes[] = {22, 16, 13, 12, 11, 10};
    int idx = std::min(level - 1, 5);
    HPDF_Font font = fonts.bold ? fonts.bold : fonts.regular;
    float size = sizes[idx];
    float lead = size * 1.6f;

    if (y - lead < 40) { page = add_page(pdf, fonts.regular, 12); y = HEIGHT - MARGIN; }

    HPDF_Page_SetFontAndSize(page, font, size);
    HPDF_Page_BeginText(page);
    float ty = y - size * 0.3f;
    for (auto& l : lines) {
        HPDF_Page_TextOut(page, MARGIN, ty, l.c_str());
        ty -= lead;
    }
    HPDF_Page_EndText(page);
    y = ty + size * 0.3f - size * 0.4f;
}

static void render_paragraph(HPDF_Page& page, float& y, const std::vector<std::string>& lines,
                             HPDF_Doc pdf, const FontSet& fonts) {
    float size = 10;
    float lead = size * 1.5f;

    for (auto& l : lines) {
        if (y - lead < 40) { page = add_page(pdf, fonts.regular, size); y = HEIGHT - MARGIN; }
        std::string clean = strip_md(l);
        HPDF_Page_SetFontAndSize(page, fonts.regular, size);
        HPDF_Page_BeginText(page);
        HPDF_Page_TextOut(page, MARGIN, y - size * 0.3f, clean.c_str());
        HPDF_Page_EndText(page);
        y -= lead;
    }
    y -= size * 0.3f;
}

static void render_list(HPDF_Page& page, float& y, const std::vector<std::string>& items,
                        HPDF_Doc pdf, const FontSet& fonts) {
    float size = 10;
    float lead = size * 1.5f;

    for (auto& item : items) {
        if (y - lead < 40) { page = add_page(pdf, fonts.regular, size); y = HEIGHT - MARGIN; }
        std::string clean = strip_md(item);
        HPDF_Page_SetFontAndSize(page, fonts.regular, size);
        HPDF_Page_BeginText(page);
        HPDF_Page_TextOut(page, MARGIN, y - size * 0.3f, ("• " + clean).c_str());
        HPDF_Page_EndText(page);
        y -= lead;
    }
    y -= size * 0.3f;
}

static void render_code(HPDF_Page& page, float& y, const std::vector<std::string>& lines,
                        HPDF_Doc pdf, const FontSet& fonts) {
    float size = 9;
    float lead = size * 1.4f;

    for (auto& l : lines) {
        if (y - lead < 40) { page = add_page(pdf, fonts.mono, size); y = HEIGHT - MARGIN; }
        HPDF_Page_SetFontAndSize(page, fonts.mono, size);
        HPDF_Page_BeginText(page);
        HPDF_Page_TextOut(page, MARGIN + 10, y - size * 0.3f, l.c_str());
        HPDF_Page_EndText(page);
        y -= lead;
    }
    y -= size * 0.5f;
}

static void render_hr(HPDF_Page& page, float& y, HPDF_Doc pdf, const FontSet& fonts) {
    (void)fonts;
    if (y < 60) { page = add_page(pdf, fonts.regular, 10); y = HEIGHT - MARGIN; }
    float yy = y - 6;
    HPDF_Page_SetLineWidth(page, 0.5f);
    HPDF_Page_MoveTo(page, MARGIN, yy);
    HPDF_Page_LineTo(page, WIDTH - MARGIN, yy);
    HPDF_Page_Stroke(page);
    y -= 16;
}

static bool md_to_pdf(const fs::path& input, const fs::path& output) {
    MdDoc doc = parse_markdown(input);
    if (doc.blocks.empty()) {
        std::cerr << "Error: empty markdown file\n";
        return false;
    }

    HPDF_Doc pdf = HPDF_New(haru_error, nullptr);
    if (!pdf) {
        std::cerr << "Error: cannot create PDF\n";
        return false;
    }

    FontSet fonts = load_fonts(pdf);
    if (!fonts.regular) {
        std::cerr << "Error: failed to load TTF font\n";
        HPDF_Free(pdf);
        return false;
    }

    HPDF_Page page = add_page(pdf, fonts.regular, 10);
    float y = HEIGHT - MARGIN;

    auto block_spacing = [&]() {
        if (y < 50) { page = add_page(pdf, fonts.regular, 10); y = HEIGHT - MARGIN; }
        y -= 8;
    };

    for (auto& block : doc.blocks) {
        switch (block.type) {
        case MdDoc::Block::Heading:
            render_heading(page, y, block.lines, block.level, pdf, fonts);
            break;
        case MdDoc::Block::Paragraph:
            render_paragraph(page, y, block.lines, pdf, fonts);
            break;
        case MdDoc::Block::List:
            render_list(page, y, block.lines, pdf, fonts);
            break;
        case MdDoc::Block::Code:
            render_code(page, y, block.lines, pdf, fonts);
            break;
        case MdDoc::Block::Hr:
            render_hr(page, y, pdf, fonts);
            break;
        }
        block_spacing();
    }

    HPDF_SaveToFile(pdf, output.string().c_str());
    HPDF_Free(pdf);

    auto size = fs::file_size(output);
    std::cout << "Written: " << output << " (" << size << " bytes)\n";
    return true;
}

// ═══════════════════════════════════════════════
//  Mode detection
// ═══════════════════════════════════════════════

static Mode detect_mode(const fs::path& input, const fs::path& output) {
    auto ext = [](const fs::path& p) {
        std::string e = p.extension().string();
        std::transform(e.begin(), e.end(), e.begin(), ::tolower);
        return e;
    };
    if (ext(input) == ".pdf" && ext(output) == ".md")  return Mode::PdfToMd;
    if (ext(input) == ".md"  && ext(output) == ".pdf") return Mode::MdToPdf;
    std::cerr << "Warning: cannot detect direction, defaulting to PDF->MD\n";
    return Mode::PdfToMd;
}

static void print_help() {
    std::cout
        << "pdf2md v" << PROJECT_VERSION << " — PDF / Markdown converter\n"
        << "\n"
        << "Usage: pdf2md [options] <input> <output>\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help     Show this help\n"
        << "  -v, --version  Show version\n"
        << "\n"
        << "Direction is auto-detected from file extensions:\n"
        << "  pdf2md doc.pdf  doc.md    PDF -> Markdown\n"
        << "  pdf2md doc.md   doc.pdf   Markdown -> PDF\n";
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { print_help(); return 0; }
        if (arg == "-v" || arg == "--version") { std::cout << "pdf2md v" << PROJECT_VERSION << "\n"; return 0; }
    }

    if (argc < 3) { print_help(); return 1; }

    fs::path input  = argv[1];
    fs::path output = argv[2];

    if (!fs::exists(input)) {
        std::cerr << "Error: input not found: " << input << "\n";
        return 1;
    }

    Mode mode = detect_mode(input, output);
    bool ok;

    if (mode == Mode::PdfToMd) {
        std::cout << "PDF -> MD: " << input << " -> " << output << "\n";
        ok = pdf_to_md(input, output);
    } else {
        std::cout << "MD -> PDF: " << input << " -> " << output << "\n";
        ok = md_to_pdf(input, output);
    }

    return ok ? 0 : 1;
}

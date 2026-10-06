// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/md_render.hpp"

#include "pdf2md/text_utils.hpp"

#include <hpdf.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace pdf2md {

namespace {

// ── константы геометрии страницы ──

constexpr float kMargin = 50.0f;          // левый/правый отступ
constexpr float kBottomLimit = 40.0f;     // резерв места внизу страницы
constexpr float kBlockSpacing = 8.0f;     // вертикальный зазор между блоками
constexpr float kHrSpacing = 16.0f;       // вертикальное пространство вокруг ``---``
constexpr float kHrDrop = 6.0f;           // сдвиг линии hr ниже базовой линии
constexpr float kHrLineWidth = 0.5f;
constexpr float kCodeIndent = 10.0f;      // дополнительный левый отступ для кода
constexpr float kFontSizeBody = 10.0f;
constexpr float kFontSizeCode = 9.0f;
constexpr float kHeadingLeadingMult = 1.6f;
constexpr float kBodyLeadingMult = 1.5f;
constexpr float kCodeLeadingMult = 1.4f;
constexpr float kTextBaselineRatio = 0.3f;
constexpr float kHeadingTail = 0.4f;
constexpr float kCodeTail = 0.5f;

// размеры шрифта заголовков по уровню (уровень 1..6)
constexpr std::array<float, 6> kHeadingSizes = {22.0f, 16.0f, 13.0f, 12.0f, 11.0f, 10.0f};

constexpr std::string_view kBulletGlyph = "\xE2\x80\xA2 ";  // "• "

// ── вспомогательные функции libharu ──

void haru_error(HPDF_STATUS error, HPDF_STATUS detail, void*) {
    std::cerr << "libharu error: " << std::hex << error << "/" << detail << "\n";
}

struct PdfDoc {
    HPDF_Doc h{nullptr};
    PdfDoc()  { h = HPDF_New(haru_error, nullptr); }
    ~PdfDoc() { if (h) HPDF_Free(h); }
    PdfDoc(const PdfDoc&) = delete;
    PdfDoc& operator=(const PdfDoc&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

struct FontSet {
    HPDF_Font regular{nullptr};
    HPDF_Font bold{nullptr};
    HPDF_Font mono{nullptr};
};

HPDF_Font load_font(HPDF_Doc pdf, std::string_view path) {
    if (path.empty()) return nullptr;
    std::string p(path);
    const char* name = HPDF_LoadTTFontFromFile(pdf, p.c_str(), HPDF_TRUE);
    return name ? HPDF_GetFont(pdf, name, "UTF-8") : nullptr;
}

FontSet load_fonts(HPDF_Doc pdf, const FontPaths& paths) {
    HPDF_UseUTFEncodings(pdf);

    FontSet fonts;
    fonts.regular = load_font(pdf, paths.regular);
    fonts.bold = load_font(pdf, paths.bold);
    fonts.mono = load_font(pdf, paths.mono);

    // запасной вариант — regular, чтобы вызывающий код никогда не работал с нулевым шрифтом
    if (!fonts.bold) fonts.bold = fonts.regular;
    if (!fonts.mono) fonts.mono = fonts.regular;
    return fonts;
}

// libharu пишет из своего внутреннего потока в std::ofstream,
// который принимает std::filesystem::path, а значит и юникодные имена.
bool save_pdf_stream(HPDF_Doc pdf, const std::filesystem::path& output) {
    if (HPDF_SaveToStream(pdf) != HPDF_OK) return false;

    HPDF_UINT32 stream_size = HPDF_GetStreamSize(pdf);
    std::vector<HPDF_BYTE> buffer(stream_size, 0);
    HPDF_UINT32 bytes_read = stream_size;
    if (HPDF_ReadFromStream(pdf, buffer.data(), &bytes_read) != HPDF_OK) return false;
    buffer.resize(bytes_read);

    std::ofstream out(output, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(buffer.data()),
              static_cast<std::streamsize>(buffer.size()));
    return out.good();
}

// ── состояние страницы и курсора ──

// разбивает текст на строки, помещающиеся в max_width, с измерением тем же
// шрифтом/размером, которым текст будет нарисован; длинные одиночные слова
// принудительно разрываются
std::vector<std::string> wrap_text(HPDF_Page page, HPDF_Font font, float size,
                                   std::string_view text, float max_width) {
    HPDF_Page_SetFontAndSize(page, font, size);

    auto width = [&](std::string_view s) {
        return HPDF_Page_TextWidth(page, std::string(s).c_str());
    };
    const float space_w = width(" ");

    std::vector<std::string> words;
    for (std::size_t p = 0; p < text.size();) {
        while (p < text.size() &&
               (text[p] == ' ' || text[p] == '\t' || text[p] == '\n')) ++p;
        std::size_t start = p;
        while (p < text.size() &&
               !(text[p] == ' ' || text[p] == '\t' || text[p] == '\n')) ++p;
        if (p > start) words.emplace_back(text.substr(start, p - start));
    }

    std::vector<std::string> out;
    std::string line;
    double line_w = 0.0;

    for (const std::string& word : words) {
        double ww = width(word);
        double sep = line.empty() ? 0.0 : space_w;
        if (line_w + sep + ww <= max_width) {
            if (!line.empty()) line += ' ';
            line += word;
            line_w += sep + ww;
            continue;
        }
        if (!line.empty()) out.push_back(std::move(line));
        line.clear();
        line_w = 0.0;

        if (ww <= max_width) {
            line = word;
            line_w = ww;
        } else {
            // разрываем слишком длинное слово посимвольно
            std::string chunk;
            for (char ch : word) {
                chunk += ch;
                if (width(chunk) > max_width && chunk.size() > 1) {
                    out.push_back(chunk.substr(0, chunk.size() - 1));
                    chunk = chunk.substr(chunk.size() - 1);
                }
            }
            if (!chunk.empty()) { line = chunk; line_w = width(chunk); }
        }
    }
    if (!line.empty()) out.push_back(std::move(line));
    return out;
}

class Layout {
public:
    Layout(HPDF_Doc pdf, const FontSet& fonts)
        : pdf_(pdf), fonts_(fonts) {
        new_page(kFontSizeBody);
    }

    float page_width() const  { return HPDF_Page_GetWidth(page_); }
    float page_height() const { return HPDF_Page_GetHeight(page_); }
    float y() const { return y_; }
    HPDF_Page page() const { return page_; }

    void new_page(float size) {
        page_ = HPDF_AddPage(pdf_);
        HPDF_Page_SetSize(page_, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
        HPDF_Page_SetFontAndSize(page_, fonts_.regular, size);
        HPDF_Page_SetTextLeading(page_, size * kBodyLeadingMult);
        y_ = page_height() - kMargin;
    }

    void ensure_space(float needed, float size) {
        if (y_ - needed < kBottomLimit) new_page(size);
    }

    void ensure_space(float needed) { ensure_space(needed, kFontSizeBody); }

    // рисует одну строку текста в текущей позиции курсора
    void draw_text(float x, float size, HPDF_Font font, std::string_view text) {
        HPDF_Page_SetFontAndSize(page_, font, size);
        HPDF_Page_BeginText(page_);
        HPDF_Page_TextOut(page_, x, y_ - size * kTextBaselineRatio,
                          std::string(text).c_str());
        HPDF_Page_EndText(page_);
    }

    void advance(float dy) { y_ -= dy; }

    void block_spacing() {
        ensure_space(kBlockSpacing, kFontSizeBody);
        y_ -= kBlockSpacing;
    }

    void draw_block(float font_size, float lead_mult, HPDF_Font font,
                    float x, float max_width, std::string_view text) {
        float lead = font_size * lead_mult;
        for (const std::string& line : wrap_text(page_, font, font_size, text, max_width)) {
            ensure_space(lead, font_size);
            draw_text(x, font_size, font, line);
            advance(lead);
        }
    }

private:
    HPDF_Doc pdf_;
    const FontSet& fonts_;
    HPDF_Page page_{nullptr};
    float y_{0.0f};
};

} // namespace

struct PdfRenderer::Impl {
    FontPaths paths;
    PdfDoc doc;
    FontSet fonts{};

    explicit Impl(const FontPaths& p) : paths(p) {
        if (doc) fonts = load_fonts(doc.h, paths);
    }
    bool ready() const { return doc && fonts.regular; }
};

PdfRenderer::PdfRenderer(const FontPaths& fonts)
    : impl_(std::make_unique<Impl>(fonts)) {}

PdfRenderer::~PdfRenderer() = default;

bool PdfRenderer::is_ready() const { return impl_->ready(); }

bool PdfRenderer::render(const MdDoc& doc, const std::filesystem::path& output) const {
    if (!is_ready()) {
        std::cerr << "Error: failed to load TTF font\n";
        return false;
    }

    Layout layout(impl_->doc.h, impl_->fonts);
    const float text_w = layout.page_width() - 2.0f * kMargin;

    for (const auto& block : doc.blocks) {
        switch (block.type) {
        case MdBlock::Type::Heading: {
            int idx = std::clamp(block.level - 1, 0, static_cast<int>(kHeadingSizes.size()) - 1);
            float size = kHeadingSizes[static_cast<std::size_t>(idx)];
            for (const auto& l : block.lines)
                layout.draw_block(size, kHeadingLeadingMult, impl_->fonts.bold,
                                  kMargin, text_w, MarkdownParser::strip_md(l));
            layout.advance(size * kHeadingTail);
            break;
        }
        case MdBlock::Type::Paragraph:
            for (const auto& l : block.lines)
                layout.draw_block(kFontSizeBody, kBodyLeadingMult, impl_->fonts.regular,
                                  kMargin, text_w, MarkdownParser::strip_md(l));
            layout.advance(kFontSizeBody * kTextBaselineRatio);
            break;
        case MdBlock::Type::List:
            for (const auto& item : block.lines) {
                std::string line = std::string(kBulletGlyph) + MarkdownParser::strip_md(item);
                layout.draw_block(kFontSizeBody, kBodyLeadingMult, impl_->fonts.regular,
                                  kMargin, text_w, line);
            }
            layout.advance(kFontSizeBody * kTextBaselineRatio);
            break;
        case MdBlock::Type::Code:
            for (const auto& l : block.lines)
                layout.draw_block(kFontSizeCode, kCodeLeadingMult, impl_->fonts.mono,
                                  kMargin + kCodeIndent, text_w - kCodeIndent, l);
            layout.advance(kFontSizeCode * kCodeTail);
            break;
        case MdBlock::Type::Hr: {
            layout.ensure_space(kHrSpacing, kFontSizeBody);
            float yy = layout.y() - kHrDrop;
            HPDF_Page_SetLineWidth(layout.page(), kHrLineWidth);
            HPDF_Page_MoveTo(layout.page(), kMargin, yy);
            HPDF_Page_LineTo(layout.page(), layout.page_width() - kMargin, yy);
            HPDF_Page_Stroke(layout.page());
            layout.advance(kHrSpacing);
            break;
        }
        }
        layout.block_spacing();
    }

    if (!save_pdf_stream(impl_->doc.h, output)) {
        std::cerr << "Error: cannot write " << output << "\n";
        return false;
    }

    std::error_code ec;
    auto size = std::filesystem::file_size(output, ec);
    if (ec) {
        std::cerr << "Error: cannot stat " << output << ": " << ec.message() << "\n";
        return false;
    }
    std::cout << "Written: " << output << " (" << size << " bytes)\n";
    return true;
}

} // namespace pdf2md
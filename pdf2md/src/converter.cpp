// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/converter.hpp"

#include "pdf2md/md_parse.hpp"
#include "pdf2md/pdf_classify.hpp"
#include "pdf2md/pdf_extract.hpp"

#include <fstream>
#include <iostream>

namespace pdf2md {

FontPaths default_font_paths() {
    return {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
}

Converter::Converter(const std::filesystem::path& input,
                     const std::filesystem::path& output,
                     FontPaths fonts)
    : input_(input), output_(output), fonts_(fonts) {
    auto ext = [](const std::filesystem::path& p) { return p.extension(); };
    if (ext(input_) == ".pdf" && ext(output_) == ".md")       direction_ = Direction::PdfToMd;
    else if (ext(input_) == ".md" && ext(output_) == ".pdf")  direction_ = Direction::MdToPdf;
    else                                                       direction_ = Direction::Unknown;
}

Converter::Direction Converter::direction() const { return direction_; }

bool Converter::convert() const {
    switch (direction_) {
    case Direction::PdfToMd: return pdf_to_md();
    case Direction::MdToPdf: return md_to_pdf();
    case Direction::Unknown:
        std::cerr << "Error: cannot detect direction (use .pdf -> .md or .md -> .pdf)\n";
        return false;
    }
    return false;
}

bool Converter::pdf_to_md() const {
    PdfDocument pdf(input_);
    if (!pdf.is_valid()) {
        std::cerr << "Error: cannot open PDF: " << input_ << "\n";
        return false;
    }

    std::vector<RawLine> raw = pdf.extract_lines();
    if (raw.empty()) {
        std::cerr << "Error: no text found in PDF\n";
        return false;
    }

    LineClassifier classifier;
    std::vector<LineKind> kinds = classifier.classify(raw);
    std::string md = classifier.render(raw, kinds, input_.stem().string(), __DATE__);

    std::ofstream out(output_);
    if (!out) {
        std::cerr << "Error: cannot write " << output_ << "\n";
        return false;
    }
    out << md;
    out.close();

    std::error_code ec;
    auto size = std::filesystem::file_size(output_, ec);
    if (ec) {
        std::cerr << "Error: cannot stat " << output_ << ": " << ec.message() << "\n";
        return false;
    }
    std::cout << "Written: " << output_ << " (" << size << " bytes, "
              << pdf.page_count() << " pages)\n";
    return true;
}

bool Converter::md_to_pdf() const {
    MarkdownParser parser;
    MdDoc doc = parser.parse(input_);
    if (doc.blocks.empty()) {
        std::cerr << "Error: empty markdown file: " << input_ << "\n";
        return false;
    }

    PdfRenderer renderer(fonts_);
    if (!renderer.is_ready()) {
        std::cerr << "Error: failed to load TTF font\n";
        return false;
    }
    return renderer.render(doc, output_);
}

} // namespace pdf2md
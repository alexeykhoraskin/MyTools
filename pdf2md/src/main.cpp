// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/converter.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void print_help() {
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

} // namespace

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
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

    pdf2md::Converter converter(input, output);
    if (converter.direction() == pdf2md::Converter::Direction::Unknown) {
        std::cerr << "Error: cannot detect direction (use .pdf -> .md or .md -> .pdf)\n";
        return 1;
    }

    std::cout << (converter.direction() == pdf2md::Converter::Direction::PdfToMd
                      ? "PDF -> MD: "
                      : "MD -> PDF: ")
              << input << " -> " << output << "\n";

    return converter.convert() ? 0 : 1;
}
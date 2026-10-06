// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#include "pdf2md/pdf_extract.hpp"

#include "pdf2md/text_utils.hpp"

#include <poppler-document.h>
#include <poppler-page.h>

#include <sstream>
#include <string>

namespace pdf2md {

namespace {

constexpr std::size_t kMaxPageNumberLength = 4;

} // namespace

struct PdfDocument::Impl {
    std::filesystem::path input;
    std::unique_ptr<poppler::document> doc;
};

PdfDocument::PdfDocument(const std::filesystem::path& input)
    : impl_(std::make_unique<Impl>()) {
    impl_->input = input;
    impl_->doc.reset(poppler::document::load_from_file(input.string()));
}

PdfDocument::~PdfDocument() = default;

bool PdfDocument::is_valid() const {
    return impl_ && impl_->doc && !impl_->doc->is_locked();
}

int PdfDocument::page_count() const {
    return is_valid() ? impl_->doc->pages() : 0;
}

std::vector<RawLine> PdfDocument::extract_lines() const {
    std::vector<RawLine> lines;
    if (!is_valid()) return lines;

    for (int p = 0; p < impl_->doc->pages(); ++p) {
        std::unique_ptr<poppler::page> pg(impl_->doc->create_page(p));
        if (!pg) continue;

        if (p > 0) lines.push_back(RawLine{.text = {}, .page_break = true});

        auto u8 = pg->text().to_utf8();
        std::string text(u8.data(), u8.size());
        std::istringstream stream(text);
        std::string line;

        while (std::getline(stream, line)) {
            std::string_view t = trim(line);
            if (t.empty()) { lines.push_back({}); continue; }
            if (t.size() <= kMaxPageNumberLength &&
                t.find_first_not_of("0123456789") == std::string_view::npos)
                continue;
            lines.push_back({std::move(line), false});
        }
    }
    return lines;
}

} // namespace pdf2md
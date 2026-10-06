// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include "pdf2md/pdf_classify.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace pdf2md {

// Оборачивает PDF-документ и извлекает из него сырые строки текста.
class PdfDocument {
public:
    explicit PdfDocument(const std::filesystem::path& input);
    ~PdfDocument();

    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;

    bool is_valid() const;
    int page_count() const;
    std::vector<RawLine> extract_lines() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pdf2md
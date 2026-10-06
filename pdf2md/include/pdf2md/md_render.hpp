// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include "pdf2md/md_parse.hpp"

#include <filesystem>
#include <memory>

namespace pdf2md {

struct FontPaths {
    std::string_view regular;
    std::string_view bold;
    std::string_view mono;
};

// Рендерит блоки Markdown в PDF-файл через libharu.
class PdfRenderer {
public:
    explicit PdfRenderer(const FontPaths& fonts);
    ~PdfRenderer();

    PdfRenderer(const PdfRenderer&) = delete;
    PdfRenderer& operator=(const PdfRenderer&) = delete;

    // возвращает false, если шрифты не удалось загрузить
    bool is_ready() const;

    bool render(const MdDoc& doc, const std::filesystem::path& output) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pdf2md
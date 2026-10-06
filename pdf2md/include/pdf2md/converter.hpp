// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include "pdf2md/md_render.hpp"

#include <filesystem>

namespace pdf2md {

FontPaths default_font_paths();

// Полный конвейер конвертации; направление определяется автоматически по расширениям файлов.
class Converter {
public:
    enum class Direction { PdfToMd, MdToPdf, Unknown };

    Converter(const std::filesystem::path& input,
              const std::filesystem::path& output,
              FontPaths fonts = default_font_paths());

    Direction direction() const;

    // выполняет конвертацию; возвращает false при ошибке
    bool convert() const;

private:
    bool pdf_to_md() const;
    bool md_to_pdf() const;

    std::filesystem::path input_;
    std::filesystem::path output_;
    FontPaths fonts_;
    Direction direction_;
};

} // namespace pdf2md
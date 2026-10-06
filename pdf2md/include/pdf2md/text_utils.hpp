// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md
#pragma once

#include <cstddef>
#include <string_view>

namespace pdf2md {

constexpr std::string_view kWhitespace = " \t\r\n";

constexpr unsigned char kUtf8ContinuationMask = 0xC0;
constexpr unsigned char kUtf8ContinuationByte = 0x80;

inline std::string_view ltrim(std::string_view s) {
    std::size_t first = s.find_first_not_of(kWhitespace);
    if (first == std::string_view::npos) return {};
    return s.substr(first);
}

inline std::string_view rtrim(std::string_view s) {
    std::size_t last = s.find_last_not_of(kWhitespace);
    if (last == std::string_view::npos) return {};
    return s.substr(0, last + 1);
}

inline std::string_view trim(std::string_view s) {
    return rtrim(ltrim(s));
}

// приблизительная визуальная длина UTF-8 строки (считает кодовые точки)
inline std::size_t vis_len(std::string_view s) {
    std::size_t n = 0;
    for (unsigned char c : s)
        if ((c & kUtf8ContinuationMask) != kUtf8ContinuationByte) ++n;
    return n;
}

} // namespace pdf2md
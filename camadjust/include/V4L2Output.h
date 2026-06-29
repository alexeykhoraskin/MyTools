// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class V4L2Output {
    int fd = -1;
    int width_ = 0;
    int height_ = 0;

public:
    V4L2Output() = default;
    ~V4L2Output();

    bool open(const std::string& device, int width, int height);
    bool write_frame(const cv::Mat& bgr_frame);
    void close();
    bool is_open() const { return fd >= 0; }
};

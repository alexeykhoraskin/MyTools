// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#pragma once

#include <opencv2/opencv.hpp>

class CameraProcessor {
    cv::VideoCapture cap;
    cv::Mat frame;
    cv::Mat gamma_lut;

public:
    double brightness = 0.0;
    double contrast   = 1.0;
    double saturation = 1.0;
    double hue_shift  = 0.0;
    double sharpness  = 0.0;
    double gamma_val  = 1.0;
    int    denoise    = 0;
    double wb_red     = 1.0;
    double wb_blue    = 1.0;

    CameraProcessor();
    ~CameraProcessor();

    bool open(int device);
    void close();
    bool is_open() const;

    cv::Mat process();

    int width()  const { return frame.empty() ? 640 : frame.cols; }
    int height() const { return frame.empty() ? 480 : frame.rows; }
};

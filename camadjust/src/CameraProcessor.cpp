// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#include "CameraProcessor.h"

CameraProcessor::CameraProcessor() {
    gamma_lut.create(1, 256, CV_8U);
}

CameraProcessor::~CameraProcessor() { close(); }

bool CameraProcessor::open(int device) {
    cap.open(device);
    if (cap.isOpened())
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    return cap.isOpened();
}

void CameraProcessor::close() { cap.release(); }
bool CameraProcessor::is_open() const { return cap.isOpened(); }

cv::Mat CameraProcessor::process() {
    if (!cap.read(frame)) return {};

    cv::Mat out = frame.clone();

    // gamma
    if (gamma_val != 1.0) {
        for (int i = 0; i < 256; ++i)
            gamma_lut.at<uchar>(i) = cv::saturate_cast<uchar>(
                std::pow(i / 255.0, gamma_val) * 255.0);
        cv::LUT(out, gamma_lut, out);
    }

    // white balance
    if (wb_red != 1.0 || wb_blue != 1.0) {
        std::vector<cv::Mat> ch;
        cv::split(out, ch);
        ch[2] *= wb_red;
        ch[0] *= wb_blue;
        cv::merge(ch, out);
    }

    // brightness / contrast
    if (brightness != 0.0 || contrast != 1.0)
        out.convertTo(out, -1, contrast, brightness);

    // hue + saturation
    if (saturation != 1.0 || hue_shift != 0.0) {
        cv::Mat hsv;
        cv::cvtColor(out, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        channels[1] = cv::min(channels[1] * saturation, 255.0);
        channels[0] = (channels[0] + hue_shift);
        cv::merge(channels, hsv);
        cv::cvtColor(hsv, out, cv::COLOR_HSV2BGR);
    }

    // denoise
    if (denoise > 0) {
        int h = denoise * 2;
        cv::fastNlMeansDenoisingColored(out, out, h, h, 7, 21);
    }

    // sharpness
    if (sharpness > 0.0) {
        cv::Mat kernel = (cv::Mat_<float>(3,3) <<
            0, -1, 0,
            -1, 5 + sharpness, -1,
            0, -1, 0);
        cv::filter2D(out, out, out.depth(), kernel);
    }

    return out;
}

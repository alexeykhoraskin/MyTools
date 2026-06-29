// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#include "V4L2Output.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

V4L2Output::~V4L2Output() { close(); }

bool V4L2Output::open(const std::string& device, int width, int height) {
    fd = ::open(device.c_str(), O_RDWR);
    if (fd < 0) return false;

    width_ = width;
    height_ = height;

    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    fmt.fmt.pix.colorspace  = V4L2_COLORSPACE_SRGB;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        close();
        return false;
    }

    struct v4l2_streamparm parm = {};
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.numerator   = 1;
    parm.parm.output.timeperframe.denominator = 30;
    ioctl(fd, VIDIOC_S_PARM, &parm);

    return true;
}

static void bgr_to_yuyv(const cv::Mat& bgr, cv::Mat& yuyv) {
    cv::Mat yuv;
    cv::cvtColor(bgr, yuv, cv::COLOR_BGR2YUV);

    yuyv.create(bgr.rows, bgr.cols, CV_8UC2);

    for (int r = 0; r < bgr.rows; ++r) {
        const auto* src = yuv.ptr<cv::Vec3b>(r);
        auto*       dst = yuyv.ptr<cv::Vec2b>(r);

        for (int c = 0; c < bgr.cols; c += 2) {
            dst[c][0] = src[c][0];   // Y0
            dst[c][1] = (c + 1 < bgr.cols)
                ? ((src[c][1] + src[c+1][1]) / 2)  // avg U
                : src[c][1];

            if (c + 1 < bgr.cols) {
                dst[c+1][0] = src[c+1][0];                      // Y1
                dst[c+1][1] = (src[c][2] + src[c+1][2]) / 2;    // avg V
            }
        }
    }
}

bool V4L2Output::write_frame(const cv::Mat& bgr_frame) {
    if (fd < 0) return false;

    cv::Mat yuyv;
    bgr_to_yuyv(bgr_frame, yuyv);
    return ::write(fd, yuyv.data, yuyv.total() * yuyv.elemSize()) > 0;
}

void V4L2Output::close() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

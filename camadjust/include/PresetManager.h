// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#pragma once

#include <QString>
#include <QStringList>

struct Preset {
    double brightness  = 0.0;
    double contrast    = 100.0;
    double saturation  = 100.0;
    double hue_shift   = 0.0;
    double sharpness   = 0.0;
    double gamma_val   = 100.0;
    int    denoise     = 0;
    double wb_red      = 100.0;
    double wb_blue     = 100.0;
    int    device_idx  = 0;
    int    v4l2_idx    = 5;
};

class PresetManager {
    QString dir_;
public:
    PresetManager();

    void save(const QString& name, const Preset& p);
    Preset load(const QString& name) const;
    QStringList list() const;
    void remove(const QString& name);

private:
    void seed_defaults();
};

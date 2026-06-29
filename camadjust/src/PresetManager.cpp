// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#include "PresetManager.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

PresetManager::PresetManager() {
    dir_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
         + "/presets";
    QDir().mkpath(dir_);
    seed_defaults();
}

static Preset red_face_fix() {
    Preset p;
    p.wb_red   = 70;
    p.wb_blue  = 130;
    p.saturation = 90;
    return p;
}

void PresetManager::seed_defaults() {
    if (!QFile::exists(dir_ + "/Red face fix.json"))
        save("Red face fix", red_face_fix());
}

static QJsonObject to_json(const Preset& p) {
    QJsonObject obj;
    obj["brightness"]  = p.brightness;
    obj["contrast"]    = p.contrast;
    obj["saturation"]  = p.saturation;
    obj["hue_shift"]   = p.hue_shift;
    obj["sharpness"]   = p.sharpness;
    obj["gamma_val"]   = p.gamma_val;
    obj["denoise"]     = p.denoise;
    obj["wb_red"]      = p.wb_red;
    obj["wb_blue"]     = p.wb_blue;
    obj["device_idx"]  = p.device_idx;
    obj["v4l2_idx"]    = p.v4l2_idx;
    return obj;
}

static Preset from_json(const QJsonObject& obj) {
    Preset p;
    p.brightness  = obj["brightness"].toDouble(0.0);
    p.contrast    = obj["contrast"].toDouble(100.0);
    p.saturation  = obj["saturation"].toDouble(100.0);
    p.hue_shift   = obj["hue_shift"].toDouble(0.0);
    p.sharpness   = obj["sharpness"].toDouble(0.0);
    p.gamma_val   = obj["gamma_val"].toDouble(100.0);
    p.denoise     = obj["denoise"].toInt(0);
    p.wb_red      = obj["wb_red"].toDouble(100.0);
    p.wb_blue     = obj["wb_blue"].toDouble(100.0);
    p.device_idx  = obj["device_idx"].toInt(0);
    p.v4l2_idx    = obj["v4l2_idx"].toInt(5);
    return p;
}

void PresetManager::save(const QString& name, const Preset& p) {
    QFile file(dir_ + "/" + name + ".json");
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(to_json(p)).toJson());
    file.close();
}

Preset PresetManager::load(const QString& name) const {
    QFile file(dir_ + "/" + name + ".json");
    if (file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        return from_json(doc.object());
    }
    return {};
}

QStringList PresetManager::list() const {
    QDir dir(dir_);
    QStringList names;
    for (auto f : dir.entryList({"*.json"}, QDir::Files, QDir::Name))
        names.append(f.replace(".json", ""));
    return names;
}

void PresetManager::remove(const QString& name) {
    QFile::remove(dir_ + "/" + name + ".json");
}

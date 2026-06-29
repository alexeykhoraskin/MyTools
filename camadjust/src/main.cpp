// Copyright (c) 2026 Alexey.Khoraskin@gmail.com
// Licensed under the Apache License, Version 2.0
// https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QDir>
#include <QFile>

#include "CameraProcessor.h"
#include "V4L2Output.h"
#include "PresetManager.h"

// ── Helpers ──

static int detect_v4l2loopback() {
    QDir sys("/sys/devices/virtual/video4linux");
    if (!sys.exists()) return -1;
    for (const auto& e : sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(sys.path() + "/" + e + "/name");
        if (f.open(QIODevice::ReadOnly)) {
            QString name = QString::fromUtf8(f.readAll()).trimmed().toLower();
            if (name.contains("dummy") || name.contains("v4l2loopback")) {
                return QStringView(e).mid(5).toInt(); // "video4" -> 4
            }
        }
    }
    return -1;
}

struct SliderDef {
    QSlider* slider;
    QLabel*  val;
};

static SliderDef make_slider(QVBoxLayout* parent,
    const char* name, int min, int max, int def)
{
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(name));
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(def);
    row->addWidget(slider, 1);
    auto* val = new QLabel(QString::number(def));
    val->setFixedWidth(40);
    row->addWidget(val);
    parent->addLayout(row);
    return {slider, val};
}

// ── Main window ──

class MainWindow : public QMainWindow {
    Q_OBJECT

    CameraProcessor cam;
    V4L2Output      vout;
    PresetManager   presets;

    QLabel*      preview;
    QComboBox*   device_box;
    QComboBox*   v4l2_box;
    QPushButton* start_btn;
    QTimer*      timer;

    SliderDef s_brightness, s_contrast, s_saturation, s_hue;
    SliderDef s_sharpness, s_gamma, s_denoise, s_wb_red, s_wb_blue;

    QComboBox* preset_box;
    QLineEdit* preset_name;

    bool v4l2_enabled = false;

public:
    MainWindow() {
        setWindowTitle("camadjust v" PROJECT_VERSION);
        setMinimumSize(900, 680);

        auto* central = new QWidget;
        setCentralWidget(central);
        auto* main_layout = new QVBoxLayout(central);

        // ── preview ──
        preview = new QLabel("No camera");
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumSize(640, 480);
        preview->setStyleSheet("background: #222; color: #888;");
        main_layout->addWidget(preview, 1);

        // ── controls ──
        auto* ctrl = new QGroupBox("Adjustments");
        auto* ctrl_l = new QVBoxLayout(ctrl);

        auto add_range = [&](const char* name, int min, int max, int def)
            -> SliderDef
        {
            return make_slider(ctrl_l, name, min, max, def);
        };

        s_brightness = add_range("Brightness", -100, 100, 0);
        s_contrast   = add_range("Contrast",    0, 300, 100);
        s_saturation = add_range("Saturation",  0, 300, 100);
        s_hue        = add_range("Hue shift",  -180, 180, 0);
        s_sharpness  = add_range("Sharpness",   0, 100, 0);
        s_gamma      = add_range("Gamma",      10, 500, 100);
        s_denoise    = add_range("Denoise",     0, 20, 0);
        s_wb_red     = add_range("WB red",     50, 300, 100);
        s_wb_blue    = add_range("WB blue",    50, 300, 100);

        main_layout->addWidget(ctrl);

        // ── v4l2 toggle ──
        auto* v4l2_row = new QHBoxLayout;
        auto* v4l2_cb = new QCheckBox("Output to virtual camera (v4l2loopback)");
        v4l2_row->addWidget(v4l2_cb);
        v4l2_box = new QComboBox;
        int detected = detect_v4l2loopback();
        for (int i = 0; i < 8; ++i) {
            bool exists = QFile::exists(QString("/dev/video%1").arg(i));
            v4l2_box->addItem(QString("/dev/video%1").arg(i));
            if (!exists)
                v4l2_box->setItemText(i, v4l2_box->itemText(i) + " (missing)");
        }
        v4l2_box->setCurrentIndex(detected >= 0 ? detected : 4);
        v4l2_row->addWidget(new QLabel("Output:"));
        v4l2_row->addWidget(v4l2_box);
        v4l2_row->addStretch();
        main_layout->addLayout(v4l2_row);

        // ── bottom bar ──
        auto* bottom = new QHBoxLayout;

        device_box = new QComboBox;
        for (int i = 0; i < 8; ++i) {
            bool exists = QFile::exists(QString("/dev/video%1").arg(i));
            device_box->addItem(QString("/dev/video%1").arg(i));
            if (!exists)
                device_box->setItemText(i, device_box->itemText(i) + " (missing)");
        }
        bottom->addWidget(new QLabel("Camera:"));
        bottom->addWidget(device_box);

        start_btn = new QPushButton("Start");
        bottom->addWidget(start_btn);

        bottom->addSpacing(20);

        // presets
        bottom->addWidget(new QLabel("Preset:"));
        preset_box = new QComboBox;
        preset_box->setEditable(true);
        preset_box->lineEdit()->setPlaceholderText("New preset...");
        bottom->addWidget(preset_box);

        auto* save_btn = new QPushButton("Save");
        auto* load_btn = new QPushButton("Load");
        auto* del_btn  = new QPushButton("Delete");
        bottom->addWidget(save_btn);
        bottom->addWidget(load_btn);
        bottom->addWidget(del_btn);

        bottom->addStretch();
        main_layout->addLayout(bottom);

        // ── timer ──
        timer = new QTimer(this);
        timer->setInterval(33);

        // ── connections ──
        connect(s_brightness.slider, &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_contrast.slider,   &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_saturation.slider, &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_hue.slider,        &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_sharpness.slider,  &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_gamma.slider,      &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_denoise.slider,    &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_wb_red.slider,     &QSlider::valueChanged, this, &MainWindow::update_from_ui);
        connect(s_wb_blue.slider,    &QSlider::valueChanged, this, &MainWindow::update_from_ui);

        connect(start_btn, &QPushButton::clicked, this, &MainWindow::toggle_camera);
        connect(v4l2_cb, &QCheckBox::toggled, this, [this](bool on){ v4l2_enabled = on; });

        connect(timer, &QTimer::timeout, this, &MainWindow::grab_frame);

        connect(save_btn, &QPushButton::clicked, this, &MainWindow::save_preset);
        connect(load_btn, &QPushButton::clicked, this, &MainWindow::load_preset);
        connect(del_btn,  &QPushButton::clicked, this, &MainWindow::delete_preset);

        refresh_preset_list();
    }

private slots:
    void update_from_ui() {
        auto v = [](const SliderDef& s) { return s.slider->value(); };
        s_brightness.val->setText(QString::number(v(s_brightness)));
        s_contrast.val->setText(QString::number(v(s_contrast)));
        s_saturation.val->setText(QString::number(v(s_saturation)));
        s_hue.val->setText(QString::number(v(s_hue)));
        s_sharpness.val->setText(QString::number(v(s_sharpness)));
        s_gamma.val->setText(QString::number(v(s_gamma)));
        s_denoise.val->setText(QString::number(v(s_denoise)));
        s_wb_red.val->setText(QString::number(v(s_wb_red)));
        s_wb_blue.val->setText(QString::number(v(s_wb_blue)));

        cam.brightness = v(s_brightness); // -100..100, applied as-is
        cam.contrast   = v(s_contrast)   / 100.0;
        cam.saturation = v(s_saturation) / 100.0;
        cam.hue_shift  = v(s_hue);
        cam.sharpness  = v(s_sharpness)  / 10.0;
        cam.gamma_val  = v(s_gamma)      / 100.0;
        cam.denoise    = v(s_denoise);
        cam.wb_red     = v(s_wb_red)     / 100.0;
        cam.wb_blue    = v(s_wb_blue)    / 100.0;
    }

    void toggle_camera() {
        if (cam.is_open()) {
            cam.close();
            vout.close();
            timer->stop();
            start_btn->setText("Start");
            preview->setText("No camera");
            return;
        }

        int dev = device_box->currentIndex();
        if (!cam.open(dev)) {
            QMessageBox::warning(this, "Error", "Cannot open camera " +
                device_box->currentText());
            return;
        }

        if (v4l2_enabled) {
            std::string devpath = v4l2_box->currentText().toStdString();
            if (!vout.open(devpath, cam.width(), cam.height())) {
                int det = detect_v4l2loopback();
                QString msg = "Cannot open " + QString::fromStdString(devpath);
                if (det >= 0)
                    msg += "\nDetected loopback at /dev/video" + QString::number(det)
                        + " — select it above";
                else
                    msg += "\nLoad module: sudo modprobe v4l2loopback";
                QMessageBox::warning(this, "v4l2loopback", msg);
            }
        }

        start_btn->setText("Stop");
        timer->start();
    }

    void grab_frame() {
        cv::Mat mat = cam.process();
        if (mat.empty()) return;

        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);

        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        preview->setPixmap(QPixmap::fromImage(img).scaled(
            preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        if (vout.is_open())
            vout.write_frame(mat);
    }

    // ── presets ──
    Preset read_preset_from_ui() const {
        auto v = [](const SliderDef& s) { return s.slider->value(); };
        Preset p;
        p.brightness  = v(s_brightness);
        p.contrast    = v(s_contrast);
        p.saturation  = v(s_saturation);
        p.hue_shift   = v(s_hue);
        p.sharpness   = v(s_sharpness);
        p.gamma_val   = v(s_gamma);
        p.denoise     = v(s_denoise);
        p.wb_red      = v(s_wb_red);
        p.wb_blue     = v(s_wb_blue);
        p.device_idx  = device_box->currentIndex();
        p.v4l2_idx    = v4l2_box->currentIndex();
        return p;
    }

    void apply_preset_to_ui(const Preset& p) {
        auto set = [](SliderDef& s, int val) {
            s.slider->setValue(val);
        };
        set(s_brightness, static_cast<int>(p.brightness));
        set(s_contrast,   static_cast<int>(p.contrast));
        set(s_saturation, static_cast<int>(p.saturation));
        set(s_hue,        static_cast<int>(p.hue_shift));
        set(s_sharpness,  static_cast<int>(p.sharpness));
        set(s_gamma,      static_cast<int>(p.gamma_val));
        set(s_denoise,    p.denoise);
        set(s_wb_red,     static_cast<int>(p.wb_red));
        set(s_wb_blue,    static_cast<int>(p.wb_blue));
        device_box->setCurrentIndex(p.device_idx);
        v4l2_box->setCurrentIndex(p.v4l2_idx);
        update_from_ui();
    }

    void save_preset() {
        QString name = preset_box->currentText().trimmed();
        if (name.isEmpty()) {
            QMessageBox::information(this, "Save", "Type preset name");
            return;
        }
        presets.save(name, read_preset_from_ui());
        refresh_preset_list();
        preset_box->setCurrentText(name);
    }

    void load_preset() {
        QString name = preset_box->currentText();
        if (name.isEmpty()) return;
        Preset p = presets.load(name);
        apply_preset_to_ui(p);
    }

    void delete_preset() {
        QString name = preset_box->currentText();
        if (name.isEmpty()) return;
        presets.remove(name);
        refresh_preset_list();
    }

    void refresh_preset_list() {
        QString current = preset_box->currentText();
        preset_box->clear();
        preset_box->addItems(presets.list());
        int idx = preset_box->findText(current);
        if (idx >= 0) preset_box->setCurrentIndex(idx);
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("camadjust");
    app.setOrganizationName("MyTools");

    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"

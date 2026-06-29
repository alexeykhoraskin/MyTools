Copyright (c) 2026 Alexey.Khoraskin@gmail.com  
Licensed under the Apache License, Version 2.0  
https://github.com/alexeykhoraskin/MyTools/tree/main/camadjust

# camadjust

Инструмент для коррекции видеопотока с USB-камеры в реальном времени.

## Возможности

- Захват видео с USB-камеры (/dev/videoN)
- Регулировка яркости, контраста, насыщенности, резкости
- Предпросмотр обработанного видео
- Вывод в виртуальную камеру (v4l2loopback) для использования в мессенджерах

## Зависимости

```bash
sudo apt install qt6-base-dev libopencv-dev
```

Для виртуальной камеры (опционально):

```bash
sudo apt install v4l2loopback-dkms v4l2loopback-utils
sudo modprobe -r v4l2loopback          # выгрузить, если уже загружен
sudo modprobe v4l2loopback exclusive_caps=1 card_label="CamAdjust"
```

## Сборка

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Использование

```bash
./build/camadjust
```

Выберите устройство камеры в выпадающем списке, настройте параметры изображения и нажмите "Start".

## Параметры

| Параметр    | Диапазон | Описание                   |
|-------------|----------|----------------------------|
| Brightness  | -100–100 | Яркость изображения        |
| Contrast    | 0–300    | Контраст                   |
| Saturation  | 0–300    | Насыщенность цветов        |
| Hue shift   | -180–180 | Сдвиг оттенка              |
| Sharpness   | 0–100    | Резкость                   |
| Gamma       | 0.1–5.0  | Гамма-коррекция            |
| Denoise     | 0–20     | Шумоподавление             |
| WB red      | 50–300%  | Усиление красного канала   |
| WB blue     | 50–300%  | Усиление синего канала     |

## Пресеты

Параметры можно сохранять и загружать через интерфейс.
Пресеты хранятся в `~/.config/MyTools/camadjust/presets/*.json`.

## Вывод в виртуальную камеру

1. Загрузите модуль v4l2loopback с параметрами (см. выше)
2. Запустите camadjust
3. Включите "Output to virtual camera"
4. В мессенджере выберите камеру "CamAdjust"

Параметр `exclusive_caps=1` обязателен — без него большинство мессенджеров (Chrome, Zoom, Telegram) не увидят виртуальную камеру.

# pdf2md

Copyright (c) 2026 Alexey.Khoraskin@gmail.com  
Licensed under the Apache License, Version 2.0  
https://github.com/alexeykhoraskin/MyTools/tree/main/pdf2md

Инструмент для двусторонней конвертации между PDF и Markdown на C++.

## Возможности

- **PDF → MD** — извлечение текста из PDF в Markdown (через Poppler)
- **MD → PDF** — преобразование Markdown в PDF (через libharu + собственный парсер)

## Зависимости (dev)

```bash
sudo apt install libpoppler-cpp-dev libhpdf-dev
```

## Сборка

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Использование

```bash
./build/pdf2md документ.pdf  документ.md    # PDF -> Markdown
./build/pdf2md документ.md   документ.pdf   # Markdown -> PDF
```

Направление определяется автоматически по расширениям файлов.

### Параметры

| Параметр        | Описание              |
|-----------------|-----------------------|
| `-h`, `--help`  | Показать справку      |
| `-v`, `--version`| Показать версию      |

## Поддерживаемый Markdown

- Заголовки (`#` .. `######`)
- Абзацы
- Жирный (`**text**`) и курсив (*italic*) — очищается от разметки в PDF
- Списки (`- item`)
- Блоки кода (`` ``` ``)
- Горизонтальные линии (`---`)

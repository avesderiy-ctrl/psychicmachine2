# THEE PSYCHICK MACHINE

VST3-плагин (JUCE 8) для FL Studio: психоделический мультиэффект для канала микшера.
Оформление — крест Psychick, череп из тест-таблицы, пентаграмма, хаос-звезда, глаз в треугольнике, сканлайны.
Анимация реагирует на уровень сигнала.

## Модули (порядок в цепочке)

| Секция | Что делает |
|---|---|
| **TRANSMUTE** | Смена тональности ±24 полутона (granular pitch shifter), параметр GRAIN — размер зерна: короче = «стекляннее», длиннее = плавнее |
| **SIGIL** | Кольцевая модуляция 20–4000 Hz: металл, колокола, инопланетные обертоны |
| **FIRE** | DRIVE — насыщение, FOLD — волновой фолдер: расширяет и искажает спектр |
| **ELIXIR** | «Кислотный» резонансный фильтр с LFO и огибающей входа (303-подобный сквик) |
| **VORTEX** | 6-ступенчатый фэйзер с обратной связью — вихрь |
| **SCRYING** | Шиммер: пинг-понг задержка с питч-сдвигом вверх, диффузией и высокочастотным фильтром — кристаллический хвост (CRYSTAL PITCH, FEEDBACK) |
| **SHATTER** | Децимация + битовое разрушение: ломаное стекло |
| **ASTRAL** | WIDTH (Mid/Side до 300%) и SPREAD (расстройка L/R ±25 cent + Haas-задержка) |
| **THEE TRIP** | Макро: медленный хаотичный дрейф высоты, глубины LFO фильтра и фэйзера |
| **THE GATE** | DRY/WET, OUTPUT, предохранительный лимитер (SAFETY LIMITER) |

Все параметры автоматизируются в FL Studio. Двойной клик по ручке — сброс значения.
Хвост шиммера продолжает звучать после остановки сигнала.

## Сборка (Windows)

Нужно: Visual Studio 2022 (Desktop development with C++) или Build Tools, CMake ≥ 3.22, Git.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PsychickMachine_VST3
```

JUCE скачается автоматически при первой конфигурации.
Готовый плагин: `build\PsychickMachine_artefacts\Release\VST3\THEE PSYCHICK MACHINE.vst3`

## Установка в FL Studio

1. Скопируйте папку `THEE PSYCHICK MACHINE.vst3` в `C:\Program Files\Common Files\VST3\` (нужны права администратора).
2. FL Studio → Options → Manage plugins → **Find installed plugins** (Verify plugins, при необходимости).
3. Загрузите в слот микшера: Mixer → слот эффекта → выберите THEE PSYCHICK MACHINE.

## Сборка без установки VS

Залейте папку в репозиторий GitHub — workflow `.github/workflows/build.yml` соберёт Windows- и macOS-версию,
готовый `.vst3` будет в Actions → Artifacts.

## Структура

```
Source/PsychickEngine.h   DSP (чистый C++, без JUCE)
Source/PluginProcessor.*  параметры, обмен с хостом
Source/PluginEditor.*     интерфейс
Assets/skull.png          череп (встраивается в плагин), Assets/cross.png — крест-референс
Tests/engine_test.cpp     тесты DSP (ctest)
```

## Тест DSP без JUCE

```
g++ -std=c++17 -O2 -ISource Tests/engine_test.cpp -o engine_test && ./engine_test
```

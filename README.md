# Outlast 1 — Internal ESP & Utility (DirectX 11 + Dear ImGui)

[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)](https://microsoft.com)
[![Engine](https://img.shields.io/badge/Engine-Unreal%20Engine%203.5-lightgrey.svg)](https://unrealengine.com)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20-orange.svg)](https://isocpp.org)
[![DirectX](https://img.shields.io/badge/Graphics-DirectX%2011%20%2F%209-green.svg)](https://microsoft.com)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Полнофункциональный **Internal DLL** чит и инструмент исследования для игры **Outlast 1** (движок **Unreal Engine 3.5**, Red Barrels). Написан на современном стандарте **C++20** с использованием прямого перехвата DirectX 11 SwapChain Present и пользовательского интерфейса **Dear ImGui**.

---

## 📸 Возможности (Features)

### 💀 Враги (Enemy ESP)
- **2D Bounding Boxes**: классические 2D боксы или угловые рамки (Corner Box), опциональная полупрозрачная заливка.
- **Snaplines**: линии привязки к противникам (с низа экрана или от прицела).
- **Health Bar**: точная шкала здоровья в реальном времени с цветовой индикацией.
- **Идентификация персонажей**:
  - Крис Уокер (*Chris Walker*)
  - Доктор Ричард Трагер (*Dr. Trager*)
  - Эдди Глускин (*Eddie Gluskin / Groom*)
  - Отец Мартин (*Father Martin*)
  - Волтрайдер (*Walrider*)
  - Обычные варианты и пациенты лечебницы (*Variant / Patient*)
- **Дистанция** в метрах с настраиваемым радиусом отрисовки.

### 🔋 Предметы и Квесты (Items & Pickups)
- **Батарейки**: подсветка батареек для прибора ночного видения (`Battery [NV]`).
- **Документы**: физические папки с документами на столах (`Document [Folder]`).
- **Заметки для камеры**: триггеры видеокамеры (`[Camcorder] Note Event`) с возможностью отключения, чтобы не захламлять обзор.
- **Квестовые предметы**: ключи, карточки допуска и квестовые объекты (`Key` / `Item`).
- **Умная фильтрация**: автоматическое скрытие уже подобранных предметов на основе флага движка `bHidden` (`AActor + 0xE8`).

### 🚪 Мир и Интерактивные объекты (World & Navigation)
- **Укрытия**: шкафчики (`Locker`) и кровати (`Bed`).
- **Квестовые механизмы**: вентили, насосы, генераторы, предохранители, рубильники, кнопки, лифты с отслеживанием статуса (`Quest` / `Used` / `Done`).
- **Двери**: подсветка дверей лечебницы.

### 🛡️ Эксплойты (Cheats)
- **God Mode (Бессмертие)**: перехват нативных функций движка `AOLHero::TakeDamage` (`0xD8A140`), `TryKillHero` (`0xD69870`) и `TryDecapitate` (`0xD697B0`). Блокирует любой урон, скриптовые захваты, отрывание головы и падения с высоты.

### ⚡ Плавность отрисовки (Zero-Jitter Sync)
- **Синхронная камера**: чтение матриц камеры происходит синхронно на кадре рендера `Present()`, исключая эффект «дрожания как в External» при резких движениях мыши.
- **Pixel Snapping**: математическое округление координат для идеальной резкости шрифтов и линий.

### 🔍 Диагностика и Inspector
- Таблица ближайших энтити в радиусе 30м с отображением их внутреннего класса UE3, имени актора и дистанции.
- Отладчик смещений: `GWorld`, `GEngine`, `GNames`, координаты камеры и угол обзора (FOV).
- Опция `Show Debug Info on ESP` для отображения имен классов прямо на экране игры.

---

## 🎮 Архитектура и движок

- **Движок**: **Unreal Engine 3 (UE 3.5)** x64.
- **Целевой исполняемый файл**: `OLGame.exe` (расположен в папке `Binaries\Win64\`).
- **Графический API**: **DirectX 11** (`d3d11.dll`, `dxgi.dll`).
- **Иерархия объектов UE3**:
  - `GWorld` (RVA `0x21F3848`) -> `UWorld` -> `PersistentLevel` (`0x50`) -> `Actors` (`0x88`).
  - `PlayerController` -> `PlayerCamera` (`0x438`) -> `POV.Location` (`0x4A8`), `POV.Rotation` (`0x4B4`), `FOV` (`0x4C0`).

---

## 🛠️ Сборка проекта

### Требования:
- **Windows 10 / 11 (x64)**
- **Visual Studio 2022** (с установленным набором «Разработка классических приложений на C++») или **Build Tools** с поддержкой **C++20**.

### Способ 1: Сборка через Visual Studio (Рекомендуется)
1. Откройте файл решения [`OutlastInternal.sln`](OutlastInternal.sln).
2. Выберите конфигурацию **Release** и платформу **x64**.
3. Нажмите `Ctrl + Shift + B` (Сборка -> Собрать решение).
4. Скомпилированная DLL будет находиться в:
   ```
   bin\Release\OutlastESP.dll
   ```

### Способ 2: Сборка через bat-скрипт
Запустите файл [`build.bat`](build.bat) — он автоматически найдет установленный MSBuild через `vswhere` и скомпилирует проект.

### Способ 3: Сборка через CMake
```powershell
cmake -B build -A x64
cmake --build build --config Release
```

---

## 💉 Использование (Инжект)

1. Запустите игру **Outlast 1** (`OLGame.exe`).
2. Используйте любой инжектор x64 (например, *Cheat Engine*, *Process Hacker / System Informer*, *Xenos*).
3. Выберите процесс `OLGame.exe`.
4. Укажите скомпилированный файл `bin\Release\OutlastESP.dll` и выполните инжект.

### Управление в игре:
| Клавиша | Действие |
|---|---|
| **`INSERT`** | Открыть / свернуть меню настроек чита (Dear ImGui) |
| **`END`** | Корректно выгрузить DLL из игры (восстановление хуков и памяти) |

---

## 📁 Структура репозитория

```
outlast_esp/
├── OutlastInternal.sln           # Файл решения Visual Studio 2022
├── OutlastInternal.vcxproj       # Файл проекта MSBuild C++
├── CMakeLists.txt                # Конфигурация для кроссплатформенной сборки CMake
├── build.bat                     # Скрипт быстрой сборки одной командой
├── README.md                     # Документация проекта
├── .gitignore                    # Исключения версионного контроля
├── ext/                          # Внешние библиотеки (включены в репозиторий)
│   ├── imgui/                    # Dear ImGui v1.91.8 (модули таблиц, DX11/Win32)
│   └── minhook/                  # MinHook (x64 перехват API и нативных функций)
├── src/                          # Исходный код чита
│   ├── dllmain.cpp               # Точка входа DllMain, потоки и логика выгрузки
│   ├── config.hpp                # Глобальная конфигурация и структуры настроек
│   ├── hooks/
│   │   ├── hooks.hpp             # Заголовки хуков DirectX и оконных процедур
│   │   └── hooks.cpp             # Хуки IDXGISwapChain::Present и WndProc
│   └── game/
│       ├── esp.hpp               # Логика сбора данных, кэш камеры и рендер
│       ├── esp.cpp               # Реализация математики, обход актеров и ImGui
│       ├── math.hpp              # Векторы, матрицы, алгоритм UE3 WorldToScreen
│       ├── offsets.hpp           # Смещения Unreal Engine 3 и нативные RVA
│       └── sdk.hpp               # Структуры данных и перечисления сущностей
└── tools/                        # Скрипты реверс-инжиниринга и анализа памяти (IDA/Python)
```

---

## ⚠️ Отказ от ответственности (Disclaimer)

Проект разработан исключительно в образовательных целях и для изучения архитектуры движка Unreal Engine 3, реверс-инжиниринга x64 и рендеринга DirectX 11.

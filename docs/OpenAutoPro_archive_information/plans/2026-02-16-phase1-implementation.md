# Phase 1: Skeleton & Android Auto — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a bootable Qt 6 head unit application that connects to Android Auto (wired + wireless), renders video projection, plays audio through PipeWire, and provides a basic navigable UI with day/night theming and INI config loading.

**Architecture:** Qt 6.8 QML application with C++17 backend. aasdk (SonOfGib fork) as git submodule handles the AA protocol. Boost.ASIO runs on worker threads for async I/O; Qt event loop runs on the main thread. Services (video, audio, input) bridge between aasdk channels and Qt rendering. Configuration loaded from OAP-compatible INI files.

**Tech Stack:** C++17, Qt 6.8 (Quick, Multimedia, Network), CMake 3.22+, Boost (system, log, asio), aasdk, libusb-1.0, protobuf, OpenSSL, PipeWire (via Qt 6 Multimedia GStreamer backend), GStreamer (H.264 decode)

---

## Development Environment

**Primary dev:** MINIMEES (Windows 10, WSL2 for cross-compilation)
**Target:** Raspberry Pi 4, RPi OS Trixie (Debian 13, 64-bit)
**Test display:** DFRobot 7" 1024x600 capacitive touch (HDMI + USB)

**Two build targets:**
1. **Desktop (x86_64):** Qt 6 on WSL2/Ubuntu for UI development. Stub backends for AA (no real phone connection). Used for rapid QML iteration.
2. **Pi (aarch64):** Cross-compiled or native-built for real hardware testing. Real AA connections, PipeWire audio, touch input.

**Phase 1 dev strategy:** Build UI shell on desktop first (Tasks 1-4), then add AA integration that requires Pi hardware (Tasks 5-8), then settings UI (Task 9), then integration test on Pi (Task 10).

---

## Task 1: Repository & Build System

**Goal:** Create the openauto-prodigy repo with CMake build system, aasdk submodule, and a "hello world" Qt 6 Quick window that compiles and runs.

**Files:**
- Create: `openauto-prodigy/CMakeLists.txt` (root)
- Create: `openauto-prodigy/src/main.cpp`
- Create: `openauto-prodigy/src/CMakeLists.txt`
- Create: `openauto-prodigy/qml/main.qml`
- Create: `openauto-prodigy/qml/CMakeLists.txt`
- Create: `openauto-prodigy/resources/resources.qrc`
- Create: `openauto-prodigy/.gitignore`
- Create: `openauto-prodigy/README.md`
- Create: `openauto-prodigy/LICENSE` (GPLv3)
- Submodule: `openauto-prodigy/libs/aasdk/` → SonOfGib's aasdk

### Step 1: Create GitHub repo and clone

```bash
cd /e/claude/personal/openautopro
gh repo create mrmees/openauto-prodigy --public --license gpl-3.0 --clone
cd openauto-prodigy
```

### Step 2: Add aasdk as git submodule

```bash
git submodule add https://github.com/SonOfGib/aasdk.git libs/aasdk
git submodule update --init --recursive
```

### Step 3: Create root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS
    Core Quick Multimedia Network
)

# Boost
find_package(Boost REQUIRED COMPONENTS system log)

# Protobuf
find_package(Protobuf REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

# libusb
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# aasdk submodule
add_subdirectory(libs/aasdk)

# Our application
add_subdirectory(src)
```

### Step 4: Create src/CMakeLists.txt

```cmake
qt_add_executable(openauto-prodigy
    main.cpp
)

qt_add_qml_module(openauto-prodigy
    URI OpenAutoProdigy
    VERSION 1.0
    QML_FILES
        ../qml/main.qml
    RESOURCES
        ../resources/resources.qrc
)

target_link_libraries(openauto-prodigy PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Multimedia
    Qt6::Network
    aasdk
    Boost::system
    Boost::log
    ${LIBUSB_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
)

target_include_directories(openauto-prodigy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBUSB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs/aasdk/include
)
```

### Step 5: Create minimal main.cpp

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");

    QQmlApplicationEngine engine;
    engine.loadFromModule("OpenAutoProdigy", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
```

### Step 6: Create minimal qml/main.qml

```qml
import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: "#1a1a2e"

    Text {
        anchors.centerIn: parent
        text: "OpenAuto Prodigy v0.1.0"
        color: "#e94560"
        font.pixelSize: 32
    }
}
```

### Step 7: Create .gitignore

```
build/
.cache/
CMakeLists.txt.user
*.o
*.so
*.deb
```

### Step 8: Create directory structure

```bash
mkdir -p src/core/{aa,audio,bt,mirror,obd,api,system,hw}
mkdir -p src/ui
mkdir -p qml/{components,controls,applications}
mkdir -p resources/icons
mkdir -p proto
mkdir -p config
mkdir -p systemd
mkdir -p cmake
mkdir -p tests
mkdir -p docs
```

### Step 9: Verify build (WSL2)

```bash
# In WSL2 with Qt 6 dev packages installed:
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Expected: Compiles successfully. Window displays on desktop (if X11/Wayland forwarding works) or at minimum cmake configure + compile succeeds.

### Step 10: Commit

```bash
git add -A
git commit -m "feat: initial project skeleton with Qt 6 + aasdk submodule"
```

**Note on aasdk compatibility:** aasdk is C++14 and uses its own CMake. It may need patches to build with our toolchain. If `add_subdirectory(libs/aasdk)` fails, we may need to:
- Build aasdk separately and link as external library
- Or patch its CMakeLists.txt (fork if needed)

Document any required patches in this task.

---

## Task 2: Configuration System

**Goal:** Load OAP-compatible INI config files into a C++ ConfigurationController that exposes typed properties. Unit-testable without Qt GUI.

**Files:**
- Create: `src/core/Configuration.hpp`
- Create: `src/core/Configuration.cpp`
- Create: `tests/test_configuration.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `config/openauto_system.ini` (default config)

### Step 1: Create default INI config

```ini
[AndroidAuto]
day_night_mode_controller=true
show_clock_in_android_auto=true
show_top_bar=true

[Display]
screen_type=STANDARD
handedness_of_traffic=LHD
screen_dpi=140

[Colors]
background_color=#1a1a2e
highlight_color=#e94560
control_background_color=#16213e
control_foreground_color=#0f3460
normal_font_color=#eaeaea
special_font_color=#e94560
description_font_color=#a0a0a0
bar_background_color=#16213e
control_box_background_color=#0f3460
gauge_indicator_color=#e94560
icon_color=#eaeaea
side_widget_background_color=#16213e
bar_shadow_color=#0a0a1a

[Colors_Night]
background_color=#0a0a1a
highlight_color=#c73650
control_background_color=#0d1829
control_foreground_color=#091833
normal_font_color=#c0c0c0
special_font_color=#c73650
description_font_color=#808080
bar_background_color=#0d1829
control_box_background_color=#091833
gauge_indicator_color=#c73650
icon_color=#c0c0c0
side_widget_background_color=#0d1829
bar_shadow_color=#050510

[Audio]
volume_step=5

[Bluetooth]
adapter_type=LOCAL

[System]
language=en_US
time_format=FORMAT_12H
```

### Step 2: Write failing test

```cpp
// tests/test_configuration.cpp
#include <QtTest/QtTest>
#include "core/Configuration.hpp"

class TestConfiguration : public QObject {
    Q_OBJECT
private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testDayNightColors();
    void testSaveRoundTrip();
};

void TestConfiguration::testLoadDefaults()
{
    oap::Configuration config;
    // Should have sensible defaults even without a file
    QCOMPARE(config.screenType(), oap::ScreenType::Standard);
    QCOMPARE(config.handednessOfTraffic(), oap::Handedness::LHD);
    QCOMPARE(config.timeFormat(), oap::TimeFormat::Format12H);
    QVERIFY(!config.backgroundColor(oap::ThemeMode::Day).isEmpty());
}

void TestConfiguration::testLoadFromFile()
{
    oap::Configuration config;
    config.load(QFINDTESTDATA("data/test_config.ini"));

    QCOMPARE(config.showClockInAndroidAuto(), true);
    QCOMPARE(config.volumeStep(), 5);
    QCOMPARE(config.bluetoothAdapterType(), oap::BluetoothAdapterType::Local);
    QCOMPARE(config.language(), QString("en_US"));
}

void TestConfiguration::testDayNightColors()
{
    oap::Configuration config;
    config.load(QFINDTESTDATA("data/test_config.ini"));

    // Day colors
    QCOMPARE(config.backgroundColor(oap::ThemeMode::Day), QString("#1a1a2e"));
    QCOMPARE(config.highlightColor(oap::ThemeMode::Day), QString("#e94560"));

    // Night colors should be different
    QCOMPARE(config.backgroundColor(oap::ThemeMode::Night), QString("#0a0a1a"));
    QCOMPARE(config.highlightColor(oap::ThemeMode::Night), QString("#c73650"));
}

void TestConfiguration::testSaveRoundTrip()
{
    oap::Configuration config;
    config.load(QFINDTESTDATA("data/test_config.ini"));

    QTemporaryFile tmp;
    tmp.open();
    config.save(tmp.fileName());

    oap::Configuration config2;
    config2.load(tmp.fileName());

    QCOMPARE(config.backgroundColor(oap::ThemeMode::Day),
             config2.backgroundColor(oap::ThemeMode::Day));
    QCOMPARE(config.volumeStep(), config2.volumeStep());
    QCOMPARE(config.language(), config2.language());
}

QTEST_MAIN(TestConfiguration)
#include "test_configuration.moc"
```

### Step 3: Run test to verify it fails

```bash
cd build && cmake .. && make test_configuration
./tests/test_configuration
```

Expected: FAIL — `Configuration` class doesn't exist yet.

### Step 4: Implement Configuration class

```cpp
// src/core/Configuration.hpp
#pragma once
#include <QString>
#include <QSettings>
#include <QColor>
#include <memory>

namespace oap {

enum class ScreenType { Standard, Wide };
enum class Handedness { LHD, RHD };
enum class TimeFormat { Format12H, Format24H };
enum class ThemeMode { Day, Night };
enum class BluetoothAdapterType { None, Local, Remote };
enum class DayNightController { Manual, Sensor, Clock, Gpio };

class Configuration {
public:
    Configuration();

    void load(const QString& filePath);
    void save(const QString& filePath) const;

    // [AndroidAuto]
    bool dayNightModeController() const;
    bool showClockInAndroidAuto() const;
    bool showTopBar() const;

    // [Display]
    ScreenType screenType() const;
    Handedness handednessOfTraffic() const;
    int screenDpi() const;

    // [Colors] — mode selects Day or Night
    QString backgroundColor(ThemeMode mode) const;
    QString highlightColor(ThemeMode mode) const;
    QString controlBackgroundColor(ThemeMode mode) const;
    QString controlForegroundColor(ThemeMode mode) const;
    QString normalFontColor(ThemeMode mode) const;
    QString specialFontColor(ThemeMode mode) const;
    QString descriptionFontColor(ThemeMode mode) const;
    QString barBackgroundColor(ThemeMode mode) const;
    QString controlBoxBackgroundColor(ThemeMode mode) const;
    QString gaugeIndicatorColor(ThemeMode mode) const;
    QString iconColor(ThemeMode mode) const;
    QString sideWidgetBackgroundColor(ThemeMode mode) const;
    QString barShadowColor(ThemeMode mode) const;

    // [Audio]
    int volumeStep() const;

    // [Bluetooth]
    BluetoothAdapterType bluetoothAdapterType() const;
    QString remoteAdapterAddress() const;

    // [System]
    QString language() const;
    TimeFormat timeFormat() const;

    // Setters for all of the above (follow same pattern)
    void setBackgroundColor(ThemeMode mode, const QString& color);
    // ... etc for each property

private:
    QString colorSection(ThemeMode mode) const;
    QString colorValue(ThemeMode mode, const QString& key, const QString& defaultValue) const;

    std::unique_ptr<QSettings> settings_;

    // Cached values with defaults
    struct {
        bool dayNightModeController = true;
        bool showClockInAndroidAuto = true;
        bool showTopBar = true;
        ScreenType screenType = ScreenType::Standard;
        Handedness handedness = Handedness::LHD;
        int screenDpi = 140;
        int volumeStep = 5;
        BluetoothAdapterType btAdapter = BluetoothAdapterType::Local;
        QString remoteAdapterAddress;
        QString language = "en_US";
        TimeFormat timeFormat = TimeFormat::Format12H;
    } values_;

    struct ColorSet {
        QString background = "#1a1a2e";
        QString highlight = "#e94560";
        QString controlBackground = "#16213e";
        QString controlForeground = "#0f3460";
        QString normalFont = "#eaeaea";
        QString specialFont = "#e94560";
        QString descriptionFont = "#a0a0a0";
        QString barBackground = "#16213e";
        QString controlBoxBackground = "#0f3460";
        QString gaugeIndicator = "#e94560";
        QString icon = "#eaeaea";
        QString sideWidgetBackground = "#16213e";
        QString barShadow = "#0a0a1a";
    };
    ColorSet dayColors_;
    ColorSet nightColors_;
};

} // namespace oap
```

### Step 5: Implement Configuration.cpp

Use `QSettings` with `QSettings::IniFormat` for loading/saving. Map section names to OAP's format. Parse enums from strings. Populate cached values from loaded settings with defaults as fallback.

Key implementation pattern:
```cpp
void Configuration::load(const QString& filePath)
{
    QSettings ini(filePath, QSettings::IniFormat);

    ini.beginGroup("AndroidAuto");
    values_.showClockInAndroidAuto = ini.value("show_clock_in_android_auto", true).toBool();
    // ... etc
    ini.endGroup();

    ini.beginGroup("Colors");
    dayColors_.background = ini.value("background_color", dayColors_.background).toString();
    // ... etc
    ini.endGroup();

    ini.beginGroup("Colors_Night");
    nightColors_.background = ini.value("background_color", nightColors_.background).toString();
    // ... etc
    ini.endGroup();
}
```

### Step 6: Run tests, verify pass

```bash
make test_configuration && ./tests/test_configuration
```

Expected: All 4 tests pass.

### Step 7: Commit

```bash
git add src/core/Configuration.hpp src/core/Configuration.cpp \
        tests/test_configuration.cpp tests/CMakeLists.txt \
        config/openauto_system.ini tests/data/test_config.ini
git commit -m "feat: INI configuration system with OAP-compatible format"
```

---

## Task 3: Theme Engine

**Goal:** ThemeController QML context object that provides the current color set (day or night) as QML properties. Mode can be toggled. All QML elements bind to these properties for automatic theme switching.

**Files:**
- Create: `src/ui/ThemeController.hpp`
- Create: `src/ui/ThemeController.cpp`
- Create: `tests/test_theme_controller.cpp`
- Modify: `src/main.cpp` — register ThemeController with QML engine

### Step 1: Write failing test

```cpp
// tests/test_theme_controller.cpp
#include <QtTest/QtTest>
#include "ui/ThemeController.hpp"
#include "core/Configuration.hpp"

class TestThemeController : public QObject {
    Q_OBJECT
private slots:
    void testDefaultsToDay();
    void testToggleMode();
    void testColorsChangeOnToggle();
    void testSignalsEmitted();
};

void TestThemeController::testDefaultsToDay()
{
    auto config = std::make_shared<oap::Configuration>();
    oap::ThemeController theme(config);

    QCOMPARE(theme.mode(), oap::ThemeMode::Day);
    QCOMPARE(theme.backgroundColor(), config->backgroundColor(oap::ThemeMode::Day));
}

void TestThemeController::testToggleMode()
{
    auto config = std::make_shared<oap::Configuration>();
    oap::ThemeController theme(config);

    theme.toggleMode();
    QCOMPARE(theme.mode(), oap::ThemeMode::Night);

    theme.toggleMode();
    QCOMPARE(theme.mode(), oap::ThemeMode::Day);
}

void TestThemeController::testColorsChangeOnToggle()
{
    auto config = std::make_shared<oap::Configuration>();
    config->load(QFINDTESTDATA("data/test_config.ini"));
    oap::ThemeController theme(config);

    QString dayBg = theme.backgroundColor();
    theme.toggleMode();
    QString nightBg = theme.backgroundColor();

    QVERIFY(dayBg != nightBg);
}

void TestThemeController::testSignalsEmitted()
{
    auto config = std::make_shared<oap::Configuration>();
    oap::ThemeController theme(config);

    QSignalSpy spy(&theme, &oap::ThemeController::modeChanged);
    theme.toggleMode();
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestThemeController)
#include "test_theme_controller.moc"
```

### Step 2: Implement ThemeController

```cpp
// src/ui/ThemeController.hpp
#pragma once
#include <QObject>
#include <QQmlEngine>
#include <memory>
#include "core/Configuration.hpp"

namespace oap {

class ThemeController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ThemeMode mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString backgroundColor READ backgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString highlightColor READ highlightColor NOTIFY colorsChanged)
    Q_PROPERTY(QString controlBackgroundColor READ controlBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString controlForegroundColor READ controlForegroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString normalFontColor READ normalFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QString specialFontColor READ specialFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QString descriptionFontColor READ descriptionFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QString barBackgroundColor READ barBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString controlBoxBackgroundColor READ controlBoxBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString gaugeIndicatorColor READ gaugeIndicatorColor NOTIFY colorsChanged)
    Q_PROPERTY(QString iconColor READ iconColor NOTIFY colorsChanged)
    Q_PROPERTY(QString sideWidgetBackgroundColor READ sideWidgetBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QString barShadowColor READ barShadowColor NOTIFY colorsChanged)

public:
    explicit ThemeController(std::shared_ptr<Configuration> config,
                             QObject *parent = nullptr);

    ThemeMode mode() const;
    QString backgroundColor() const;
    QString highlightColor() const;
    QString controlBackgroundColor() const;
    QString controlForegroundColor() const;
    QString normalFontColor() const;
    QString specialFontColor() const;
    QString descriptionFontColor() const;
    QString barBackgroundColor() const;
    QString controlBoxBackgroundColor() const;
    QString gaugeIndicatorColor() const;
    QString iconColor() const;
    QString sideWidgetBackgroundColor() const;
    QString barShadowColor() const;

    Q_INVOKABLE void toggleMode();
    Q_INVOKABLE void setMode(ThemeMode mode);

signals:
    void modeChanged();
    void colorsChanged();

private:
    std::shared_ptr<Configuration> config_;
    ThemeMode mode_ = ThemeMode::Day;
};

} // namespace oap
```

Implementation: Each color getter delegates to `config_->xxxColor(mode_)`. `toggleMode()` flips mode and emits both signals.

### Step 3: Register with QML engine

In `main.cpp`, create ThemeController and set it as a context property or singleton:

```cpp
auto config = std::make_shared<oap::Configuration>();
config->load(configPath);

auto theme = new oap::ThemeController(config);
engine.rootContext()->setContextProperty("ThemeController", theme);
```

### Step 4: Run tests, verify pass

```bash
make test_theme_controller && ./tests/test_theme_controller
```

### Step 5: Commit

```bash
git add src/ui/ThemeController.hpp src/ui/ThemeController.cpp \
        tests/test_theme_controller.cpp
git commit -m "feat: theme engine with day/night color switching"
```

---

## Task 4: QML UI Shell

**Goal:** Main application screen with top bar, content area (application stack), and bottom bar. Launcher screen with navigation tiles. Desktop-testable — no AA connection needed.

This task creates the core QML component hierarchy that all other screens plug into.

**Files:**
- Create: `qml/main.qml` (replace hello world)
- Create: `qml/components/TopBar.qml`
- Create: `qml/components/BottomBar.qml`
- Create: `qml/components/Clock.qml`
- Create: `qml/components/Wallpaper.qml`
- Create: `qml/controls/NormalText.qml`
- Create: `qml/controls/SpecialText.qml`
- Create: `qml/controls/Icon.qml`
- Create: `qml/controls/Tile.qml`
- Create: `qml/controls/CustomButton.qml`
- Create: `qml/applications/ApplicationBase.qml`
- Create: `qml/applications/launcher/LauncherMenu.qml`
- Create: `qml/applications/home/HomeMenu.qml`
- Create: `qml/applications/android_auto/AndroidAutoMenu.qml`
- Create: `qml/applications/settings/SettingsMenu.qml`
- Create: `src/ui/ApplicationController.hpp`
- Create: `src/ui/ApplicationController.cpp`

### Step 1: Define the application types enum

```cpp
// src/ui/ApplicationTypes.hpp
#pragma once
#include <QObject>
#include <QQmlEngine>

namespace oap {

class ApplicationTypes : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Enum container")

public:
    enum Type {
        Launcher,
        Home,
        AndroidAuto,
        Music,
        Equalizer,
        Dashboards,
        Settings,
        Telephone,
        Exit
    };
    Q_ENUM(Type)
};

} // namespace oap
```

### Step 2: Create ApplicationController

C++ context object that manages the application stack (which screen is active):

```cpp
// src/ui/ApplicationController.hpp
#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QStack>
#include "ApplicationTypes.hpp"

namespace oap {

class ApplicationController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int currentApplication READ currentApplication NOTIFY currentApplicationChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentTitleChanged)

public:
    explicit ApplicationController(QObject *parent = nullptr);

    int currentApplication() const;
    QString currentTitle() const;

    Q_INVOKABLE void navigateTo(int appType);
    Q_INVOKABLE void navigateBack();
    Q_INVOKABLE void setTitle(const QString& title);

signals:
    void currentApplicationChanged();
    void currentTitleChanged();

private:
    QStack<ApplicationTypes::Type> stack_;
    QString title_;
};

} // namespace oap
```

### Step 3: Build main.qml — the root layout

```qml
import QtQuick
import QtQuick.Layouts

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: ThemeController.backgroundColor

    // Background wallpaper (behind everything)
    Wallpaper {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top bar: title, clock, status icons
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.10
        }

        // Content area: current application
        StackView {
            id: appStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            initialItem: launcherComponent
        }

        // Bottom bar: volume control
        BottomBar {
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.12
        }
    }

    Component {
        id: launcherComponent
        LauncherMenu {}
    }

    // Navigation handler
    Connections {
        target: ApplicationController
        function onCurrentApplicationChanged() {
            // Push appropriate component onto stack
        }
    }
}
```

### Step 4: Build TopBar.qml

```qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: topBar
    color: ThemeController.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: parent.width * 0.02
        anchors.rightMargin: parent.width * 0.02

        // Back button (visible when not on launcher)
        Icon {
            source: "qrc:/icons/back.svg"
            visible: ApplicationController.currentApplication !== 0
            Layout.preferredWidth: parent.height * 0.6
            Layout.preferredHeight: parent.height * 0.6
            MouseArea {
                anchors.fill: parent
                onClicked: ApplicationController.navigateBack()
            }
        }

        // Title
        SpecialText {
            text: ApplicationController.currentTitle
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        // Clock
        Clock {
            Layout.preferredWidth: parent.width * 0.12
        }
    }
}
```

### Step 5: Build LauncherMenu.qml

Grid of tiles for navigating to applications. Phase 1 shows: Android Auto, Settings, and placeholder tiles for Music, Phone, etc.

```qml
import QtQuick

Item {
    id: launcher

    Component.onCompleted: ApplicationController.setTitle("OpenAuto Prodigy")

    GridView {
        anchors.fill: parent
        anchors.margins: parent.width * 0.04
        cellWidth: width / 4
        cellHeight: height / 2

        model: ListModel {
            ListElement { name: "Android Auto"; icon: "android_auto"; appType: 2 }
            ListElement { name: "Music"; icon: "music"; appType: 3; enabled: false }
            ListElement { name: "Phone"; icon: "phone"; appType: 7; enabled: false }
            ListElement { name: "Settings"; icon: "settings"; appType: 6 }
        }

        delegate: Tile {
            required property string name
            required property string icon
            required property int appType
            width: GridView.view.cellWidth - 10
            height: GridView.view.cellHeight - 10
            tileName: name
            tileIcon: icon
            onClicked: ApplicationController.navigateTo(appType)
        }
    }
}
```

### Step 6: Build base controls (NormalText, SpecialText, Icon, Tile, CustomButton)

Each follows the OAP pattern of binding to ThemeController colors:

```qml
// controls/NormalText.qml
import QtQuick
Text {
    color: ThemeController.normalFontColor
    font.pixelSize: parent ? parent.height * 0.04 : 14
}

// controls/SpecialText.qml
import QtQuick
Text {
    color: ThemeController.specialFontColor
    font.pixelSize: parent ? parent.height * 0.05 : 18
    font.bold: true
}

// controls/Icon.qml
import QtQuick
import Qt5Compat.GraphicalEffects  // or Qt6 equivalent for ColorOverlay
Image {
    property string iconColor: ThemeController.iconColor
    fillMode: Image.PreserveAspectFit
    // ColorOverlay to tint SVG icons with theme color
}

// controls/Tile.qml
import QtQuick
Rectangle {
    property string tileName
    property string tileIcon
    signal clicked()
    color: mouseArea.containsMouse ? ThemeController.highlightColor
                                   : ThemeController.controlBackgroundColor
    radius: 8
    // Icon + NormalText centered
    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: true; onClicked: parent.clicked() }
}
```

### Step 7: Build placeholder application screens

- `AndroidAutoMenu.qml` — "Connect Android Auto" message (AA integration comes in Tasks 5-8)
- `SettingsMenu.qml` — Grid of settings categories (detailed settings UI in Task 9)
- `HomeMenu.qml` — Simple dashboard placeholder

### Step 8: Wire ApplicationController into main.cpp

```cpp
auto appController = new oap::ApplicationController();
engine.rootContext()->setContextProperty("ApplicationController", appController);
```

### Step 9: Verify on desktop

```bash
cd build && cmake .. && make -j$(nproc)
./openauto-prodigy
```

Expected: Window shows launcher grid. Clicking "Android Auto" navigates to AA placeholder. Clicking "Settings" navigates to settings placeholder. Back button returns to launcher. Day/night toggle (keyboard shortcut for now) changes all colors.

### Step 10: Commit

```bash
git add qml/ src/ui/ resources/
git commit -m "feat: QML UI shell with launcher, navigation, and theme integration"
```

---

## Task 5: Android Auto Service Layer

**Goal:** Core AA lifecycle management — ASIO integration with Qt event loop, USB device detection via AOAP protocol, control channel handshake, and service discovery. No video/audio yet — just successful protocol negotiation.

**This is the hardest integration task.** The aasdk async model (Boost.ASIO + promises) must coexist with Qt's event loop without deadlocks.

**Files:**
- Create: `src/core/aa/AndroidAutoService.hpp`
- Create: `src/core/aa/AndroidAutoService.cpp`
- Create: `src/core/aa/ServiceFactory.hpp`
- Create: `src/core/aa/ServiceFactory.cpp`
- Create: `src/core/aa/ControlService.hpp`
- Create: `src/core/aa/ControlService.cpp`
- Modify: `src/main.cpp` — initialize ASIO, libusb, AA service

### Step 1: Understand the threading model

```
Main Thread (Qt):
  QGuiApplication::exec() — processes UI events, renders QML

Worker Threads (4x):
  boost::asio::io_service::run() — processes aasdk async operations
  All aasdk work dispatched through strand_ for serialization

USB Threads (4x):
  libusb_handle_events_timeout_completed() — USB I/O polling

Communication between threads:
  ASIO → Qt: QMetaObject::invokeMethod(obj, "slot", Qt::QueuedConnection)
  Qt → ASIO: strand_.dispatch([]{...})
```

### Step 2: Create AndroidAutoService (the App class equivalent)

This is the central coordinator, equivalent to `App` in the reference openauto.

```cpp
// src/core/aa/AndroidAutoService.hpp
#pragma once
#include <QObject>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <aasdk/USB/IUSBHub.hpp>
#include <aasdk/USB/AOAPDevice.hpp>
#include <aasdk/Transport/USBTransport.hpp>
#include <aasdk/Transport/TCPTransport.hpp>
#include <aasdk/Messenger/Messenger.hpp>
#include <aasdk/Channel/Control/ControlServiceChannel.hpp>

namespace oap::aa {

class AndroidAutoService : public QObject {
    Q_OBJECT

public:
    explicit AndroidAutoService(
        std::shared_ptr<Configuration> config,
        QObject *parent = nullptr);
    ~AndroidAutoService();

    void start();
    void stop();

    // Connect wirelessly to given IP
    void connectWireless(const QString& ipAddress);

signals:
    void deviceConnected();
    void deviceDisconnected();
    void connectionFailed(const QString& error);
    void projectionStarted();   // video channel opened
    void projectionStopped();

private:
    void initializeAsio();
    void waitForUsbDevice();
    void onDeviceFound(aasdk::usb::DeviceHandle handle);
    void onTransportReady(std::shared_ptr<aasdk::transport::ITransport> transport);
    void startServices();
    void stopServices();

    std::shared_ptr<Configuration> config_;
    boost::asio::io_service ioService_;
    boost::asio::io_service::work work_;  // prevent io_service from stopping
    boost::asio::io_service::strand strand_;
    std::vector<std::thread> workerThreads_;
    std::vector<std::thread> usbThreads_;

    // aasdk objects (created on connection)
    std::shared_ptr<aasdk::messenger::IMessenger> messenger_;
    // ... service channels
};

} // namespace oap::aa
```

### Step 3: Implement ASIO + Qt integration

```cpp
void AndroidAutoService::initializeAsio()
{
    // Start ASIO worker threads
    for (int i = 0; i < 4; ++i) {
        workerThreads_.emplace_back([this]() {
            ioService_.run();
        });
    }

    // Start USB polling threads
    for (int i = 0; i < 4; ++i) {
        usbThreads_.emplace_back([this]() {
            timeval tv{1, 0};  // 1 second timeout
            while (!stopping_) {
                libusb_handle_events_timeout_completed(usbContext_, &tv, nullptr);
            }
        });
    }
}
```

### Step 4: Implement USB device detection

Follow the reference pattern:
1. Create `USBHub` that watches for AOAP devices
2. On device found → create `AOAPDevice` → create `USBTransport`
3. Create `Messenger` on top of transport
4. Emit `deviceConnected()`

### Step 5: Implement ControlService

Handles the AA protocol handshake:
1. Send version request
2. Receive version response
3. SSL handshake exchange
4. Auth complete
5. Receive service discovery request → respond with our capabilities
6. Audio focus management

```cpp
// src/core/aa/ControlService.hpp
class ControlService : public aasdk::channel::control::IControlServiceChannelEventHandler {
public:
    // Implement all event handler methods
    void onVersionResponse(uint16_t major, uint16_t minor,
                          aasdk::proto::messages::VersionResponseStatus status) override;
    void onHandshake(const aasdk::common::DataConstBuffer& payload) override;
    void onServiceDiscoveryRequest(const aasdk::proto::messages::ServiceDiscoveryRequest& request) override;
    void onAudioFocusRequest(const aasdk::proto::messages::AudioFocusRequest& request) override;
    void onShutdownRequest(const aasdk::proto::messages::ShutdownRequest& request) override;
    void onPingRequest(const aasdk::proto::messages::PingRequest& request) override;
    // ... etc
};
```

### Step 6: Service Discovery response

Tell the phone which channels we support:

```cpp
void ControlService::onServiceDiscoveryRequest(...)
{
    aasdk::proto::messages::ServiceDiscoveryResponse response;

    // Video channel
    auto* videoChannel = response.add_channels();
    videoChannel->set_channel_type(aasdk::proto::enums::ChannelType::VIDEO);
    // Set supported resolutions, FPS, DPI from config

    // Media audio
    auto* mediaAudio = response.add_channels();
    mediaAudio->set_channel_type(aasdk::proto::enums::ChannelType::MEDIA_AUDIO);
    // 48kHz, stereo, 16-bit

    // Speech audio
    auto* speechAudio = response.add_channels();
    speechAudio->set_channel_type(aasdk::proto::enums::ChannelType::SPEECH_AUDIO);
    // 16kHz, mono, 16-bit

    // System audio
    auto* systemAudio = response.add_channels();
    systemAudio->set_channel_type(aasdk::proto::enums::ChannelType::SYSTEM_AUDIO);

    // Input channel
    auto* input = response.add_channels();
    input->set_channel_type(aasdk::proto::enums::ChannelType::INPUT);

    // Bluetooth channel
    auto* bt = response.add_channels();
    bt->set_channel_type(aasdk::proto::enums::ChannelType::BLUETOOTH);

    controlChannel_->sendServiceDiscoveryResponse(response, sendPromise);
}
```

### Step 7: Test on Pi with real phone

This task doesn't have traditional unit tests — it's integration-level. Test by:
1. Build on Pi (or cross-compile)
2. Connect Android phone via USB
3. Observe logs: version handshake, SSL exchange, service discovery
4. Phone should show "Android Auto starting..." then complain about missing video (expected — we haven't implemented video yet)

### Step 8: Commit

```bash
git add src/core/aa/
git commit -m "feat: Android Auto service layer with ASIO integration and control channel"
```

---

## Task 6: Video Projection

**Goal:** Receive H.264 video from phone via aasdk video channel, decode it, and render fullscreen in QML. Touch events from the QML surface sent back to phone.

**Files:**
- Create: `src/core/aa/VideoService.hpp`
- Create: `src/core/aa/VideoService.cpp`
- Create: `src/core/aa/InputService.hpp`
- Create: `src/core/aa/InputService.cpp`
- Create: `qml/components/ProjectionSurface.qml`
- Modify: `qml/applications/android_auto/AndroidAutoMenu.qml`
- Modify: `src/core/aa/ServiceFactory.cpp`

### Step 1: Understand Qt 6 video pipeline

Qt 6 changed the video API significantly from Qt 5:
- `QMediaPlayer` + `QVideoSink` (replaces QVideoWidget)
- `QVideoFrame` for direct frame access
- GStreamer backend on Linux handles H.264 decode

For AA video, we receive raw H.264 NAL units. Options:
1. **Feed to GStreamer pipeline** — most reliable for hardware decode on Pi
2. **Use QMediaPlayer with custom QIODevice** — simpler but may not handle raw H.264
3. **Direct GStreamer** — bypass Qt Multimedia, use GStreamer C API

**Recommended approach for Phase 1:** Use a GStreamer pipeline (`appsrc → h264parse → avdec_h264 → videoconvert → qtvideosink`) wrapped in a C++ class. This gives us hardware decode on Pi and works on desktop too.

Simpler alternative: Write H.264 to a `QBuffer` and use `QMediaPlayer`. Try this first.

### Step 2: Create VideoService

```cpp
// src/core/aa/VideoService.hpp
class VideoService : public QObject,
                     public aasdk::channel::av::IVideoServiceChannelEventHandler {
    Q_OBJECT

public:
    VideoService(boost::asio::io_service::strand& strand,
                 std::shared_ptr<aasdk::channel::av::VideoServiceChannel> channel,
                 std::shared_ptr<Configuration> config);

    void start();
    void stop();

    // IVideoServiceChannelEventHandler
    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest&) override;
    void onAVChannelSetupRequest(const proto::messages::AVChannelSetupRequest&) override;
    void onAVChannelStartIndication(const proto::messages::AVChannelStartIndication&) override;
    void onAVChannelStopIndication(const proto::messages::AVChannelStopIndication&) override;
    void onVideoFocusRequest(const proto::messages::VideoFocusRequest&) override;
    void onAVMediaWithTimestampIndication(aasdk::messenger::Timestamp::ValueType timestamp,
                                          const aasdk::common::DataConstBuffer& buffer) override;
    void onAVMediaIndication(const aasdk::common::DataConstBuffer& buffer) override;
    void onChannelError(const aasdk::error::Error& e) override;

signals:
    void videoFrameReceived(const QByteArray& h264Data);
    void projectionStarted(int width, int height);
    void projectionStopped();

private:
    void sendSetupResponse(int width, int height, int fps, int dpi);

    boost::asio::io_service::strand& strand_;
    std::shared_ptr<aasdk::channel::av::VideoServiceChannel> channel_;
    std::shared_ptr<Configuration> config_;
};
```

### Step 3: Implement video frame delivery

On `onAVMediaWithTimestampIndication` and `onAVMediaIndication`:
- Copy H.264 data to QByteArray
- Emit signal to Qt thread
- Send ACK back to phone

```cpp
void VideoService::onAVMediaWithTimestampIndication(
    aasdk::messenger::Timestamp::ValueType timestamp,
    const aasdk::common::DataConstBuffer& buffer)
{
    QByteArray h264Data(reinterpret_cast<const char*>(buffer.cdata), buffer.size);
    emit videoFrameReceived(h264Data);

    // ACK the frame
    auto ackPromise = aasdk::io::Promise<void>::defer(strand_);
    channel_->sendAVMediaAckIndication(ackPromise);

    // Continue receiving
    channel_->receive(this->shared_from_this());
}
```

### Step 4: Create ProjectionSurface.qml

QML component that displays the decoded video:

```qml
import QtQuick
import QtMultimedia

Item {
    id: projectionSurface

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
    }

    // Touch forwarding
    MultiPointTouchArea {
        anchors.fill: parent
        mouseEnabled: true

        onPressed: (touchPoints) => {
            for (let tp of touchPoints) {
                InputService.sendTouch(tp.x / width, tp.y / height, 1) // ACTION_DOWN
            }
        }
        onUpdated: (touchPoints) => {
            for (let tp of touchPoints) {
                InputService.sendTouch(tp.x / width, tp.y / height, 2) // ACTION_MOVE
            }
        }
        onReleased: (touchPoints) => {
            for (let tp of touchPoints) {
                InputService.sendTouch(tp.x / width, tp.y / height, 0) // ACTION_UP
            }
        }
    }
}
```

### Step 5: Create InputService

```cpp
class InputService : public QObject,
                     public aasdk::channel::input::IInputServiceChannelEventHandler {
    Q_OBJECT

public:
    Q_INVOKABLE void sendTouch(float x, float y, int action);
    Q_INVOKABLE void sendKey(int keyCode, bool isDown);

    // Scale touch coordinates from QML (0-1 normalized) to video resolution
private:
    void sendTouchEvent(uint32_t x, uint32_t y, aasdk::proto::enums::TouchAction action);
    int videoWidth_ = 800;
    int videoHeight_ = 480;
};
```

### Step 6: Wire into AndroidAutoMenu.qml

When AA projection is active, show ProjectionSurface fullscreen:

```qml
Item {
    id: androidAutoScreen

    // Show when not connected
    Column {
        visible: !AndroidAutoService.isProjecting
        anchors.centerIn: parent
        // "Waiting for connection" + connect button
    }

    // Show when projecting
    ProjectionSurface {
        visible: AndroidAutoService.isProjecting
        anchors.fill: parent
    }
}
```

### Step 7: Video decode strategy

**Primary approach (GStreamer):**
Create a C++ `VideoDecoder` class that:
1. Creates GStreamer pipeline: `appsrc ! h264parse ! v4l2h264dec ! videoconvert ! appsink`
   (Fall back to `avdec_h264` on desktop where v4l2 isn't available)
2. Pushes H.264 NAL units into `appsrc`
3. Pulls decoded frames from `appsink`
4. Converts to `QVideoFrame` and pushes to `QVideoSink`

**Fallback approach (QMediaPlayer):**
If GStreamer integration is too complex for Phase 1, use `QProcess` to pipe H.264 through `ffplay` or similar. This is a hack but gets video on screen quickly.

### Step 8: Test with phone

1. Build and deploy to Pi
2. Connect phone via USB
3. Expected: AA UI appears on screen, touch works
4. Log any frame drops, latency, decode errors

### Step 9: Commit

```bash
git add src/core/aa/VideoService.* src/core/aa/InputService.* \
        qml/components/ProjectionSurface.qml
git commit -m "feat: video projection with H.264 decode and touch input"
```

---

## Task 7: Audio Pipeline

**Goal:** Route three AA audio streams (media, speech, system) through PipeWire. Volume control in bottom bar.

**Files:**
- Create: `src/core/audio/AudioOutput.hpp`
- Create: `src/core/audio/AudioOutput.cpp`
- Create: `src/core/aa/MediaAudioService.hpp`
- Create: `src/core/aa/MediaAudioService.cpp`
- Create: `src/core/aa/SpeechAudioService.hpp`
- Create: `src/core/aa/SpeechAudioService.cpp`
- Create: `src/core/aa/SystemAudioService.hpp`
- Create: `src/core/aa/SystemAudioService.cpp`
- Create: `src/core/aa/AudioInputService.hpp` (microphone for voice commands)
- Modify: `qml/components/BottomBar.qml` — wire up volume slider

### Step 1: Create AudioOutput class

Wraps Qt 6's `QAudioSink` (which routes through PipeWire on Trixie):

```cpp
// src/core/audio/AudioOutput.hpp
class AudioOutput : public QObject {
    Q_OBJECT

public:
    AudioOutput(int sampleRate, int channelCount, int sampleSize,
                QObject *parent = nullptr);

    void start();
    void stop();
    void write(const QByteArray& pcmData);

private:
    std::unique_ptr<QAudioSink> audioSink_;
    QIODevice* audioDevice_ = nullptr;  // opened by audioSink_->start()
    QAudioFormat format_;
};
```

### Step 2: Create audio services

Each follows the same pattern — implement the aasdk event handler, forward PCM data to AudioOutput:

```cpp
// src/core/aa/MediaAudioService.hpp
class MediaAudioService : public aasdk::channel::av::IAudioServiceChannelEventHandler {
public:
    MediaAudioService(boost::asio::io_service::strand& strand,
                      std::shared_ptr<aasdk::channel::av::MediaAudioServiceChannel> channel);

    void start();
    void stop();

    // Event handlers
    void onAVMediaWithTimestampIndication(
        aasdk::messenger::Timestamp::ValueType timestamp,
        const aasdk::common::DataConstBuffer& buffer) override;
    // ... etc

private:
    std::unique_ptr<AudioOutput> audioOutput_;  // 48kHz, stereo, 16-bit
};
```

**Audio specs per channel:**
| Channel | Sample Rate | Channels | Bits |
|---------|-------------|----------|------|
| Media | 48000 Hz | 2 (stereo) | 16 |
| Speech | 16000 Hz | 1 (mono) | 16 |
| System | 16000 Hz | 1 (mono) | 16 |

### Step 3: Implement BottomBar volume control

```qml
// qml/components/BottomBar.qml
Rectangle {
    color: ThemeController.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8

        Icon {
            source: volumeSlider.value === 0 ? "qrc:/icons/volume_mute.svg"
                                              : "qrc:/icons/volume.svg"
            Layout.preferredWidth: parent.height * 0.6
            Layout.preferredHeight: parent.height * 0.6
        }

        Slider {
            id: volumeSlider
            Layout.fillWidth: true
            from: 0; to: 100
            value: AudioManager.volume
            onMoved: AudioManager.setVolume(value)
        }
    }
}
```

### Step 4: AudioInputService for microphone

Required for voice commands ("OK Google"):

```cpp
class AudioInputService : public QObject {
    Q_OBJECT
public:
    void start();   // opens microphone
    void stop();
    // Reads PCM from mic, sends to phone via aasdk AV input channel
private:
    std::unique_ptr<QAudioSource> audioSource_;
};
```

### Step 5: Test on Pi

1. Connect phone, start AA
2. Play music on phone → should hear through Pi's audio output
3. Navigation voice → should play through speech channel
4. Volume slider should control output level
5. "OK Google" → microphone should work

### Step 6: Commit

```bash
git add src/core/audio/ src/core/aa/*Audio* src/core/aa/AudioInputService.*
git commit -m "feat: audio pipeline with 3 AA channels and PipeWire output"
```

---

## Task 8: Wireless Android Auto

**Goal:** Connect to phone over WiFi (TCP) in addition to USB. Includes WiFi service channel for credential negotiation and a connection UI.

**Files:**
- Create: `src/core/aa/WifiService.hpp`
- Create: `src/core/aa/WifiService.cpp`
- Create: `qml/applications/android_auto/AndroidAutoConnectMenu.qml`
- Create: `qml/applications/android_auto/AndroidAutoEnterAddressMenu.qml`
- Create: `qml/controls/CustomKeyboard.qml`
- Modify: `src/core/aa/AndroidAutoService.cpp` — add TCP connection path

### Step 1: Add TCP server to AndroidAutoService

Listen on port 5000 for incoming wireless connections (phone connects to us):

```cpp
void AndroidAutoService::startTcpServer()
{
    tcpAcceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
        ioService_,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 5000));

    acceptConnection();
}

void AndroidAutoService::acceptConnection()
{
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioService_);
    tcpAcceptor_->async_accept(*socket, [this, socket](const auto& error) {
        if (!error) {
            auto transport = std::make_shared<aasdk::transport::TCPTransport>(
                ioService_, std::move(*socket));
            onTransportReady(transport);
        }
        acceptConnection();  // keep listening
    });
}
```

### Step 2: Add outgoing TCP connection (manual IP entry)

For connecting to a phone that's hosting its own AA wireless:

```cpp
void AndroidAutoService::connectWireless(const QString& ipAddress)
{
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(ioService_);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(ipAddress.toStdString()), 5277);

    socket->async_connect(endpoint, [this, socket](const auto& error) {
        if (!error) {
            auto transport = std::make_shared<aasdk::transport::TCPTransport>(
                ioService_, std::move(*socket));
            onTransportReady(transport);
        } else {
            QMetaObject::invokeMethod(this, [this, error]() {
                emit connectionFailed(QString::fromStdString(error.message()));
            }, Qt::QueuedConnection);
        }
    });
}
```

### Step 3: Implement WifiService

Handles WiFi credential exchange with the phone:

```cpp
class WifiService : public aasdk::channel::wifi::IWIFIServiceChannelEventHandler {
public:
    void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) override;
    void onWifiSecurityRequest(const proto::messages::WifiSecurityRequest& request) override;
    // Respond with our AP's SSID, security mode, passphrase
};
```

### Step 4: Create connection UI

`AndroidAutoConnectMenu.qml` — shown when user taps "Android Auto" tile:
- "Waiting for USB connection..." status
- "Connect Wirelessly" button → navigates to IP entry screen
- Connection status indicators

`AndroidAutoEnterAddressMenu.qml`:
- IP address text field
- On-screen numeric keyboard with '.' delimiter
- "Connect" button
- "Connecting..." spinner on attempt

### Step 5: CustomKeyboard.qml

Numeric keyboard optimized for IP entry:

```qml
GridView {
    model: ["1","2","3","4","5","6","7","8","9",".","0","←"]
    delegate: CustomButton {
        text: modelData
        onClicked: {
            if (modelData === "←") textField.remove(textField.cursorPosition - 1, textField.cursorPosition)
            else textField.insert(textField.cursorPosition, modelData)
        }
    }
}
```

### Step 6: Test wireless connection

1. Phone and Pi on same WiFi network
2. Enter phone IP in UI
3. Should establish TCP connection and start AA projection
4. Alternatively: set up Pi as WiFi hotspot, phone auto-connects

### Step 7: Commit

```bash
git add src/core/aa/WifiService.* qml/applications/android_auto/ qml/controls/CustomKeyboard.qml
git commit -m "feat: wireless Android Auto via TCP with connection UI"
```

---

## Task 9: Settings UI

**Goal:** Settings screens for Phase 1 configuration: AA video/audio settings, display settings, day/night mode, Bluetooth adapter type, system settings (language, time format).

**Files:**
- Create: `qml/applications/settings/SettingsMenu.qml` (replace placeholder)
- Create: `qml/applications/settings/AndroidAutoVideoSettingsMenu.qml`
- Create: `qml/applications/settings/AndroidAutoAudioSettingsMenu.qml`
- Create: `qml/applications/settings/AndroidAutoBluetoothSettingsMenu.qml`
- Create: `qml/applications/settings/AndroidAutoSystemSettingsMenu.qml`
- Create: `qml/applications/settings/DisplaySettingsMenu.qml`
- Create: `qml/applications/settings/DayNightSettingsMenu.qml`
- Create: `qml/applications/settings/ColorSettingsMenu.qml`
- Create: `qml/applications/settings/SystemSettingsMenu.qml`
- Create: `qml/controls/CustomSwitch.qml`
- Create: `qml/controls/LabeledSlider.qml`
- Create: `qml/controls/RadioButtonsGroup.qml`
- Create: `src/ui/ConfigurationController.hpp`
- Create: `src/ui/ConfigurationController.cpp`

### Step 1: Create ConfigurationController

QML-facing wrapper around Configuration that exposes properties with change notifications:

```cpp
class ConfigurationController : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool showClockInAndroidAuto READ showClockInAndroidAuto
               WRITE setShowClockInAndroidAuto NOTIFY showClockInAndroidAutoChanged)
    Q_PROPERTY(int screenDpi READ screenDpi WRITE setScreenDpi NOTIFY screenDpiChanged)
    Q_PROPERTY(int volumeStep READ volumeStep WRITE setVolumeStep NOTIFY volumeStepChanged)
    // ... all Phase 1 settings

public:
    Q_INVOKABLE void save();  // Writes to INI file

private:
    std::shared_ptr<Configuration> config_;
};
```

### Step 2: Build SettingsMenu.qml (main grid)

```qml
Item {
    Component.onCompleted: ApplicationController.setTitle("Settings")

    GridView {
        anchors.fill: parent
        anchors.margins: parent.width * 0.04
        cellWidth: width / 4
        cellHeight: height / 2

        model: ListModel {
            ListElement { name: "Android Auto"; icon: "android_auto_settings" }
            ListElement { name: "Display"; icon: "display" }
            ListElement { name: "Day/Night"; icon: "day_night" }
            ListElement { name: "Colors"; icon: "colors" }
            ListElement { name: "System"; icon: "system" }
            ListElement { name: "About"; icon: "info" }
        }

        delegate: Tile {
            // Navigate to sub-menu on click
        }
    }
}
```

### Step 3: Build AA video settings

Resolution, FPS, margins, DPI — using LabeledSlider and RadioButtonsGroup:

```qml
// AndroidAutoVideoSettingsMenu.qml
Column {
    spacing: 12

    RadioButtonsGroup {
        title: "Resolution"
        model: ["480p", "720p", "1080p"]
        currentIndex: ConfigurationController.videoResolution
        onSelected: (idx) => ConfigurationController.videoResolution = idx
    }

    RadioButtonsGroup {
        title: "Frame Rate"
        model: ["30 FPS", "60 FPS"]
        currentIndex: ConfigurationController.videoFps === 60 ? 1 : 0
        onSelected: (idx) => ConfigurationController.videoFps = idx === 1 ? 60 : 30
    }

    LabeledSlider {
        title: "Screen DPI"
        from: 80; to: 500; stepSize: 10
        value: ConfigurationController.screenDpi
        onMoved: ConfigurationController.screenDpi = value
    }
}
```

### Step 4: Build day/night settings

```qml
// DayNightSettingsMenu.qml
Column {
    RadioButtonsGroup {
        title: "Day/Night Mode"
        model: ["Manual", "Light Sensor", "Clock", "GPIO"]
        currentIndex: ConfigurationController.dayNightController
        onSelected: (idx) => ConfigurationController.dayNightController = idx
    }

    CustomButton {
        text: "Toggle Day/Night Now"
        onClicked: ThemeController.toggleMode()
    }
}
```

### Step 5: Build reusable settings controls

- `CustomSwitch.qml` — Toggle with label, themed colors
- `LabeledSlider.qml` — Slider with title text, min/max labels, value display
- `RadioButtonsGroup.qml` — Vertical list of radio buttons with title

### Step 6: Auto-save on exit

When user navigates away from settings, call `ConfigurationController.save()`:

```qml
Component.onDestruction: ConfigurationController.save()
```

### Step 7: Verify on desktop

All settings screens should be navigable and functional without a phone connected. Values should persist across app restarts.

### Step 8: Commit

```bash
git add qml/applications/settings/ qml/controls/ \
        src/ui/ConfigurationController.*
git commit -m "feat: settings UI for AA, display, day/night, and system config"
```

---

## Task 10: Integration Testing & Polish

**Goal:** End-to-end testing on Pi 4 hardware. Fix issues found during real-world testing. Polish for first usable release.

**Files:**
- Create: `systemd/openauto-prodigy.service`
- Create: `cmake/deploy.cmake` (Pi deployment helper)
- Create: `docs/BUILDING.md`
- Modify: various files based on testing results

### Step 1: Create systemd service

```ini
[Unit]
Description=OpenAuto Prodigy
After=graphical.target
Wants=graphical.target

[Service]
Type=simple
User=pi
Environment=DISPLAY=:0
Environment=QT_QPA_PLATFORM=eglfs
ExecStart=/usr/local/bin/openauto-prodigy
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical.target
```

### Step 2: Create deployment script

```bash
#!/bin/bash
# deploy.sh — build and deploy to Pi
PI_HOST="${1:-raspberrypi.local}"

echo "Building..."
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

echo "Deploying to ${PI_HOST}..."
rsync -avz --progress openauto-prodigy ${PI_HOST}:/usr/local/bin/
rsync -avz --progress ../config/ ${PI_HOST}:~/.openauto/
rsync -avz --progress ../systemd/ ${PI_HOST}:/etc/systemd/system/

echo "Restarting service..."
ssh ${PI_HOST} "sudo systemctl daemon-reload && sudo systemctl restart openauto-prodigy"
```

### Step 3: Test matrix

| Test | How | Expected |
|------|-----|----------|
| Boot to UI | Power on Pi, service starts | Launcher visible on DFRobot display |
| Touch works | Tap tiles | Navigation between screens |
| Wired AA | Plug in Android phone via USB | AA projection displays, touch works |
| Wireless AA | Enter phone IP in connect menu | AA projection over WiFi |
| Audio playback | Play music on phone | Sound from Pi audio output |
| Voice commands | Say "OK Google" | Microphone input sent to phone |
| Day/Night toggle | Use settings or connected menu | All colors change, AA notification sent |
| Settings persist | Change settings, restart app | Values retained |
| Disconnect/reconnect | Unplug USB, plug back in | App recovers, AA restarts |
| App switch | Navigate away from AA and back | Projection resumes |

### Step 4: Write BUILDING.md

Document:
- Dependencies (`apt install` list for Trixie)
- Build instructions (native on Pi and cross-compile from x86)
- Configuration
- Running

### Step 5: Fix issues from testing

This step is inherently iterative. Common issues to expect:
- Video decode pipeline tuning (latency, frame drops)
- Touch coordinate mapping (aspect ratio, margins)
- Audio buffer sizing (underruns → crackling)
- USB reconnection handling (device hotplug)
- Display scaling on DFRobot 1024x600

### Step 6: Tag v0.1.0

```bash
git tag -a v0.1.0 -m "Phase 1: Skeleton + Android Auto (wired + wireless)"
git push origin main --tags
```

---

## Dependency Installation (Reference)

### Pi (RPi OS Trixie)

```bash
sudo apt install -y \
    build-essential cmake pkg-config \
    qt6-base-dev qt6-declarative-dev qt6-multimedia-dev \
    qml6-module-qtquick qml6-module-qtmultimedia \
    libboost-dev libboost-system-dev libboost-log-dev \
    libprotobuf-dev protobuf-compiler \
    libssl-dev \
    libusb-1.0-0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

### WSL2 (Ubuntu, for desktop development)

Same packages but x86_64 versions. Add Qt 6 PPA if Ubuntu version doesn't ship 6.8.

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| aasdk CMake doesn't integrate cleanly | Build blocked | Build aasdk separately, link as external |
| aasdk is C++14, may have deprecation warnings with C++17 | Build warnings | Suppress with `-Wno-deprecated` |
| Qt 6 video pipeline different enough from Qt 5 to cause issues | Video broken | Fall back to direct GStreamer pipeline |
| H.264 hardware decode unavailable on Pi 4 with V4L2 | High CPU, frame drops | Use GStreamer `v4l2h264dec` or fall back to `avdec_h264` |
| Wireless AA requires WiFi Direct (not just TCP) | Wireless broken | May need wpa_supplicant P2P setup |
| PipeWire not default on minimal Trixie install | No audio | Install PipeWire, or fall back to ALSA via Qt |
| Touch coordinate mapping wrong for DFRobot display | Input broken | Log coordinates, add calibration settings |
| ASIO + Qt event loop deadlock | App freezes | Strict strand discipline, no blocking cross-thread calls |

---

## Task Dependency Graph

```
Task 1 (Repo/Build) ─────────────────────────────────────────┐
    │                                                          │
    ├── Task 2 (Config) ──────────────────────────────────┐    │
    │       │                                              │    │
    │       ├── Task 3 (Theme) ───┐                        │    │
    │       │                      │                        │    │
    │       └──────────────────────┼── Task 4 (QML Shell) ─┤    │
    │                              │         │              │    │
    │                              │         │              │    │
    ├── Task 5 (AA Service) ──────────────────┤              │    │
    │       │                                 │              │    │
    │       ├── Task 6 (Video) ───────────────┤              │    │
    │       │                                 │              │    │
    │       ├── Task 7 (Audio) ───────────────┤              │    │
    │       │                                 │              │    │
    │       └── Task 8 (Wireless) ────────────┤              │    │
    │                                         │              │    │
    │                                         │              │    │
    └─────────────────────────── Task 9 (Settings UI) ──────┘    │
                                              │                   │
                                              └── Task 10 (Integration)
```

**Parallel work possible:**
- Tasks 2+3 can run in parallel with Task 5 (config system vs AA protocol are independent until integration)
- Task 4 depends on Tasks 2+3 (needs theme and config)
- Tasks 6, 7, 8 all depend on Task 5 but are independent of each other
- Task 9 depends on Task 2 (config) and Task 4 (QML shell)
- Task 10 depends on everything

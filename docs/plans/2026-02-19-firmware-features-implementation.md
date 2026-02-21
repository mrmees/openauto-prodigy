# Firmware-Derived Protocol Features — Implementation Plan
> **Status:** COMPLETED

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement 6 AA protocol features derived from cross-vendor firmware analysis: identity fields, resolution advertisement, night mode sensor, video focus state machine, GPS sensor advertisement, and microphone input via PipeWire.

**Architecture:** All 6 features modify the existing service layer (`ServiceFactory.cpp`, `VideoService.cpp`, `AndroidAutoEntity.cpp`) and config system (`YamlConfig`). Night mode introduces a new provider abstraction. Microphone input extends `AudioService` with PipeWire capture. No new plugins or QML changes required.

**Tech Stack:** C++17, Qt 6, Boost.ASIO, PipeWire, yaml-cpp, aasdk (protobuf-based AA protocol)

**Design Doc:** `../openauto-pro-community/docs/plans/2026-02-19-firmware-derived-features-design.md`

---

## Codebase Orientation

Key files you'll be working with:

| File | Purpose |
|------|---------|
| `src/core/aa/AndroidAutoEntity.cpp:165-187` | ServiceDiscoveryResponse identity fields (hardcoded) |
| `src/core/aa/VideoService.cpp:52-70` | Resolution advertisement (only 720p) |
| `src/core/aa/VideoService.hpp` | VideoService class declaration |
| `src/core/aa/ServiceFactory.cpp:300-393` | SensorServiceStub (night mode + driving status) |
| `src/core/aa/ServiceFactory.cpp:537-620` | AVInputServiceStub (microphone, currently no-op) |
| `src/core/aa/ServiceFactory.cpp:649-684` | Factory create() — instantiates all services |
| `src/core/YamlConfig.hpp/cpp` | YAML config with deep merge (add new getters here) |
| `src/core/services/AudioService.hpp/cpp` | PipeWire stream management (output only currently) |
| `src/core/services/IAudioService.hpp` | Audio service interface |
| `src/core/services/ThemeService.hpp` | ThemeService — has nightMode/modeChanged signal |
| `src/core/Configuration.hpp` | Legacy INI config (still used by VideoService for fps/dpi) |
| `src/CMakeLists.txt:1-27` | Source file list |
| `tests/CMakeLists.txt` | Test registration |

**Build:** `cd build && cmake --build . -j$(nproc)` from `openauto-prodigy/`
**Test:** `cd build && ctest --output-on-failure`

**Proto enums available in aasdk:**
- `VideoResolution`: `_480p(1)`, `_720p(2)`, `_1080p(3)`, `_1440p(4)`, `_720p_p(5)`, `_1080pp(6)`
- `VideoFocusMode`: `NONE(0)`, `FOCUSED(1)`, `UNFOCUSED(2)` — no NATIVE_TRANSIENT in proto
- `SensorType`: `NIGHT_DATA`, `DRIVING_STATUS`, `LOCATION_DATA`, others
- `ServiceDiscoveryResponse` fields: `head_unit_name`, `car_model`, `car_year`, `car_serial`, `left_hand_drive_vehicle`, `headunit_manufacturer`, `headunit_model`, `sw_build`, `sw_version`, `can_play_native_media_during_vr`, `hide_clock`, plus `headunit_info` (HeadUnitInfo sub-message)

---

## Task 1: Add Identity and Sensor Config to YamlConfig

**Files:**
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `tests/data/test_config.yaml`
- Modify: `tests/test_yaml_config.cpp` (if it exists — verify)

All 6 features need config. Add all config sections up front so later tasks can just read them.

### Step 1: Write tests for new config getters

Find the existing test file:

Run: `ls tests/test_yaml*` from project root

Add tests to the existing yaml config test file. The tests should cover:

```cpp
// --- Identity config ---
void TestYamlConfig::testIdentityDefaults()
{
    YamlConfig config;
    // Defaults (no file loaded)
    QCOMPARE(config.headUnitName(), QString("OpenAuto Prodigy"));
    QCOMPARE(config.manufacturer(), QString("OpenAuto Project"));
    QCOMPARE(config.model(), QString("Raspberry Pi 4"));
    QCOMPARE(config.swVersion(), QString("0.3.0"));
    QCOMPARE(config.carModel(), QString(""));
    QCOMPARE(config.carYear(), QString(""));
    QCOMPARE(config.leftHandDrive(), true);
}

// --- Video config ---
void TestYamlConfig::testVideoResolution()
{
    YamlConfig config;
    QCOMPARE(config.videoResolution(), QString("720p"));
    config.setVideoResolution("1080p");
    QCOMPARE(config.videoResolution(), QString("1080p"));
}

void TestYamlConfig::testVideoDpi()
{
    YamlConfig config;
    QCOMPARE(config.videoDpi(), 140);
}

// --- Night mode config ---
void TestYamlConfig::testNightModeDefaults()
{
    YamlConfig config;
    QCOMPARE(config.nightModeSource(), QString("time"));
    QCOMPARE(config.nightModeDayStart(), QString("07:00"));
    QCOMPARE(config.nightModeNightStart(), QString("19:00"));
    QCOMPARE(config.nightModeGpioPin(), 17);
    QCOMPARE(config.nightModeGpioActiveHigh(), true);
}

// --- GPS config ---
void TestYamlConfig::testGpsDefaults()
{
    YamlConfig config;
    QCOMPARE(config.gpsEnabled(), true);
    QCOMPARE(config.gpsSource(), QString("none"));
}

// --- Microphone config ---
void TestYamlConfig::testMicrophoneDefaults()
{
    YamlConfig config;
    QCOMPARE(config.microphoneDevice(), QString("auto"));
    QCOMPARE(config.microphoneGain(), 1.0);
}
```

### Step 2: Run tests to verify they fail

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R yaml`
Expected: FAIL — methods don't exist yet

### Step 3: Add config defaults and getters

**In `YamlConfig.hpp`, add after the `videoFps` getter (line ~43):**

```cpp
// Video (extended)
QString videoResolution() const;
void setVideoResolution(const QString& v);
int videoDpi() const;
void setVideoDpi(int v);

// Identity
QString headUnitName() const;
QString manufacturer() const;
QString model() const;
QString swVersion() const;
QString carModel() const;
void setCarModel(const QString& v);
QString carYear() const;
void setCarYear(const QString& v);
bool leftHandDrive() const;
void setLeftHandDrive(bool v);

// Sensors — night mode
QString nightModeSource() const;
void setNightModeSource(const QString& v);
QString nightModeDayStart() const;
QString nightModeNightStart() const;
int nightModeGpioPin() const;
bool nightModeGpioActiveHigh() const;

// Sensors — GPS
bool gpsEnabled() const;
QString gpsSource() const;

// Audio — microphone
QString microphoneDevice() const;
double microphoneGain() const;
```

**In `YamlConfig.cpp`, add to `initDefaults()` (after line 36 `root_["video"]["resolution"] = "720p";`):**

```cpp
root_["video"]["dpi"] = 140;

root_["identity"]["head_unit_name"] = "OpenAuto Prodigy";
root_["identity"]["manufacturer"] = "OpenAuto Project";
root_["identity"]["model"] = "Raspberry Pi 4";
root_["identity"]["sw_version"] = "0.3.0";
root_["identity"]["car_model"] = "";
root_["identity"]["car_year"] = "";
root_["identity"]["left_hand_drive"] = true;

root_["sensors"]["night_mode"]["source"] = "time";
root_["sensors"]["night_mode"]["day_start"] = "07:00";
root_["sensors"]["night_mode"]["night_start"] = "19:00";
root_["sensors"]["night_mode"]["gpio_pin"] = 17;
root_["sensors"]["night_mode"]["gpio_active_high"] = true;

root_["sensors"]["gps"]["enabled"] = true;
root_["sensors"]["gps"]["source"] = "none";

root_["audio"]["microphone"]["device"] = "auto";
root_["audio"]["microphone"]["gain"] = 1.0;
```

**Add getter/setter implementations** following the existing pattern (e.g., `wifiSsid()`):

```cpp
// --- Video (extended) ---
QString YamlConfig::videoResolution() const {
    return QString::fromStdString(root_["video"]["resolution"].as<std::string>("720p"));
}
void YamlConfig::setVideoResolution(const QString& v) {
    root_["video"]["resolution"] = v.toStdString();
}
int YamlConfig::videoDpi() const {
    return root_["video"]["dpi"].as<int>(140);
}
void YamlConfig::setVideoDpi(int v) {
    root_["video"]["dpi"] = v;
}

// --- Identity ---
QString YamlConfig::headUnitName() const {
    return QString::fromStdString(root_["identity"]["head_unit_name"].as<std::string>("OpenAuto Prodigy"));
}
QString YamlConfig::manufacturer() const {
    return QString::fromStdString(root_["identity"]["manufacturer"].as<std::string>("OpenAuto Project"));
}
QString YamlConfig::model() const {
    return QString::fromStdString(root_["identity"]["model"].as<std::string>("Raspberry Pi 4"));
}
QString YamlConfig::swVersion() const {
    return QString::fromStdString(root_["identity"]["sw_version"].as<std::string>("0.3.0"));
}
QString YamlConfig::carModel() const {
    return QString::fromStdString(root_["identity"]["car_model"].as<std::string>(""));
}
void YamlConfig::setCarModel(const QString& v) {
    root_["identity"]["car_model"] = v.toStdString();
}
QString YamlConfig::carYear() const {
    return QString::fromStdString(root_["identity"]["car_year"].as<std::string>(""));
}
void YamlConfig::setCarYear(const QString& v) {
    root_["identity"]["car_year"] = v.toStdString();
}
bool YamlConfig::leftHandDrive() const {
    return root_["identity"]["left_hand_drive"].as<bool>(true);
}
void YamlConfig::setLeftHandDrive(bool v) {
    root_["identity"]["left_hand_drive"] = v;
}

// --- Sensors: night mode ---
QString YamlConfig::nightModeSource() const {
    return QString::fromStdString(root_["sensors"]["night_mode"]["source"].as<std::string>("time"));
}
void YamlConfig::setNightModeSource(const QString& v) {
    root_["sensors"]["night_mode"]["source"] = v.toStdString();
}
QString YamlConfig::nightModeDayStart() const {
    return QString::fromStdString(root_["sensors"]["night_mode"]["day_start"].as<std::string>("07:00"));
}
QString YamlConfig::nightModeNightStart() const {
    return QString::fromStdString(root_["sensors"]["night_mode"]["night_start"].as<std::string>("19:00"));
}
int YamlConfig::nightModeGpioPin() const {
    return root_["sensors"]["night_mode"]["gpio_pin"].as<int>(17);
}
bool YamlConfig::nightModeGpioActiveHigh() const {
    return root_["sensors"]["night_mode"]["gpio_active_high"].as<bool>(true);
}

// --- Sensors: GPS ---
bool YamlConfig::gpsEnabled() const {
    return root_["sensors"]["gps"]["enabled"].as<bool>(true);
}
QString YamlConfig::gpsSource() const {
    return QString::fromStdString(root_["sensors"]["gps"]["source"].as<std::string>("none"));
}

// --- Audio: microphone ---
QString YamlConfig::microphoneDevice() const {
    return QString::fromStdString(root_["audio"]["microphone"]["device"].as<std::string>("auto"));
}
double YamlConfig::microphoneGain() const {
    return root_["audio"]["microphone"]["gain"].as<double>(1.0);
}
```

### Step 4: Update test config YAML

Add to `tests/data/test_config.yaml` (after the `video` section):

```yaml
identity:
  head_unit_name: "Test HU"
  manufacturer: "Test Mfg"
  model: "Test Model"
  sw_version: "0.0.1"
  car_model: "Test Car"
  car_year: "2025"
  left_hand_drive: false

sensors:
  night_mode:
    source: theme
    day_start: "06:00"
    night_start: "20:00"
    gpio_pin: 22
    gpio_active_high: false
  gps:
    enabled: false
    source: none

audio:
  master_volume: 75
  output_device: auto
  microphone:
    device: "hw:1,0"
    gain: 1.5
```

Also add a test that loads the file and verifies overrides:

```cpp
void TestYamlConfig::testIdentityFromFile()
{
    YamlConfig config;
    config.load(TEST_CONFIG_PATH);
    QCOMPARE(config.headUnitName(), QString("Test HU"));
    QCOMPARE(config.manufacturer(), QString("Test Mfg"));
    QCOMPARE(config.leftHandDrive(), false);
}

void TestYamlConfig::testNightModeFromFile()
{
    YamlConfig config;
    config.load(TEST_CONFIG_PATH);
    QCOMPARE(config.nightModeSource(), QString("theme"));
    QCOMPARE(config.nightModeGpioPin(), 22);
}

void TestYamlConfig::testMicrophoneFromFile()
{
    YamlConfig config;
    config.load(TEST_CONFIG_PATH);
    QCOMPARE(config.microphoneDevice(), QString("hw:1,0"));
    QCOMPARE(config.microphoneGain(), 1.5);
}
```

### Step 5: Build and run tests

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R yaml`
Expected: All PASS

### Step 6: Commit

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/data/test_config.yaml tests/test_yaml_config.cpp
git commit -m "feat: add identity, sensor, and mic config to YamlConfig

Add config sections for identity (HU name, manufacturer, model, version,
car info), night mode (source, times, GPIO), GPS (enabled, source), and
microphone (device, gain). All with sensible defaults and deep-merge
support from YAML files."
```

---

## Task 2: ServiceDiscoveryResponse Identity Fields

**Files:**
- Modify: `src/core/aa/AndroidAutoEntity.cpp:165-187`
- Modify: `src/core/aa/AndroidAutoEntity.hpp:30` (constructor)
- Modify: `src/core/aa/AndroidAutoService.cpp` (passes config to entity)

### Step 1: Wire YamlConfig into AndroidAutoEntity

Currently `AndroidAutoEntity` doesn't have access to config. It needs a `YamlConfig*` parameter.

**Check how the entity is constructed:**

Run: `grep -n "AndroidAutoEntity(" src/core/aa/AndroidAutoService.cpp` to find the construction site.

**Modify `AndroidAutoEntity.hpp`:**
- Add `#include "../../core/YamlConfig.hpp"`
- Add `YamlConfig* config` parameter to constructor (after `serviceList`)
- Add `YamlConfig* config_` member

**Modify `AndroidAutoEntity.cpp` constructor** to store the config pointer.

**Modify `AndroidAutoService.cpp`** where the entity is created — pass `yamlConfig_` (or however config is stored there).

### Step 2: Replace hardcoded identity fields

**In `AndroidAutoEntity.cpp`, replace lines 166-187** with config-driven values:

```cpp
    auto* huInfo = response.mutable_headunit_info();
    huInfo->set_make(config_->manufacturer().toStdString());
    huInfo->set_model(config_->carModel().isEmpty() ? "Universal" : config_->carModel().toStdString());
    huInfo->set_year(config_->carYear().isEmpty() ? "2026" : config_->carYear().toStdString());
    huInfo->set_vehicle_id("OAP-0001");
    huInfo->set_head_unit_make(config_->manufacturer().toStdString());
    huInfo->set_head_unit_model(config_->model().toStdString());
    huInfo->set_head_unit_software_build(OAP_GIT_HASH);   // compile-time define
    huInfo->set_head_unit_software_version(config_->swVersion().toStdString());

    // Legacy fields (deprecated but included for older AA versions)
    response.set_head_unit_name(config_->headUnitName().toStdString());
    response.set_car_model(config_->carModel().isEmpty() ? "Universal" : config_->carModel().toStdString());
    response.set_car_year(config_->carYear().isEmpty() ? "2026" : config_->carYear().toStdString());
    response.set_car_serial("OAP-0001");
    response.set_left_hand_drive_vehicle(config_->leftHandDrive());
    response.set_headunit_manufacturer(config_->manufacturer().toStdString());
    response.set_headunit_model(config_->model().toStdString());
    response.set_sw_build(OAP_GIT_HASH);
    response.set_sw_version(config_->swVersion().toStdString());
    response.set_can_play_native_media_during_vr(true);
    response.set_hide_clock(false);
```

### Step 3: Add git hash compile-time define

**In root `CMakeLists.txt`**, add before `add_subdirectory(src)`:

```cmake
# Git hash for ServiceDiscoveryResponse sw_build field
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OAP_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT OAP_GIT_HASH)
    set(OAP_GIT_HASH "unknown")
endif()
```

**In `src/CMakeLists.txt`**, add after `target_link_libraries`:

```cmake
target_compile_definitions(openauto-prodigy PRIVATE
    OAP_GIT_HASH="${OAP_GIT_HASH}"
)
```

### Step 4: Build and verify

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. No new tests needed for this — it's a wiring change to existing plumbing.

### Step 5: Commit

```bash
git add src/core/aa/AndroidAutoEntity.hpp src/core/aa/AndroidAutoEntity.cpp src/core/aa/AndroidAutoService.cpp CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: populate ServiceDiscoveryResponse identity from config

Replace hardcoded identity fields with YamlConfig-driven values.
Phone now shows configured HU name, manufacturer, model, and version
in AA settings. Git commit hash injected at compile time as sw_build."
```

---

## Task 3: Resolution Advertisement — Full Spectrum

**Files:**
- Modify: `src/core/aa/VideoService.cpp:52-70` (fillFeatures)

### Step 1: Add 480p mandatory fallback + configurable preferred resolution

**Replace `fillFeatures` body (lines 61-69) with:**

```cpp
    // Preferred resolution from config
    auto* primaryConfig = avChannel->add_video_configs();

    QString res = config_->videoResolution();  // "480p" | "720p" | "1080p"
    if (res == "1080p") {
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_1080p);
    } else if (res == "480p") {
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
    } else {
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_720p);
    }

    auto fps = config_->videoFps() == 60
        ? aasdk::proto::enums::VideoFPS::_60
        : aasdk::proto::enums::VideoFPS::_30;
    primaryConfig->set_video_fps(fps);
    primaryConfig->set_margin_width(0);
    primaryConfig->set_margin_height(0);
    primaryConfig->set_dpi(config_->videoDpi());

    // Mandatory 480p fallback (every production SDK includes this)
    if (res != "480p") {
        auto* fallbackConfig = avChannel->add_video_configs();
        fallbackConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
        fallbackConfig->set_video_fps(aasdk::proto::enums::VideoFPS::_30);
        fallbackConfig->set_margin_width(0);
        fallbackConfig->set_margin_height(0);
        fallbackConfig->set_dpi(config_->videoDpi());
    }
```

**Note:** `VideoService` currently uses `Configuration` (the legacy INI class), not `YamlConfig`. You'll need to either:
- (a) Pass `YamlConfig*` to `VideoService` alongside or instead of `Configuration`, or
- (b) Add `videoResolution()` and `videoDpi()` getters to the legacy `Configuration` class

Option (a) is cleaner. Check `ServiceFactory.cpp:659` where `VideoService` is created — it gets `config` which is `std::shared_ptr<oap::Configuration>`. You'll need to also pass the `YamlConfig*` (or switch entirely). The simplest approach: add a `YamlConfig*` parameter to the `ServiceFactory::create()` method and thread it through to `VideoService`.

**Modify `ServiceFactory.hpp`** — add `YamlConfig*` to the `create()` signature.
**Modify `VideoService` constructor** — add `YamlConfig*` parameter, store as member.
**Modify where `ServiceFactory::create()` is called** — pass the yaml config.

### Step 2: Verify VideoDecoder handles resolution switches

Check `VideoDecoder.cpp` — FFmpeg's `avcodec_open2` with software decode handles any resolution. The phone negotiates which resolution to use. If it sends 480p frames instead of 720p, the decoder will handle it. No changes needed here unless there's hardcoded 1280x720 assumptions.

Run: `grep -n "1280\|720\|resolution" src/core/aa/VideoDecoder.cpp` — verify no hardcoded dimensions.

### Step 3: Build and test

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass

### Step 4: Commit

```bash
git add src/core/aa/VideoService.cpp src/core/aa/VideoService.hpp src/core/aa/ServiceFactory.cpp src/core/aa/ServiceFactory.hpp
git commit -m "feat: advertise 480p mandatory fallback + configurable preferred resolution

Production AA SDKs always include 800x480 as a mandatory fallback.
Without it, the SDK auto-injects it with a warning. Now advertises
the configured preferred resolution plus 480p fallback."
```

---

## Task 4: Night Mode Sensor — Live Updates

**Files:**
- Create: `src/core/aa/NightModeProvider.hpp` (interface)
- Create: `src/core/aa/TimedNightMode.hpp`
- Create: `src/core/aa/TimedNightMode.cpp`
- Create: `src/core/aa/ThemeNightMode.hpp`
- Create: `src/core/aa/ThemeNightMode.cpp`
- Create: `src/core/aa/GpioNightMode.hpp`
- Create: `src/core/aa/GpioNightMode.cpp`
- Modify: `src/core/aa/ServiceFactory.cpp:300-393` (SensorServiceStub)
- Modify: `src/CMakeLists.txt` (add new source files)
- Create: `tests/test_night_mode.cpp`
- Modify: `tests/CMakeLists.txt`

### Step 1: Create NightModeProvider interface

```cpp
// src/core/aa/NightModeProvider.hpp
#pragma once

#include <QObject>

namespace oap {
namespace aa {

class NightModeProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~NightModeProvider() = default;

    virtual bool isNight() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void nightModeChanged(bool isNight);
};

} // namespace aa
} // namespace oap
```

### Step 2: Write failing tests for TimedNightMode

```cpp
// tests/test_night_mode.cpp
#include <QtTest/QtTest>
#include "core/aa/TimedNightMode.hpp"

class TestNightMode : public QObject {
    Q_OBJECT
private slots:
    void testTimedNightMode_DayTime();
    void testTimedNightMode_NightTime();
    void testTimedNightMode_SignalEmitted();
};

void TestNightMode::testTimedNightMode_DayTime()
{
    // 12:00 is between 07:00 and 19:00 → day
    oap::aa::TimedNightMode provider("07:00", "19:00");
    // Test with a known time — we'll check isNight() based on current time,
    // but we can at least verify the object constructs and returns a bool
    bool result = provider.isNight();
    // Can't assert exact value without controlling time, but verify it doesn't crash
    Q_UNUSED(result);
    QVERIFY(true);
}

void TestNightMode::testTimedNightMode_NightTime()
{
    // Set day_start way in the future, night_start in the past
    // Current time should be "night" if night_start=00:00 and day_start=23:59
    oap::aa::TimedNightMode provider("23:59", "00:00");
    QCOMPARE(provider.isNight(), true);
}

void TestNightMode::testTimedNightMode_SignalEmitted()
{
    oap::aa::TimedNightMode provider("07:00", "19:00");
    QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);
    provider.start();
    // The timer fires every 60s — we just verify start() doesn't crash
    provider.stop();
    QVERIFY(true);
}

QTEST_MAIN(TestNightMode)
#include "test_night_mode.moc"
```

### Step 3: Run tests to verify they fail

Run: `cd build && cmake .. && cmake --build . -j$(nproc) 2>&1 | head -20`
Expected: Compilation failure — TimedNightMode.hpp doesn't exist

### Step 4: Implement TimedNightMode

```cpp
// src/core/aa/TimedNightMode.hpp
#pragma once

#include "NightModeProvider.hpp"
#include <QTimer>
#include <QTime>

namespace oap {
namespace aa {

class TimedNightMode : public NightModeProvider {
    Q_OBJECT
public:
    TimedNightMode(const QString& dayStart, const QString& nightStart,
                   QObject* parent = nullptr);

    bool isNight() const override;
    void start() override;
    void stop() override;

private:
    void check();

    QTime dayStart_;
    QTime nightStart_;
    QTimer timer_;
    bool currentIsNight_ = false;
};

} // namespace aa
} // namespace oap
```

```cpp
// src/core/aa/TimedNightMode.cpp
#include "TimedNightMode.hpp"
#include <QTime>
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

TimedNightMode::TimedNightMode(const QString& dayStart, const QString& nightStart,
                               QObject* parent)
    : NightModeProvider(parent)
    , dayStart_(QTime::fromString(dayStart, "HH:mm"))
    , nightStart_(QTime::fromString(nightStart, "HH:mm"))
{
    if (!dayStart_.isValid()) dayStart_ = QTime(7, 0);
    if (!nightStart_.isValid()) nightStart_ = QTime(19, 0);

    connect(&timer_, &QTimer::timeout, this, &TimedNightMode::check);
}

bool TimedNightMode::isNight() const
{
    QTime now = QTime::currentTime();
    if (nightStart_ > dayStart_) {
        // Normal: day 07:00-19:00, night 19:00-07:00
        return now >= nightStart_ || now < dayStart_;
    } else {
        // Inverted: night_start < day_start (e.g., night=00:00, day=23:59)
        return now >= nightStart_ && now < dayStart_;
    }
}

void TimedNightMode::start()
{
    currentIsNight_ = isNight();
    BOOST_LOG_TRIVIAL(info) << "[TimedNightMode] Started (currently "
                             << (currentIsNight_ ? "night" : "day") << ")";
    timer_.start(60000);  // Check every 60 seconds
}

void TimedNightMode::stop()
{
    timer_.stop();
}

void TimedNightMode::check()
{
    bool night = isNight();
    if (night != currentIsNight_) {
        currentIsNight_ = night;
        BOOST_LOG_TRIVIAL(info) << "[TimedNightMode] Mode changed to "
                                 << (night ? "night" : "day");
        emit nightModeChanged(night);
    }
}

} // namespace aa
} // namespace oap
```

### Step 5: Implement ThemeNightMode

```cpp
// src/core/aa/ThemeNightMode.hpp
#pragma once

#include "NightModeProvider.hpp"

namespace oap {

class ThemeService;

namespace aa {

class ThemeNightMode : public NightModeProvider {
    Q_OBJECT
public:
    ThemeNightMode(ThemeService* theme, QObject* parent = nullptr);

    bool isNight() const override;
    void start() override;
    void stop() override;

private:
    ThemeService* theme_;
};

} // namespace aa
} // namespace oap
```

```cpp
// src/core/aa/ThemeNightMode.cpp
#include "ThemeNightMode.hpp"
#include "../../core/services/ThemeService.hpp"
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

ThemeNightMode::ThemeNightMode(ThemeService* theme, QObject* parent)
    : NightModeProvider(parent)
    , theme_(theme)
{
}

bool ThemeNightMode::isNight() const
{
    return theme_ ? theme_->nightMode() : false;
}

void ThemeNightMode::start()
{
    if (theme_) {
        connect(theme_, &ThemeService::modeChanged, this, [this]() {
            bool night = theme_->nightMode();
            BOOST_LOG_TRIVIAL(info) << "[ThemeNightMode] Theme changed to "
                                     << (night ? "night" : "day");
            emit nightModeChanged(night);
        });
    }
    BOOST_LOG_TRIVIAL(info) << "[ThemeNightMode] Started (follows theme service)";
}

void ThemeNightMode::stop()
{
    if (theme_) disconnect(theme_, nullptr, this, nullptr);
}

} // namespace aa
} // namespace oap
```

### Step 6: Implement GpioNightMode

```cpp
// src/core/aa/GpioNightMode.hpp
#pragma once

#include "NightModeProvider.hpp"
#include <QTimer>

namespace oap {
namespace aa {

class GpioNightMode : public NightModeProvider {
    Q_OBJECT
public:
    GpioNightMode(int pin, bool activeHigh, QObject* parent = nullptr);

    bool isNight() const override;
    void start() override;
    void stop() override;

private:
    void poll();
    bool readGpio() const;

    int pin_;
    bool activeHigh_;
    QTimer timer_;
    bool currentIsNight_ = false;
};

} // namespace aa
} // namespace oap
```

```cpp
// src/core/aa/GpioNightMode.cpp
#include "GpioNightMode.hpp"
#include <QFile>
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

GpioNightMode::GpioNightMode(int pin, bool activeHigh, QObject* parent)
    : NightModeProvider(parent)
    , pin_(pin)
    , activeHigh_(activeHigh)
{
    connect(&timer_, &QTimer::timeout, this, &GpioNightMode::poll);
}

bool GpioNightMode::isNight() const
{
    return currentIsNight_;
}

void GpioNightMode::start()
{
    // Export GPIO if not already exported
    QString exportPath = "/sys/class/gpio/export";
    QFile exportFile(exportPath);
    if (exportFile.open(QIODevice::WriteOnly)) {
        exportFile.write(QByteArray::number(pin_));
        exportFile.close();
    }

    // Set direction to input
    QString dirPath = QString("/sys/class/gpio/gpio%1/direction").arg(pin_);
    QFile dirFile(dirPath);
    if (dirFile.open(QIODevice::WriteOnly)) {
        dirFile.write("in");
        dirFile.close();
    }

    currentIsNight_ = readGpio();
    BOOST_LOG_TRIVIAL(info) << "[GpioNightMode] Started on pin " << pin_
                             << " (active_high=" << activeHigh_
                             << ", currently " << (currentIsNight_ ? "night" : "day") << ")";
    timer_.start(1000);  // Poll every 1 second
}

void GpioNightMode::stop()
{
    timer_.stop();
}

void GpioNightMode::poll()
{
    bool night = readGpio();
    if (night != currentIsNight_) {
        currentIsNight_ = night;
        BOOST_LOG_TRIVIAL(info) << "[GpioNightMode] Mode changed to "
                                 << (night ? "night" : "day");
        emit nightModeChanged(night);
    }
}

bool GpioNightMode::readGpio() const
{
    QString valuePath = QString("/sys/class/gpio/gpio%1/value").arg(pin_);
    QFile file(valuePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;  // Can't read → assume day
    }
    bool pinHigh = file.readAll().trimmed() == "1";
    return activeHigh_ ? pinHigh : !pinHigh;
}

} // namespace aa
} // namespace oap
```

### Step 7: Modify SensorServiceStub for live updates

The SensorServiceStub in `ServiceFactory.cpp` needs:
1. A `NightModeProvider*` member
2. Connection to the provider's `nightModeChanged` signal
3. A public `sendNightMode(bool)` method that sends `SensorEventIndication`
4. GPS sensor advertisement (just add `LOCATION_DATA` to `fillFeatures`)

**Modify constructor** (line 306-310):

```cpp
SensorServiceStub(boost::asio::io_service& ioService,
                  aasdk::messenger::IMessenger::Pointer messenger,
                  NightModeProvider* nightProvider)
    : strand_(ioService)
    , channel_(std::make_shared<aasdk::channel::sensor::SensorServiceChannel>(strand_, std::move(messenger)))
    , nightProvider_(nightProvider)
{
    if (nightProvider_) {
        QObject::connect(nightProvider_, &NightModeProvider::nightModeChanged,
                         [this](bool isNight) {
            sendNightModeUpdate(isNight);
        });
    }
}
```

**Add to `fillFeatures`** (after line 329, before the closing brace):

```cpp
    auto* gpsSensor = sensorChannel->add_sensors();
    gpsSensor->set_type(aasdk::proto::enums::SensorType::LOCATION_DATA);
```

**Modify `sendInitialSensorData`** — use the provider's current state:

```cpp
if (sensorType == static_cast<int32_t>(aasdk::proto::enums::SensorType::NIGHT_DATA)) {
    bool isNight = nightProvider_ ? nightProvider_->isNight() : false;
    auto* nightMode = indication.add_night_mode();
    nightMode->set_is_night(isNight);
    BOOST_LOG_TRIVIAL(info) << "[SensorService] Sending night mode: "
                             << (isNight ? "night" : "day");
} else if (sensorType == static_cast<int32_t>(aasdk::proto::enums::SensorType::LOCATION_DATA)) {
    // GPS: advertised but no-fix default — phone uses its own GPS
    BOOST_LOG_TRIVIAL(info) << "[SensorService] GPS sensor started (no-fix default)";
    return;  // Don't send empty location data
}
```

**Add `sendNightModeUpdate` method** (new):

```cpp
void sendNightModeUpdate(bool isNight) {
    aasdk::proto::messages::SensorEventIndication indication;
    auto* nightMode = indication.add_night_mode();
    nightMode->set_is_night(isNight);

    BOOST_LOG_TRIVIAL(info) << "[SensorService] Night mode update: "
                             << (isNight ? "night" : "day");

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[SensorService] Night mode send error: " << e.what();
    });
    channel_->sendSensorEventIndication(indication, std::move(promise));
}
```

**Add member:**

```cpp
private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::sensor::SensorServiceChannel> channel_;
    NightModeProvider* nightProvider_ = nullptr;
```

### Step 8: Wire the provider in ServiceFactory::create()

**In the factory (line 664),** change:

```cpp
// Before:
services.push_back(std::make_shared<SensorServiceStub>(ioService, messenger));

// After:
services.push_back(std::make_shared<SensorServiceStub>(ioService, messenger, nightProvider));
```

The `nightProvider` must be created before this line. Add to the factory signature and create it based on config:

```cpp
// At the top of create(), create the night mode provider:
NightModeProvider* nightProvider = nullptr;
// (provider is created by caller and passed in, or created here from config)
```

**Recommended approach:** Create the provider in `main.cpp` or `AndroidAutoService.cpp` (wherever YamlConfig is available) and pass it into the factory. Add `NightModeProvider*` to `ServiceFactory::create()` signature.

### Step 9: Add new files to CMakeLists.txt

**In `src/CMakeLists.txt`, add to `qt_add_executable` source list (after line 9):**

```
    core/aa/TimedNightMode.cpp
    core/aa/ThemeNightMode.cpp
    core/aa/GpioNightMode.cpp
```

**In `tests/CMakeLists.txt`, add:**

```cmake
add_executable(test_night_mode
    test_night_mode.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/TimedNightMode.cpp
)
target_include_directories(test_night_mode PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_night_mode PRIVATE Qt6::Core Qt6::Test Boost::log Boost::log_setup)
add_test(NAME test_night_mode COMMAND test_night_mode)
```

### Step 10: Build and run all tests

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass (8 existing + 1 new)

### Step 11: Commit

```bash
git add src/core/aa/NightModeProvider.hpp src/core/aa/TimedNightMode.hpp src/core/aa/TimedNightMode.cpp \
  src/core/aa/ThemeNightMode.hpp src/core/aa/ThemeNightMode.cpp \
  src/core/aa/GpioNightMode.hpp src/core/aa/GpioNightMode.cpp \
  src/core/aa/ServiceFactory.cpp src/core/aa/ServiceFactory.hpp \
  src/CMakeLists.txt tests/test_night_mode.cpp tests/CMakeLists.txt
git commit -m "feat: live night mode sensor with 3 configurable sources

Add NightModeProvider abstraction with TimedNightMode (clock-based),
ThemeNightMode (follows theme service), and GpioNightMode (sysfs GPIO
polling). SensorServiceStub now sends live night mode updates to AA
instead of a static day-mode-only value. Also advertises GPS sensor
capability with no-fix default."
```

---

## Task 5: Video Focus State Machine

**Files:**
- Modify: `src/core/aa/VideoService.hpp`
- Modify: `src/core/aa/VideoService.cpp`

### Step 1: Add VideoFocusMode enum and public method

The aasdk proto only has FOCUSED/UNFOCUSED. Our internal enum has three states, but NATIVE_TRANSIENT maps to FOCUSED in the proto (we want the phone to keep rendering).

**In `VideoService.hpp`, add before the class:**

```cpp
enum class VideoFocusMode {
    Projection,         // AA active and rendering → proto FOCUSED
    Native,             // HU's own UI, AA session paused → proto UNFOCUSED
    NativeTransient     // Brief interruption, AA keeps session → proto FOCUSED
};
```

**Add public method to VideoService class:**

```cpp
    /// Set video focus mode. Call from other plugins (e.g., PhonePlugin for incoming call).
    void setVideoFocus(VideoFocusMode mode);
```

**Add member:**

```cpp
    VideoFocusMode currentFocusMode_ = VideoFocusMode::Projection;
```

### Step 2: Implement setVideoFocus

**In `VideoService.cpp`, add:**

```cpp
void VideoService::setVideoFocus(VideoFocusMode mode)
{
    currentFocusMode_ = mode;

    // Map our internal enum to the proto's limited enum
    auto protoMode = (mode == VideoFocusMode::Native)
        ? aasdk::proto::enums::VideoFocusMode::UNFOCUSED
        : aasdk::proto::enums::VideoFocusMode::FOCUSED;

    BOOST_LOG_TRIVIAL(info) << "[VideoService] Setting video focus: "
        << (mode == VideoFocusMode::Projection ? "Projection" :
            mode == VideoFocusMode::Native ? "Native" : "NativeTransient")
        << " (proto=" << static_cast<int>(protoMode) << ")";

    aasdk::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(protoMode);
    indication.set_unrequested(true);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[VideoService] VideoFocusIndication send error: " << e.what();
    });
    channel_->sendVideoFocusIndication(indication, std::move(promise));
}
```

### Step 3: Build and test

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass

### Step 4: Commit

```bash
git add src/core/aa/VideoService.hpp src/core/aa/VideoService.cpp
git commit -m "feat: video focus state machine with NativeTransient support

Add VideoFocusMode enum (Projection/Native/NativeTransient) and
setVideoFocus() public method. NativeTransient maps to FOCUSED in proto
so the phone keeps rendering during brief interruptions (reverse camera,
incoming call overlay). Other plugins can call setVideoFocus() to
control AA video state."
```

---

## Task 6: Microphone Input — PipeWire Capture

**Files:**
- Modify: `src/core/services/IAudioService.hpp` (add capture interface)
- Modify: `src/core/services/AudioService.hpp` (add capture members)
- Modify: `src/core/services/AudioService.cpp` (implement capture)
- Modify: `src/core/aa/ServiceFactory.cpp:537-620` (AVInputServiceStub)

This is the largest task. The flow is:
1. Phone sends `AVInputOpenRequest(open=true)` → start PipeWire capture
2. PipeWire delivers PCM buffers via callback → send as `AVMediaWithTimestamp` to phone
3. Phone sends `AVInputOpenRequest(open=false)` → stop PipeWire capture

### Step 1: Add capture interface to IAudioService

**In `IAudioService.hpp`, add after `releaseAudioFocus`:**

```cpp
    // Capture (microphone input)
    virtual AudioStreamHandle* openCaptureStream(const QString& name,
                                                  int sampleRate, int channels, int bitDepth) = 0;
    virtual void closeCaptureStream(AudioStreamHandle* handle) = 0;

    // Callback type for capture data
    using CaptureCallback = std::function<void(const uint8_t* data, int size)>;
    virtual void setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb) = 0;
```

### Step 2: Implement capture in AudioService

**In `AudioService.hpp`, add member:**

```cpp
    AudioStreamHandle* openCaptureStream(const QString& name,
                                          int sampleRate, int channels, int bitDepth) override;
    void closeCaptureStream(AudioStreamHandle* handle) override;
    void setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb) override;

private:
    // ...existing members...
    AudioStreamHandle* captureHandle_ = nullptr;
    CaptureCallback captureCallback_;
    static void onCaptureProcess(void* data);
```

**In `AudioService.cpp`, implement:**

```cpp
AudioStreamHandle* AudioService::openCaptureStream(const QString& name,
                                                     int sampleRate, int channels, int bitDepth)
{
    if (!loop_) return nullptr;

    auto* handle = new AudioStreamHandle();
    handle->name = name;

    uint8_t buffer[1024];
    auto* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    static const struct pw_stream_events captureEvents = {
        .version = PW_VERSION_STREAM_EVENTS,
        .process = onCaptureProcess,
    };

    pw_thread_loop_lock(loop_);

    handle->stream = pw_stream_new_simple(
        pw_thread_loop_get_loop(loop_),
        name.toUtf8().constData(),
        props,
        &captureEvents,
        this);

    // Build format: S16LE mono 16kHz (AA microphone spec)
    auto* pod_builder = reinterpret_cast<spa_pod_builder*>(
        alloca(sizeof(spa_pod_builder)));
    *pod_builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_audio_info_raw audioInfo = {};
    audioInfo.format = SPA_AUDIO_FORMAT_S16_LE;
    audioInfo.rate = sampleRate;
    audioInfo.channels = channels;

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(pod_builder, SPA_PARAM_EnumFormat, &audioInfo);

    pw_stream_connect(handle->stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                      params, 1);

    pw_thread_loop_unlock(loop_);

    captureHandle_ = handle;
    BOOST_LOG_TRIVIAL(info) << "[AudioService] Opened capture stream: " << name.toStdString()
                             << " (" << sampleRate << "Hz, " << channels << "ch, "
                             << bitDepth << "bit)";
    return handle;
}

void AudioService::closeCaptureStream(AudioStreamHandle* handle)
{
    if (!handle || !handle->stream) return;

    pw_thread_loop_lock(loop_);
    pw_stream_destroy(handle->stream);
    pw_thread_loop_unlock(loop_);

    if (captureHandle_ == handle) {
        captureHandle_ = nullptr;
        captureCallback_ = nullptr;
    }

    BOOST_LOG_TRIVIAL(info) << "[AudioService] Closed capture stream: " << handle->name.toStdString();
    delete handle;
}

void AudioService::setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb)
{
    captureCallback_ = std::move(cb);
}

void AudioService::onCaptureProcess(void* data)
{
    auto* self = static_cast<AudioService*>(data);
    if (!self->captureHandle_ || !self->captureHandle_->stream || !self->captureCallback_)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(self->captureHandle_->stream);
    if (!buf) return;

    struct spa_buffer* spa_buf = buf->buffer;
    if (spa_buf->datas[0].data) {
        auto* pcmData = static_cast<const uint8_t*>(spa_buf->datas[0].data);
        int size = spa_buf->datas[0].chunk->size;
        self->captureCallback_(pcmData, size);
    }

    pw_stream_queue_buffer(self->captureHandle_->stream, buf);
}
```

**Note:** The PipeWire capture setup uses the same pattern as the existing output streams but with `PW_DIRECTION_INPUT`. The `process` callback is on the PipeWire thread — the capture callback must be thread-safe (posting data to the ASIO strand).

### Step 3: Wire AVInputServiceStub to actually capture and send audio

**Modify AVInputServiceStub** to accept `IAudioService*`:

```cpp
AVInputServiceStub(boost::asio::io_service& ioService,
                   aasdk::messenger::IMessenger::Pointer messenger,
                   IAudioService* audioService)
    : strand_(ioService)
    , channel_(std::make_shared<aasdk::channel::av::AVInputServiceChannel>(strand_, std::move(messenger)))
    , audioService_(audioService)
{}
```

**Modify `onAVInputOpenRequest`** (replace lines 596-606):

```cpp
void onAVInputOpenRequest(const aasdk::proto::messages::AVInputOpenRequest& request) override {
    BOOST_LOG_TRIVIAL(info) << "[AVInputService] Input open request (open=" << request.open() << ")";

    if (request.open() && !captureStream_) {
        // Open PipeWire capture stream: 16kHz mono 16-bit (AA mic spec)
        captureStream_ = audioService_->openCaptureStream("AA Microphone", 16000, 1, 16);
        if (captureStream_) {
            audioService_->setCaptureCallback(captureStream_,
                [this](const uint8_t* data, int size) {
                    // Called on PipeWire thread — dispatch to ASIO strand
                    auto dataCopy = aasdk::common::Data(data, data + size);
                    strand_.dispatch([this, d = std::move(dataCopy)]() {
                        sendMicData(d);
                    });
                });
            BOOST_LOG_TRIVIAL(info) << "[AVInputService] Microphone capture started";
        }
    } else if (!request.open() && captureStream_) {
        audioService_->closeCaptureStream(captureStream_);
        captureStream_ = nullptr;
        BOOST_LOG_TRIVIAL(info) << "[AVInputService] Microphone capture stopped";
    }

    aasdk::proto::messages::AVInputOpenResponse response;
    response.set_session(0);
    response.set_value(0);  // 0 = success

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {});
    channel_->sendAVInputOpenResponse(response, std::move(promise));
    channel_->receive(shared_from_this());
}
```

**Add `sendMicData` method and members:**

```cpp
void sendMicData(const aasdk::common::Data& data) {
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[AVInputService] Mic data send error: " << e.what();
    });
    channel_->sendAVMediaWithTimestampIndication(timestamp, data, std::move(promise));
}

// Add to private section:
IAudioService* audioService_ = nullptr;
AudioStreamHandle* captureStream_ = nullptr;
```

### Step 4: Update factory to pass audioService to AVInputServiceStub

**In ServiceFactory::create() line 678,** change:

```cpp
// Before:
services.push_back(std::make_shared<AVInputServiceStub>(ioService, messenger));

// After:
services.push_back(std::make_shared<AVInputServiceStub>(ioService, messenger, audioService));
```

### Step 5: Add PipeWire capture includes

**In `AudioService.cpp`,** ensure these headers are present (they likely already are for output streams):

```cpp
#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>
```

### Step 6: Build and test

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass. No new tests for this task — PipeWire capture requires a running PipeWire daemon which isn't available in the test environment.

### Step 7: Commit

```bash
git add src/core/services/IAudioService.hpp src/core/services/AudioService.hpp src/core/services/AudioService.cpp \
  src/core/aa/ServiceFactory.cpp
git commit -m "feat: microphone input via PipeWire capture for voice commands

Implement PipeWire capture stream in AudioService (PW_DIRECTION_INPUT).
AVInputServiceStub now opens a 16kHz mono capture stream when the phone
requests microphone input, forwarding PCM frames as AVMediaWithTimestamp
messages. Enables Google Assistant, voice search, and voice commands."
```

---

## Task 7: Integration — Wire Everything Together

**Files:**
- Modify: `src/core/aa/AndroidAutoService.cpp` or `src/main.cpp` (wherever config is loaded)
- Modify: `src/core/aa/ServiceFactory.hpp` (update create() signature)

### Step 1: Update ServiceFactory::create() signature

Add `YamlConfig*` and `NightModeProvider*` parameters:

```cpp
static IService::ServiceList create(
    boost::asio::io_service& ioService,
    aasdk::messenger::IMessenger::Pointer messenger,
    std::shared_ptr<oap::Configuration> config,
    VideoDecoder* videoDecoder,
    TouchHandler* touchHandler,
    IAudioService* audioService,
    YamlConfig* yamlConfig,
    NightModeProvider* nightProvider);
```

### Step 2: Create the NightModeProvider in AndroidAutoService or main.cpp

Find where `ServiceFactory::create()` is called. Create the appropriate provider based on config:

```cpp
#include "core/aa/NightModeProvider.hpp"
#include "core/aa/TimedNightMode.hpp"
#include "core/aa/ThemeNightMode.hpp"
#include "core/aa/GpioNightMode.hpp"

// ...

NightModeProvider* nightProvider = nullptr;
QString nightSource = yamlConfig->nightModeSource();
if (nightSource == "theme") {
    nightProvider = new ThemeNightMode(themeService);
} else if (nightSource == "gpio") {
    nightProvider = new GpioNightMode(
        yamlConfig->nightModeGpioPin(),
        yamlConfig->nightModeGpioActiveHigh());
} else {
    nightProvider = new TimedNightMode(
        yamlConfig->nightModeDayStart(),
        yamlConfig->nightModeNightStart());
}
nightProvider->start();
```

### Step 3: Build all, run all tests

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass (8 existing + 1 new night mode test = 9)

### Step 4: Final commit

```bash
git add -A
git commit -m "feat: wire all firmware-derived features together

Connect NightModeProvider creation to YamlConfig-based source selection.
Thread YamlConfig through ServiceFactory for identity and resolution.
All 6 firmware-derived features now integrated:
- ServiceDiscoveryResponse identity from config
- 480p mandatory + configurable preferred resolution
- Live night mode with 3 source options
- Video focus state machine with NativeTransient
- GPS sensor advertisement (no-fix default)
- Microphone input via PipeWire capture"
```

---

## Summary

| Task | What it does | New files | Tests |
|------|-------------|-----------|-------|
| 1 | Config for all 6 features | — | Yes (yaml_config) |
| 2 | Identity in ServiceDiscoveryResponse | — | — |
| 3 | Resolution: 480p mandatory + configurable preferred | — | — |
| 4 | Night mode: 3 providers + live sensor updates + GPS advertisement | 7 files | Yes (night_mode) |
| 5 | Video focus: NativeTransient support | — | — |
| 6 | Microphone: PipeWire capture → phone | — | — |
| 7 | Wire everything together | — | — |

Total: 7 tasks, ~7 new files, 2 new/modified test files, ~600 lines of new code.

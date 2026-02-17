# Development Guide

## Build Environments

This project builds on two platforms:

| Environment | Qt Version | Purpose |
|-------------|-----------|---------|
| **Raspberry Pi OS Trixie** | Qt 6.8.2 | Target platform — run and test here |
| **Ubuntu 24.04 VM** (`claude-dev`) | Qt 6.4.2 | Dev builds, unit tests, code iteration |

The codebase must compile cleanly on both. See the Qt compatibility section below for known pitfalls.

## Dependencies

### Raspberry Pi OS Trixie (Debian 13)

```bash
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavcodec-dev libavutil-dev \
  hostapd dnsmasq
```

### Ubuntu 24.04 (Dev VM)

Same packages minus `qt6-connectivity-dev` (Bluetooth), `qt6-wayland`, `hostapd`, and `dnsmasq`. Build/test still works.

## Building

```bash
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Important:** Don't forget `--recurse-submodules` — the aasdk library lives in `libs/aasdk/` as a git submodule.

## Running

### On Raspberry Pi (Wayland/labwc)

```bash
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
./src/openauto-prodigy
```

The Pi runs labwc as its Wayland compositor. Do **not** use `eglfs` — it won't work with this display server setup.

### Via SSH (backgrounded)

```bash
ssh matt@192.168.1.149 'DISPLAY=:0 XDG_RUNTIME_DIR=/run/user/1000 nohup /home/matt/openauto-prodigy/build/src/openauto-prodigy </dev/null >/tmp/oap.log 2>&1 & sleep 1; pgrep -f openauto-prodigy && echo running'
```

Kill with: `ssh matt@192.168.1.149 'pkill -f openauto-prodigy'`

**Note:** Use `pkill -f` not `pkill` — the binary name exceeds 15 characters, and `pkill` silently matches nothing for long names.

## Running Tests

```bash
cd build && ctest --output-on-failure
```

Currently 2 tests: `test_configuration` and `test_theme_controller`.

## Qt 6.4 vs 6.8 Compatibility

These are the known gotchas when writing code that must compile on both versions:

### QML Loading

```cpp
// WRONG — loadFromModule() doesn't exist before Qt 6.5
engine.loadFromModule("OpenAutoProdigy", "main");

// RIGHT — works on all Qt 6.x
engine.load(QUrl(QStringLiteral("qrc:/OpenAutoProdigy/main.qml")));
```

### QML Resource Paths

- Qt 6.4: `qrc:/OpenAutoProdigy/`
- Qt 6.5+: `qrc:/qt/qml/OpenAutoProdigy/`

We use the 6.4 path format. The `qt_add_qml_module` in CMake handles this.

### QML Subdirectory Types

Files in QML subdirectories (e.g., `qml/applications/home/HomeMenu.qml`) need the `QT_QML_SOURCE_TYPENAME` property set in CMake to flatten the type name:

```cmake
set_source_files_properties(qml/applications/home/HomeMenu.qml
    PROPERTIES QT_QML_SOURCE_TYPENAME HomeMenu)
```

### QColor Linking

`QColor` lives in `Qt6::Gui`, not `Qt6::Core`. Any test target using `QColor` must link `Qt6::Gui`.

### QSettings Forward Declaration

Don't forward-declare `class QSettings` inside a namespace — it creates `yournamespace::QSettings` instead of `::QSettings`. Always `#include <QSettings>`.

### QtQuick.Controls Runtime

The `qml6-module-qtqml-workerscript` package is required at runtime for QtQuick.Controls to load. Without it you'll get mysterious QML import failures.

## Project Structure

```
openauto-prodigy/
├── CMakeLists.txt          # Top-level CMake
├── src/
│   ├── CMakeLists.txt      # App target + QML module
│   ├── main.cpp
│   ├── core/
│   │   ├── Configuration.hpp/cpp
│   │   └── aa/             # Android Auto protocol implementation
│   │       ├── AndroidAutoService.hpp/cpp
│   │       ├── AndroidAutoEntity.hpp/cpp
│   │       ├── ServiceFactory.hpp/cpp
│   │       ├── VideoService.hpp/cpp
│   │       ├── VideoDecoder.hpp/cpp
│   │       ├── TouchHandler.hpp/cpp
│   │       └── BluetoothDiscoveryService.hpp/cpp
│   └── ui/                 # Qt/QML controllers
├── qml/                    # QML UI files
├── libs/
│   └── aasdk/              # Android Auto SDK (git submodule)
├── tests/                  # Unit tests
└── docs/                   # Design docs, debugging notes
```

## Debugging Tips

### Boost.Log and Protobuf

Boost.Log truncates multiline output. When logging protobuf messages, always use `ShortDebugString()` (single line) instead of `DebugString()` (multiline).

### Viewing Pi logs

```bash
ssh matt@192.168.1.149 'tail -f /tmp/oap.log'
```

### Q_OBJECT and MOC

If you add a `Q_OBJECT` macro to a header-only class, you **must** create a corresponding `.cpp` file (even if it only contains `#include "Header.hpp"`) and list it in `src/CMakeLists.txt`. Otherwise MOC won't process the header and you'll get `undefined reference to vtable` linker errors.

### Phone Behavior

Phone state can degrade after rapid connect/disconnect cycles during development. If the phone stops responding normally:
1. Clear Android Auto app cache/data on phone
2. Reboot the phone
3. Re-pair Bluetooth

## Contributing

This is early-stage development. The most helpful thing right now is testing with different phones and reporting connection behavior. See [debugging-notes.md](debugging-notes.md) for known issues and discoveries.

# Coding Conventions

**Analysis Date:** 2025-03-01

## Naming Patterns

**Files:**
- Headers: `.hpp` (C++ headers, with `#pragma once`)
- Implementation: `.cpp` (C++ source)
- Tests: `test_<component>.cpp` (e.g., `test_yaml_config.cpp`, `test_plugin_manager.cpp`)
- QML: `.qml` files in `qml/` hierarchy
- Python: `.py` files in `web-config/`

**Classes:**
- PascalCase: `PluginManager`, `AudioService`, `YamlConfig`, `EvdevTouchReader`, `VideoDecoder`
- Prefer descriptive compound names over abbreviations

**Functions/Methods:**
- camelCase: `initializeAll()`, `registerStaticPlugin()`, `createStream()`, `setMasterVolume()`, `requestAudioFocus()`
- Private helper methods: camelCase with explicit purpose (`processSync()`, `countActive()`, `checkGesture()`, `mapX()`, `mapY()`)
- Test methods: `test<Feature>()` pattern (e.g., `testLoadDefaults()`, `testPluginScoping()`)

**Variables:**
- camelCase for public/mutable: `targetDevice`, `enqueueTimeNs`, `focusType`, `bufferMs`
- Private members: `camelCase_` with trailing underscore (e.g., `root_`, `streams_`, `context_`, `handler_`, `devicePath_`, `aaWidth_`)
- Static/const: `UPPER_SNAKE_CASE` (e.g., `MAX_SLOTS = 10`, `GESTURE_FINGER_COUNT = 3`, `OAP_PLUGIN_IID`, `SKIP_THRESHOLD = 3`)
- Atomic/thread-shared: explicit annotation in comments (e.g., `std::atomic<int> pendingAAWidth_{0}`)

**Namespaces:**
- All code in `namespace oap` (OpenAuto Project)
- Sub-namespaces for domain areas: `oap::aa` (Android Auto protocol), `oap::aa::*` for AA-specific internals
- No global `using namespace` directives

**Types:**
- Qt types: `QString`, `QObject`, `QVariant`, `QList<T>`, `QMap<K, V>`, `QByteArray`
- C++ STL: `std::unique_ptr<T>`, `std::shared_ptr<T>`, `std::vector<T>`, `std::atomic<T>`
- Custom types for structured data: struct with public members (e.g., `AudioStreamHandle`, `Slot`)

## Code Style

**Formatting:**
- No automated formatter configuration (Clang-Format/Prettier) detected in repo
- Observed style: 4-space indentation, braces on same line for blocks
- Line length: not enforced (observed 80-120 character lines)
- Smart pointers preferred (`unique_ptr`, `shared_ptr`) over raw pointers

**Includes:**
Order (strictly enforced):
1. Header guard (`#pragma once`)
2. Blank line
3. Project headers (local, relative includes)
4. Qt headers (`<QObject>`, `<QString>`)
5. C++ STL headers (`<vector>`, `<memory>`, `<atomic>`)
6. External libraries (`<yaml-cpp/...>`, `<libavcodec/...>`)
7. C headers (`<fcntl.h>`, `<unistd.h>`, `<sys/ioctl.h>`)

Example from `EvdevTouchReader.hpp`:
```cpp
#pragma once

#include <QThread>
#include <array>
#include <atomic>
#include <vector>
#include <string>
#include <chrono>
#include <linux/input.h>
#include <QDebug>
#include "TouchHandler.hpp"
```

**Linting:**
- Not detected. Code follows Qt/C++ best practices by convention.

## Error Handling

**Return codes:** Prefer bool returns for simple success/failure:
```cpp
bool initialize(IHostContext* context) override;
void shutdown() override;
```

**Null pointers:** Check before use, return gracefully:
```cpp
void registerStaticPlugin(IPlugin* plugin) {
    if (!plugin) return;
    // ...
}
```

**Exceptions:** Wrapped in try/catch at plugin boundary to prevent crashes:
```cpp
try {
    ok = entry.plugin->initialize(context);
} catch (const std::exception& e) {
    qCritical() << "Plugin " << entry.manifest.id
                << " threw during initialize(): " << e.what();
}
```

**Graceful degradation:** Services that depend on external resources (PipeWire, D-Bus, files) fail silently and return `nullptr` or `false`:
```cpp
// AudioService::AudioService()
if (!threadLoop_) {
    qWarning() << "AudioService: Failed to create PipeWire thread loop";
    return;
}
```

**Signal-based error reporting:** PluginManager emits `pluginFailed(id, reason)` for initialization failures.

## Logging

**Framework:** `QDebug` via `qDebug()`, `qInfo()`, `qWarning()`, `qCritical()`

**Patterns:**
- Info level: Lifecycle events (plugin loaded, service started, connection established)
  ```cpp
  qInfo() << "Registered static plugin: " << plugin->id();
  qInfo() << "AudioService: Connected to PipeWire daemon";
  ```
- Warning level: Graceful failures, missing optional resources
  ```cpp
  qWarning() << "AudioService: Failed to create PipeWire thread loop";
  qWarning() << "Plugin: Failed to load shared library";
  ```
- Critical level: Fatal errors within a scope (plugin failed), still recoverable at app level
  ```cpp
  qCritical() << "Plugin " << manifest.id << " threw during initialize()";
  qCritical() << "Plugin " << manifest.id << " failed to initialize — disabled";
  ```
- Debug level: Rarely used; reserved for detailed state dumps
  ```cpp
  qDebug() << "Skipping discovered plugin " << manifest.id
           << " — already registered (static)";
  ```

**Diagnostic logging:** Detailed multi-line logging for complex state (e.g., `EvdevTouchReader::computeLetterbox()`):
```cpp
qInfo() << "[EvdevTouch] X-crop mode: video " << aaWidth_ << "x" << aaHeight_
        << " in " << effectiveDisplayW << "x" << effectiveDisplayH
        << " | AA visible X: " << cropAAOffsetX_
        << " to " << (cropAAOffsetX_ + visibleAAWidth_)
        << " (" << visibleAAWidth_ << " wide)";
```

## Comments

**When to comment:**
- Document non-obvious algorithms or state transitions
- Explain WHY a choice was made, not WHAT the code does
- Mark gotchas, limitations, and cross-module dependencies

**Example (good):**
```cpp
// Gesture detection state
bool gestureActive_ = false;  // suppressing touches during gesture
int gestureMaxFingers_ = 0;   // max simultaneous fingers in current gesture window

// Ring buffer for ASIO → PipeWire bridging (needed because ASIO runs on worker thread)
std::unique_ptr<oap::AudioRingBuffer> ringBuffer;
```

**Example (good — explains WHY):**
```cpp
// Capture callback before tearing down capture stream.
// Order matters: disable callback FIRST, then destroy stream.
capture_.callbackActive.store(false, std::memory_order_release);
```

**DocStrings/Comments:**
- Used sparingly, mostly on public APIs and complex internal functions
- Single-line `//` for brief notes
- Multi-line `/* ... */` for block explanations (rare)

**NO:** Redundant comments like `int count = 0;  // Initialize count to 0`

## Function Design

**Size:** Small, focused functions (10-40 lines typical). Longer functions factored into private helpers.

**Parameters:**
- Input: pass by const reference or value
- Output: use return values (avoid output parameters)
- For Q_INVOKABLE methods used from QML: basic types only (int, QString, bool)

**Example:**
```cpp
QUrl qmlComponent() const = 0;  // const, simple return
void setMasterVolume(int volume) override;  // simple param type
QVariant pluginValue(const QString& pluginId, const QString& key) const;  // refs for efficiency
```

**Return values:**
- `bool` for success/failure
- `nullptr` for "not found" pointers
- Invalid `QVariant` for "missing config value"
- Signals for asynchronous results (`frameReady()`, `pluginLoaded()`)

## Module Design

**Exports:**
- Pure virtual interfaces (`IPlugin`, `IHostContext`, `IAudioService`) define contracts
- Concrete implementations (`PluginManager`, `AudioService`) implement interfaces
- No bare functions at namespace scope; all wrapped in classes

**Headers as contracts:**
- `.hpp` files are the public API surface
- Implementation details stay in `.cpp` files
- Private structs defined in `.hpp` (e.g., `AudioStreamHandle`) to avoid forward-declare hell

**Barrel files:**
- Not used. Each `.hpp` is self-contained with `#pragma once`.

**Singleton pattern:**
- NOT used for services. Services are owned by application and injected via `IHostContext`.

**Plugin pattern:**
- Each plugin implements `IPlugin` interface
- Plugin lifecycle: `initialize()` → `onActivated()` ↔ `onDeactivated()` → `shutdown()`
- Plugins access services via `IHostContext*` passed to `initialize()`

---

*Convention analysis: 2025-03-01*

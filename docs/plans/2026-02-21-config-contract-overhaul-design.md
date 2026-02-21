# Config Contract Overhaul — Design Document

Date: 2026-02-21
Status: Approved

## Problem

The config system has three layers that don't communicate consistently:

1. **YamlConfig** — 30+ typed accessors backed by a YAML node tree with defaults
2. **ConfigService** — a string-key facade that manually maps only 9 of 30+ keys
3. **Direct YamlConfig access** — core AA code bypasses ConfigService entirely

This causes:
- Plugin reads of valid config keys (`touch.device`, `display.width`, `display.height`) silently return empty values
- Installer writes config the runtime ignores
- Default values disagree between runtime, installer, and docs (port 5288 vs 5277, FPS 60 vs 30, password "changeme123" vs "openauto123")
- IPC theme endpoint lies (returns `{"ok":true}` for a no-op)
- Installer diagnostics look for `manifest.yaml` while runtime expects `plugin.yaml`
- GUI tests fail without display server

## Architecture Decision

**Two-tier config access with generic traversal:**

- **Core code** (ServiceFactory, BluetoothDiscovery, AndroidAutoService) keeps typed YamlConfig accessors. These are internal, compile-time safe, and work correctly.
- **Plugins** use ConfigService as the stable public API. ConfigService delegates to a new generic dot-path traversal method inside YamlConfig.
- **No manual key mapping table.** The mapping table is the root cause of drift and is eliminated entirely.

### ConfigService Generic Traversal

ConfigService::value() delegates to `YamlConfig::valueByPath()`:

```cpp
// YamlConfig — encapsulates traversal, const-safe, validates paths
QVariant YamlConfig::valueByPath(const QString& dottedKey) const;
bool YamlConfig::setValueByPath(const QString& dottedKey, const QVariant& value);
```

Key design constraints (from Codex review):
- **Const-safe reads:** Uses `YAML::Clone()` or const-aware traversal to prevent YAML::Node shared-handle mutation on read
- **Write validation:** `setValueByPath()` only writes to paths that exist in defaults (rejects typos, returns false)
- **Plugin-scoped config stays separate:** `pluginValue()`/`setPluginValue()` use plugin ID as a separate parameter, avoiding the dotted-ID problem (`org.openauto.android-auto` contains dots)
- **Type detection:** Reuses existing try-bool/int/double/string pattern from `pluginValue()`

ConfigService becomes a thin wrapper:
```cpp
QVariant ConfigService::value(const QString& key) const {
    return config_->valueByPath(key);
}
void ConfigService::setValue(const QString& key, const QVariant& val) {
    config_->setValueByPath(key, val);
}
```

## Changes

### 1. YamlConfig: Add valueByPath() / setValueByPath()

New methods in YamlConfig that split dotted keys and walk the YAML tree. Extracted `yamlNodeToVariant()` helper (already exists in `pluginValue()` logic).

### 2. ConfigService: Replace Manual Mapping

Remove the if/else chain in `value()` and `setValue()`. Delegate to `valueByPath()`/`setValueByPath()`. Plugin methods unchanged.

### 3. Default Value Alignment

| Setting | Old Runtime | Old Installer | New Canonical |
|---------|------------|---------------|---------------|
| TCP port | 5288 | 5277 | **5277** |
| Video FPS | 60 | 30 | **30** |
| WiFi password | "changeme123" | "openauto123" | **"prodigy"** |

Files: `YamlConfig.cpp`, `install.sh`

### 4. IPC Theme Endpoint

Implement `handleSetTheme()`:
- Write theme YAML to `~/.openauto/themes/<name>/theme.yaml`
- Reload ThemeService
- Return honest success/failure status

### 5. Installer Diagnostics

Change `install.sh:448` from `manifest.yaml` to `plugin.yaml`.

### 6. Headless Test Fix

Add `set_tests_properties(test_configuration test_theme_service PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")` to `tests/CMakeLists.txt`.

### 7. Config Schema Docs

Rewrite `docs/config-schema.md` from scratch based on `YamlConfig::initDefaults()`. Every key, type, default, and consumer documented.

### 8. Config Key Coverage Test

New `test_config_key_coverage` that creates a default config, wraps in ConfigService, and verifies every documented key returns a valid QVariant. Also tests installer-generated keys.

## Testing Strategy

- Existing 14 tests must continue passing
- New `test_config_key_coverage` validates no key is silently dropped
- Existing `test_config_service` updated to test generic traversal (deep keys, nested keys, missing keys)
- Codex review after each implementation step

## Files Modified

| File | Change |
|------|--------|
| `src/core/YamlConfig.hpp` | Add `valueByPath()`, `setValueByPath()`, extract `yamlNodeToVariant()` |
| `src/core/YamlConfig.cpp` | Implement traversal methods, update defaults |
| `src/core/services/ConfigService.cpp` | Replace manual mapping with delegation |
| `src/core/services/IpcServer.cpp` | Implement `handleSetTheme()` |
| `install.sh` | Fix password default, fix plugin diagnostics filename |
| `tests/CMakeLists.txt` | Add headless env, add new test target |
| `tests/test_config_service.cpp` | Add traversal tests |
| `tests/test_config_key_coverage.cpp` | New test file |
| `docs/config-schema.md` | Full rewrite |

## Review Process

Each implementation step will be reviewed by Codex before moving to the next.

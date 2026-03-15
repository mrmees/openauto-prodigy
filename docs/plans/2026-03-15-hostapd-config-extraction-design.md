# HostapdConfig Extraction Design

## Goal

Finish the `HostapdConfig` extraction so startup WiFi credential sync is isolated from `main.cpp`, preserves the current behavior fix, and stays easy to test.

## Context

`src/main.cpp` currently loads `~/.openauto/config.yaml`, then reconciles WiFi AP credentials from `/etc/hostapd/hostapd.conf` before Android Auto services start. The current extraction already moved parsing and YAML sync into `src/core/system/HostapdConfig.*`, but `main.cpp` still owns some coordination logic and the new helper needs final verification and cleanup.

This work supports the product goal of reliable Pi wireless Android Auto by ensuring the Bluetooth credential exchange uses the actual hostapd passphrase instead of stale YAML state.

## Approaches Considered

### 1. Keep `HostapdConfig` as a pure text parser/sync helper

- Lowest churn.
- Leaves file-loading logic in `main.cpp`.
- Acceptable, but spreads hostapd-specific behavior across two places.

### 2. Make `HostapdConfig` a small hostapd-specific utility

- Recommended.
- Centralizes parse and load behavior in one module.
- Keeps startup call site small without inventing a larger abstraction.

### 3. Introduce a broader startup config-sync subsystem

- Better long-term abstraction if multiple system-owned configs need reconciliation.
- Not justified for this bugfix/finalization pass.
- Higher regression risk for no immediate user benefit.

## Design

### Architecture

- Keep a narrow `HostapdConfig` helper in `src/core/system/`.
- It should expose hostapd WiFi credential parsing and a high-level sync path that `main.cpp` can call.
- `main.cpp` should only load YAML, invoke the helper, log a successful sync, and save the YAML when values changed.

### Error Handling

- Missing `/etc/hostapd/hostapd.conf` remains non-fatal.
- Missing `ssid` or `wpa_passphrase` must not partially overwrite YAML.
- Parsing ignores blank and commented lines.
- Logging should report successful syncs; malformed or absent hostapd data should fail quietly unless a new helper makes a clear load failure worth logging.

### Testing

- Keep regression coverage for parsing SSID + passphrase.
- Keep regression coverage for updating stale YAML credentials.
- Add coverage for incomplete hostapd input so partial data does not mutate YAML.
- If a file-loading helper is introduced, add a focused test for it only if it can be exercised without fragile environment coupling.

## Out of Scope

- General startup configuration abstraction.
- Changing Bluetooth discovery credential flow.
- Pi deployment/runtime changes beyond producing a verified binary.

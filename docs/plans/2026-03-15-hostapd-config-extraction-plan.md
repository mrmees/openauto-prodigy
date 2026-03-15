# HostapdConfig Extraction Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Finalize the `HostapdConfig` extraction so startup WiFi credential sync reads cleanly, preserves hostapd as the source of truth, and is covered by regression tests.

**Architecture:** Keep `HostapdConfig` as a small hostapd-focused utility in `src/core/system/` that owns credential parsing and the top-level sync helper, while `main.cpp` only coordinates YAML load/save and logging. Extend tests around the extracted helper before making any cleanup changes so the final refactor stays grounded in the actual AA credential-sync bug.

**Tech Stack:** C++17, Qt 6, CMake, QTest

---

### Task 1: Lock the intended helper behavior with tests

**Files:**
- Modify: `tests/test_hostapd_config.cpp`
- Test: `tests/test_hostapd_config.cpp`

**Step 1: Write the failing test**

Add a test proving incomplete hostapd input does not mutate YAML:

```cpp
void syncWifiCredentialsIgnoresIncompleteHostapdData()
{
    oap::YamlConfig config;
    config.setWifiSsid("Prodigy_live");
    config.setWifiPassword("CorrectPassword123");

    const QByteArray hostapdConfig = "ssid=Prodigy_new\n";

    const bool changed = oap::syncWifiCredentialsFromHostapd(config, hostapdConfig);

    QVERIFY(!changed);
    QCOMPARE(config.wifiSsid(), QString("Prodigy_live"));
    QCOMPARE(config.wifiPassword(), QString("CorrectPassword123"));
}
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . -j$(nproc) --target test_hostapd_config && ctest -R test_hostapd_config --output-on-failure`

Expected: new test fails if the helper incorrectly applies partial credentials.

**Step 3: Write minimal implementation**

Keep or adjust `HostapdConfig` so it returns no credentials unless both `ssid` and `wpa_passphrase` are present and non-empty.

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) --target test_hostapd_config && ctest -R test_hostapd_config --output-on-failure`

Expected: `test_hostapd_config` passes.

### Task 2: Clean up the extraction boundary

**Files:**
- Modify: `src/core/system/HostapdConfig.hpp`
- Modify: `src/core/system/HostapdConfig.cpp`
- Modify: `src/main.cpp`

**Step 1: Write the failing test**

If introducing a higher-level helper for `main.cpp`, first extend `tests/test_hostapd_config.cpp` with a narrowly scoped test that covers the new helper behavior via in-memory input.

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . -j$(nproc) --target test_hostapd_config && ctest -R test_hostapd_config --output-on-failure`

Expected: failure tied to the missing helper behavior rather than unrelated compile/test errors.

**Step 3: Write minimal implementation**

- Collapse the startup call site so `main.cpp` does not manually parse hostapd text.
- Keep logging and YAML persistence in `main.cpp`.
- Keep the helper API narrow; do not invent a generalized config-sync framework.

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) --target test_hostapd_config && ctest -R test_hostapd_config --output-on-failure`

Expected: targeted tests pass with the cleaned-up boundary.

### Task 3: Verify the repo-level impact

**Files:**
- Modify if needed: `docs/session-handoffs.md`

**Step 1: Run the required build**

Run: `cd build && cmake --build . -j$(nproc)`

Expected: build succeeds with no new compile errors.

**Step 2: Run the required test suite**

Run: `cd build && ctest --output-on-failure`

Expected: full suite passes.

**Step 3: Run Pi cross-build verification if behavior-affecting changes remain**

Run: `./cross-build.sh`

Expected: aarch64 build succeeds and produces the Pi binary.

**Step 4: Update handoff**

Append a session handoff entry with changed behavior, why it changed, current status, next steps, and verification results.

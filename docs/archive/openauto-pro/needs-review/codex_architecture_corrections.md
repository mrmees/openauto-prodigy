# Architecture Migration Plan --- Corrections & Clarifications

> **Purpose:** This document refines the Architecture Migration
> Implementation Plan to address structural risks discovered during
> design review.\
> **Scope:** These updates are REQUIRED before proceeding with Phase E
> (AA plugin wrapping).\
> **Priority:** High --- prevents architectural dead-ends and runtime
> regressions.

------------------------------------------------------------------------

# 1. Critical Additions to the Plan

## 1.1 NEW TASK (Phase D): Plugin-Specific QML Context Lifecycle

### Problem

Current architecture exposes AA objects (`AndroidAutoService`,
`VideoDecoder`, `TouchHandler`) via
`engine.rootContext()->setContextProperty`.\
The plugin architecture requires **plugin-scoped QML contexts**, not
global ones.

Without this task: - AndroidAutoPlugin cannot safely expose its QML
bindings. - Global context leakage will persist. - Plugin switching will
leak QML contexts. - AA fullscreen behavior will break.

------------------------------------------------------------------------

## NEW Task (Insert before Phase E / Task 14)

### Task: Implement Plugin-Specific QML Context + Activation Lifecycle

**Files:** - Modify: `src/main.cpp` - Modify: `qml/Shell.qml` - Create:
`src/ui/PluginRuntimeContext.hpp` - Create:
`src/ui/PluginRuntimeContext.cpp`

### Required Behavior

When activating a plugin:

1.  Create a **child QQmlContext**

    ``` cpp
    auto* pluginContext = new QQmlContext(engine.rootContext());
    ```

2.  Expose plugin-owned objects to this child context.

3.  Load plugin QML into `Loader` using that context.

4.  On plugin switch:

    -   Destroy previous context
    -   Ensure plugin shutdown is called
    -   Prevent QObject leaks

### Ownership Rules

-   Plugin owns its internal services.
-   Shell owns the QML context.
-   PluginManager coordinates activation and shutdown.

### Lifecycle Model

    Discover → Load → Initialize → (Activate ↔ Deactivate)* → Shutdown

Activation ≠ Initialization.

AA must start only after activation if it depends on QML.

------------------------------------------------------------------------

# 2. YAML Configuration Corrections

## 2.1 Remove pluginConfigs\_ split storage

### Problem

YamlConfig currently stores: - `root_` (YAML::Node) - `pluginConfigs_`
(QMap cache)

This creates a split-brain configuration state.

### Required Change

All plugin configuration must live under:

``` yaml
plugin_config:
  org.openauto.android-auto:
    auto_connect: true
```

Remove `pluginConfigs_` entirely.

Implement:

``` cpp
QVariant pluginValue(pluginId, key) {
    return root_["plugin_config"][pluginId][key];
}
```

No shadow state allowed.

------------------------------------------------------------------------

## 2.2 Implement Deterministic Deep Merge

yaml-cpp does NOT provide deep merge.

Add utility:

``` cpp
YAML::Node mergeYaml(const YAML::Node& defaults,
                     const YAML::Node& override)
```

Rules: - Mapping: recurse - Sequence: override entirely - Scalar:
override - Missing keys: preserve defaults

Add explicit tests for nested merge behavior.

------------------------------------------------------------------------

# 3. Interface Contracts Must Be Explicit

Phase B defines service interfaces but omits critical constraints.

For EACH interface (IAudioService, IConfigService, etc.), document:

### Required Additions

-   Thread affinity (UI thread only? background allowed?)
-   Ownership rules (who deletes returned objects?)
-   Error handling strategy (bool return? exceptions? signals?)
-   Version reporting (for manifest compatibility)

Example:

``` cpp
// IAudioService::createStream()
// - Must be called from UI thread
// - Returns std::unique_ptr<IAudioStream>
// - Stream is thread-safe for PCM writes
// - Returns nullptr on failure
```

Without this, implementations will diverge.

------------------------------------------------------------------------

# 4. Plugin Manifest Version Constraints --- Simplify

Original plan supports:

``` yaml
requires:
  services:
    - AudioService >= 1.0
```

This requires: - Version registry - Semantic constraint parser

For v0.2.0:

### Simplification

Replace with:

``` yaml
requires:
  services:
    - AudioService
```

Only enforce: - API version compatibility - Service presence

Defer semantic constraints.

------------------------------------------------------------------------

# 5. PluginManager Testing Strategy Adjustment

### Problem

Unit testing QPluginLoader-based dynamic loading is fragile.

### Required Change

Split responsibilities:

    PluginDiscovery  → file scanning + manifest parsing
    PluginLoader     → dynamic loading
    PluginManager    → lifecycle orchestration

Unit tests cover: - Discovery - Validation - Static plugin registration

Dynamic .so loading may be covered by integration tests only.

------------------------------------------------------------------------

# 6. Android Auto Migration --- Mandatory Transitional Step

## 6.1 Extract AA Bootstrap Before Wrapping

Before converting to AndroidAutoPlugin:

Create:

    src/core/aa/AaBootstrap.hpp/.cpp

Move ALL AA initialization logic from main.cpp into:

    AaRuntime startAa(...)

This must: - Create AndroidAutoService - Set up context props - Connect
navigation signals - Start EvdevTouchReader - Start AA service

No behavior change.

Only after this extraction is stable may AA be wrapped into a plugin.

------------------------------------------------------------------------

## 6.2 Preserve Startup Order

Currently:

    engine.load()
    → connect signals
    → start touch reader
    → aaService->start()

Plugin migration must preserve this ordering unless proven unnecessary.

Add lifecycle hook:

``` cpp
virtual void onActivated() {}
```

AA should start here if UI dependency exists.

------------------------------------------------------------------------

# 7. Audio/BlueZ/ofono Reality Adjustment

## 7.1 PipeWire

Unit tests must not assume daemon is running.

Convert to: - Integration tests only - Graceful fallback when PipeWire
unavailable

------------------------------------------------------------------------

## 7.2 A2DP Sink Warning

Implementing a full A2DP sink endpoint with SBC/AAC decode is complex.

If possible: - Prefer delegating A2DP handling to PipeWire/BlueZ
stack. - Use plugin only for UI + AVRCP control.

If owning endpoint: - Add explicit codec dependency planning.

------------------------------------------------------------------------

## 7.3 HFP Stack Decision Required

Decide early: - ofono - BlueZ native HF role

Add a spike task to validate stack viability on RPi OS Trixie.

------------------------------------------------------------------------

# 8. Fullscreen/Dominant Plugin Pattern Must Be Explicit

Add to IPlugin:

``` cpp
virtual bool wantsFullscreen() const { return false; }
```

Shell behavior: - Hide nav + status if active plugin wantsFullscreen().

Do NOT hardcode AA logic into Shell.

------------------------------------------------------------------------

# 9. Evdev Gesture Coupling Acknowledgement

3-finger overlay currently modifies AA touch reader.

Long-term: - Introduce IInputService.

Short-term: - Accept coupling but emit overlay signal via EventBus.

------------------------------------------------------------------------

# 10. Install Script Enhancement

Install script must output diagnostics summary:

-   Qt version
-   PipeWire running?
-   BlueZ running?
-   ofono running?
-   Detected touch device
-   Audio devices
-   Plugin directory status

This reduces support burden.

------------------------------------------------------------------------

# 11. Required Phase Ordering Changes

### Updated Critical Path

    A (YAML)
    → B (Interfaces)
    → D (Shell + Plugin Context Lifecycle)
    → AA Bootstrap Extraction (NEW)
    → E (Wrap AA)
    → C (Plugin Discovery)

Plugin context must exist before AA wrapping.

------------------------------------------------------------------------

# 12. Summary of Required Additions

Before Phase E begins, the following MUST be implemented:

-   [ ] Plugin-specific QQmlContext lifecycle
-   [ ] Remove pluginConfigs\_ shadow storage
-   [ ] Deterministic YAML deep merge
-   [ ] Interface contract documentation
-   [ ] Extract AA bootstrap
-   [ ] Explicit fullscreen capability
-   [ ] Simplified manifest requirements

------------------------------------------------------------------------

# Final Directive

Proceed with migration only after:

1.  Plugin QML context lifecycle exists.
2.  AA bootstrap is isolated and stable.
3.  Config system is single-source-of-truth.

These adjustments prevent: - UI context leaks - Config divergence - AA
regression - Lifecycle crashes - Over-engineered manifest complexity

------------------------------------------------------------------------

**End of correction document.**


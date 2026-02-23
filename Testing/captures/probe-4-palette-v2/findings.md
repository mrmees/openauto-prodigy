# Probe 4 Findings: Palette v2 Theming — BLOCKED

## Problem
Phone logs show:
```
GH.CarThemeVersionLD: Updating theming version to: 0
GH.ThemingManager: Not updating theme, palette version doesn't match. Current version: 2, HU version: 0
```

The phone expects palette version 2 from the HU. We report 0 (proto default for unset field).

## Investigation

### Sources checked (all negative):
1. **aasdk proto** — `ServiceDiscoveryResponseMessage.proto` has fields 1-17, no theme field
2. **aasdk HeadUnitInfoData.proto** — fields 1-8, all strings, no version field
3. **GAL docs** (milek7.pl) — extracted from Google's emulator binary. Same proto layout. No palette/theme version.
4. **aa-proxy-rs protos** — same ServiceDiscoveryResponse (fields 1-17), same HeadUnitInfo (fields 1-8)
5. **Firmware analyses** (Alpine, Kenwood, Sony) — no palette/theme protobuf field info
6. **Web search** — no results for "ServiceDiscoveryResponse theme_version" or similar

### Related finding: UiConfig in VideoConfig
The aa-proxy-rs proto has `VideoConfig.ui_config` (field 11) with:
```protobuf
message UiConfig {
    optional Insets margins = 1;
    optional Insets content_insets = 2;
    optional Insets stable_content_insets = 3;
    optional UiTheme ui_theme = 4;
}
```
This controls light/dark/auto theme MODE, not the palette VERSION. But it could be related.

### Hypotheses
1. **Undocumented field in ServiceDiscoveryResponse** (field 18+) — phone reads it via proto reflection
2. **Undocumented field in HeadUnitInfo** (field 9+) — nested in the headunit_info message
3. **Set via UpdateUiConfigRequest** — a runtime message, not part of discovery
4. **Set in a separate channel message** we haven't captured
5. **It's the `session_configuration` field (13)** — a generic int32 that could encode capabilities

## What needs to happen (requires Matthew present)
- **Binary approach:** Add raw protobuf fields experimentally (field 18 in SDR, or field 9+ in HeadUnitInfo) with value 2, capture phone reaction
- **Wire decode approach:** Capture a real production HU's ServiceDiscoveryResponse bytes and decode them to find the field number
- **UpdateUiConfig approach:** Send UpdateUiConfigRequest after session start with palette info

## Impact
Without palette v2, the phone says "Not updating theme" — meaning Material You / dynamic colors from the HU are not applied. The phone falls back to default Google Maps theming. This is cosmetic, not functional.

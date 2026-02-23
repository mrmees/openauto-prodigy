# Probe 3 Findings: Night Mode Sensor Push

## Approach
No code changes needed — analyzed existing captures for night mode behavior.
Night mode sensor (type 0x0d = NIGHT_DATA) is already sent at session start.

## Baseline Night Mode Behavior
From pi-protocol.log (all captures):
- `SENSOR_START_REQUEST` with `08 0d 10 00` — phone requests night mode sensor
- HU responds `SENSOR_START_RESPONSE` with `08 00` — OK
- HU sends `SENSOR_EVENT_INDICATION` with `6a 02 08 00` — night=false (day mode)

## Phone's Response (from logcat)
```
CAR.SENSOR: getDayNightModeUserSetting getting day night mode from CarServiceSettings: car
CAR.SENSOR: isNightModeEnabled DAY_NIGHT_MODE_VAL_CAR returning car day night sensor night=false
CAR.SENSOR: updateDayNightMode(source=SENSOR): userSetting=car, isNightMode=false
CAR.SYS: setting night mode false
CAR.WM: Updating video configuration. isNightMode: false
```
Phone properly reads the sensor and applies day mode.

## Bonus Finding: Theme/Palette Version Mismatch
```
GH.ThemingManager: onProjectionStart
GH.ThemingManager: Setting up theming
GH.CarThemeVersionLD: Updating theming version to: 0
GH.ThemingManager: Not updating theme, palette version doesn't match. Current version: 2, HU version: 0
```
**The phone expects palette version 2 but we report 0.** This means we're missing the palette/theming
support in our SERVICE_DISCOVERY_RESPONSE. This is directly related to Probe 4 (Palette v2).

## Bonus Finding: Car Module Features
Phone reports these active features (from SERVICE_DISCOVERY_RESPONSE):
```
HERO_THEMING, COOLWALK, INDEPENDENT_NIGHT_MODE, NATIVE_APPS,
MULTI_DISPLAY, HERO_CAR_CONTROLS, HERO_CAR_LOCAL_MEDIA,
ENHANCED_NAVIGATION_METADATA, ASSISTANT_Z, MULTI_REGION,
PREFLIGHT, CONTENT_WINDOW_INSETS, GH_DRIVEN_RESIZING, ...
```

## Verdict
Night mode sensor works correctly. No changes needed.
Palette version 2 is the next target (Probe 4).

## TODO (needs Matthew present)
- Toggle day/night mode during live AA session via NavStrip button
- Verify phone UI switches themes in real time
- Capture the SENSOR_EVENT_INDICATION payload for night=true vs night=false

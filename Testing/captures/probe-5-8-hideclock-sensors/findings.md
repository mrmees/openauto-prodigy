# Probe 5+8 Findings: hide_clock + Extra Sensors

## Changes Made
1. `AndroidAutoEntity.cpp`: `response.set_hide_clock(true)` (was false)
2. `ServiceFactory.cpp`: Added COMPASS (2), ACCEL (19), GYRO (20) to sensor channel

## Probe 5: hide_clock

### Result: No observable effect
- `CarInfoInternal` still shows `hideProjectedClock=false` — unchanged from baseline
- `RailStatusBarFrag: Show clock: true` — phone still shows its clock
- `CarDisplayUiFeatures{hasClock=false}` — same as baseline (we don't have a native HU clock)

### Analysis
The `hideProjectedClock` value in `CarInfoInternal` appears to be cached from BT pairing, not read
from the live ServiceDiscoveryResponse. The `hide_clock` field (12) in the deprecated section may
only be read during initial pairing, or may have been superseded by a different mechanism.

`CarDisplayUiFeatures` is the modern approach — `hasClock=true` would tell the phone "I have my own
native clock, don't render yours." This is likely set via the video channel's UiConfig or a different
SDR field, not the deprecated `hide_clock`.

### Verdict: `hide_clock` deprecated field has no effect on modern AA. Use `CarDisplayUiFeatures` instead.

## Probe 8: Extra Sensors

### Result: Phone requests COMPASS and ACCEL, ignores GYRO

**Baseline sensor requests:** (4 total)
- LOCATION (0x01) — twice
- NIGHT_DATA (0x0d)
- DRIVING_STATUS (0x0a)

**With extra sensors:** (7 total)
- LOCATION (0x01) — twice
- NIGHT_DATA (0x0d)
- DRIVING_STATUS (0x0a)
- ACCEL (0x13 = 19) — NEW
- COMPASS (0x02) — NEW
- GYRO — NOT requested despite being advertised

### Phone acknowledgment
Phone logs show all 6 sensors in service discovery:
```
Adding service (Service id=2 type=SensorSourceService
  {sensors=[type=10,type=13,type=1,type=2,type=19,type=20,]
  locationCharacterization=0 fuelType=[] evConnectorType=[]})
```
GYRO (20) is acknowledged but never requested with SENSOR_START_REQUEST.

### Sensor request payloads
- ACCEL: `08 13 10 03` — type=19 (ACCEL), second field=3 (reporting mode? interval?)
- COMPASS: `08 02 10 03` — type=2 (COMPASS), second field=3

The `10 03` suffix on ACCEL/COMPASS (vs `10 00` or `10 01` on others) suggests a different
reporting mode or update interval for IMU sensors.

### Observations
- Two early `requestSensorStart failed` messages before sensor channel was ready
- Phone treats ACCEL and COMPASS as legitimate sensors and requests data
- GYRO (20) is recognized but not actively requested — may need accelerometer data first,
  or may only be used in specific navigation modes (dead reckoning)

### TODO (needs Matthew present)
- Send actual sensor data (fake compass heading, fake acceleration) to see phone behavior
- Check if GYRO gets requested during active navigation
- Test with RPM, FUEL_LEVEL, and other vehicle sensors from the enum

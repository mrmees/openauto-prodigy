# Probe 1 Findings: Version Bump v1.1 → v1.7

## Change
`libs/aasdk/include/aasdk/Version.hpp`: `AASDK_MINOR` changed from 1 to 7.

## Version Negotiation
- **v1.1 baseline:** HU sends `00 01 00 01`, phone responds `00 01 00 07`
- **v1.7 probe:** HU sends `00 01 00 07`, phone responds `00 01 00 07`
- Phone logcat confirms: `Car requests protocol version ian[majorVersion=1, minorVersion=7]`
- `Negotiated protocol version ian[majorVersion=1, minorVersion=7] (STATUS_SUCCESS)`

**Key finding:** Phone always responds v1.7 regardless of what we request. But requesting v1.7 changes what the phone stores internally:
- Before: `headUnitProtocolVersion=1.1` in CarInfoInternal
- After: `headUnitProtocolVersion=1.7` in CarInfoInternal

## Service Discovery Differences
- **Baseline (v1.1):** SERVICE_DISCOVERY_REQUEST = 841 bytes
- **Probe (v1.7):** SERVICE_DISCOVERY_REQUEST = 881 bytes (+40 bytes)
- First 64 bytes of payload identical (PNG icon header)
- Extra 40 bytes likely additional capability declarations

## Channel Open Order Change
- **v1.1:** VIDEO first, then INPUT, MEDIA, SPEECH, SYSTEM, SENSOR, BT, WIFI, AV_INPUT
- **v1.7:** INPUT, SENSOR, BT, WIFI, AV_INPUT first, then MEDIA+SPEECH+SYSTEM+VIDEO in a batch

## Audio
- No observable change in audio sample rates or TTS configuration
- Phone still uses same 3 audio streams (MEDIA, TTS, SYSTEM)
- 48kHz speech audio not activated by version bump alone — needs explicit config

## Verdict
**Keep v1.7.** No regressions observed, phone properly negotiates, and the larger SERVICE_DISCOVERY_REQUEST suggests the phone may send richer capabilities at v1.7. The version bump is a prerequisite for other v1.7 features.

# Android Auto Wishlist Research Baselines

Date: 2026-07-24
Primary phone reference: Android Auto `17.3.662804-release` (`173662804`)

## Purpose

These notes are the durable starting point for wishlist features whose design
depends on Android Auto protocol behavior. They are research handoffs, not
approved scope or implementation plans. Each note separates confirmed protocol
behavior, current Prodigy state, recommended boundaries, and the minimum live
capture needed before promotion.

The central architectural rule is that Android Auto does not become the
hardware driver. Prodigy owns local camera capture, tuner audio, sensors, and
vehicle I/O. AA can receive semantic state, render a phone-side UI, and send
bounded requests back to Prodigy.

## Baseline index

| Wishlist area | Baseline | Current conclusion |
|---|---|---|
| Backup camera | [backup-camera.md](backup-camera.md) | Camera remains a native Prodigy surface; coordinate with AA through video focus |
| FM/broadcast radio | [broadcast-radio.md](broadcast-radio.md) | Prodigy owns tuner and audio; AA service type 15 supplies phone-rendered browse/control UI |
| AA vehicle control | [vehicle-control.md](vehicle-control.md) | Prodigy advertises typed semantic properties and mediates every request through an allowlisted backend |
| Local media in AA | [car-local-media.md](car-local-media.md) | Service type 20 publishes local state/metadata and accepts five transport actions |
| AA audio policy/volume | [audio-focus-and-volume.md](audio-focus-and-volume.md) | Per-stream volume is a Prodigy policy; AA exposes stream roles and focus, not a volume-setting protocol |
| Native cluster-lite | [semantic-secondary-display.md](semantic-secondary-display.md) | Render navigation, media, and phone semantic channels in a native second QML window |
| Blended UI/live viewport | [blended-ui-and-live-viewport.md](blended-ui-and-live-viewport.md) | One MAIN stream can be inset and recomposed; 17.3 has a live UI-config path requiring a capture probe |
| Projected multi-display | [projected-multi-display.md](projected-multi-display.md) | Each display is an independent video/input instance and stream, never a crop of one panoramic canvas |
| Vehicle sensors | [vehicle-sensors.md](vehicle-sensors.md) | Phone subscribes only to sensors Prodigy advertised; Prodigy replies and streams normalized readings |
| Key/rotary input | [input-key-rotary.md](input-key-rotary.md) | Rotary is a relative-input event plus an advertised capability; current Prodigy only sends touch/buttons |

## Confidence language

- **Confirmed — 17.3 static:** directly traced in the preserved decompiled AA
  17.3 phone code.
- **Confirmed — protocol reference:** represented by the separately maintained
  protocol-reference schemas and analysis.
- **Code-confirmed — Prodigy:** observed in this Prodigy source tree on the
  date above.
- **Hypothesis / runtime-open:** plausible from static code, but not yet shown
  in a live phone/head-unit exchange.
- **Recommendation:** proposed Prodigy architecture, not observed phone
  behavior.

## Evidence corpus and reproducibility

The source APK bundle and full JADX output are retained outside this repository
at:

```text
E:\claude\personal\github\open-android-auto-clean\analysis\aa_apk_17.3.662804_apkm\
```

The reviewable protocol reports are in the sibling `open-android-auto-clean`
repository, including `analysis/reports/multi-display/`,
`docs/channels/radio.md`, `docs/channels/carcontrol.md`, and the channel docs
for media, input, sensor, navigation, audio, and video.

Artifact identities:

- APKM SHA-256:
  `1db7ce995aa52b2cde47a01abfb0364220fb57fc60217de3ec714e3034795344`
- `base.apk` SHA-256:
  `5557827f259898bdab97b489e1a0aef937fd6ec711d87361cf25d51af6f48619`
- Protocol-reference snapshot:
  `8a0861a91199c48d6693ab404cfd5fc4804dcc6f`

Obfuscated class names are version-specific. The artifact hashes, log tags,
service types, wire message IDs, and source anchors make the findings
re-checkable when a newer AA bundle is analyzed.

## Shared activation-probe rules

For radio, CarLocalMedia, car control, projected secondary displays, blended UI,
and live viewport updates:

1. Advertise only the single experimental capability under test.
2. Start with a truthful simulator before connecting real vehicle hardware.
3. Save the complete service discovery response and all traffic on the test
   channel, plus relevant phone logcat.
4. Record whether the phone created a UI/service, which initial state it
   requested, and what happens on disconnect/reconnect.
5. Do not promote the feature from schema presence alone; current phones may
   gate OEM-facing surfaces by version, flags, identity, or policy.

# Blended MAIN UI and Live Viewport Changes

## Baseline conclusion

Blended UI is composition inside one logical MAIN display. Prodigy advertises
the projected content region and native regions; AA arranges its own UI within
that display, while Prodigy renders gauges, climate, camera, navbar, or other
native content around it. This is not a second screen and not a crop selected
from a larger AA framebuffer.

AA 17.3 also has a live `UpdateUiConfigRequest` path that recomputes display
parameters and encoder region-of-interest state. Static analysis therefore
identifies a credible mechanism for navbar-driven viewport changes, but a live
HU-initiated capture is still required before calling it supported.

## 17.3 configuration model

The selected `VideoConfig` and its `AdditionalVideoConfig` can describe:

- content bounds and asymmetric inset rectangles;
- native UI elements and regions;
- allowed resize-to-smaller/larger actions;
- display features and native-UI affordance;
- per-corner radii; and
- a blended-UI subtree containing native-element geometry.

`itt.java:139-350` converts these values into Android display bounds,
`CarDisplayUiFeatures`, `CarDisplayCornerRadii`, and
`CarDisplayBlendedUiConfig` for one display instance.

## Live update trace

In the 17.3 phone endpoint:

- `0x8009` is accepted HU -> Phone as an `UpdateUiConfigRequest` containing an
  updated UI configuration (`jdc.java:191-215`);
- `0x800A` is the opposite-direction phone -> HU update sender
  (`jdc.java:125-136`); and
- the received update is merged, display parameters are recomputed, and a live
  encoder may receive updated ROI parameters (`itt.java:796+`).

The current Prodigy message-name table and older protocol notes do not fully
match this 17.3 direction/numbering. Treat the direct 17.3 endpoint as the
baseline for the experiment, and reconcile schemas/IDs upstream before a
production implementation. `0x8012` is a theming-token response, not the
viewport update mechanism.

## One authoritative geometry transaction

A future implementation should change these together on the Qt owner thread:

1. rendered native/navbar geometry;
2. the AA UI configuration sent on the video channel;
3. decoder-surface placement/crop;
4. display-to-AA touch transform and input eligibility; and
5. rollback state if the phone rejects, disconnects, or fails to converge.

Do not let QML, service discovery, decoder placement, and evdev mapping each
derive their own viewport. Until runtime support is proven, navbar geometry
should remain stable for the projection session or require a controlled AA
restart rather than visually moving ahead of the phone.

## Minimum probe

1. First advertise a fixed embedded MAIN region at session start and confirm
   AA layout, native surrounding content, touch edges, and all orientations/
   navbar edges intended for support.
2. During projection, send one 0x8009 update that changes only one inset.
3. Capture the request, any response/phone-originated update, logcat, encoder
   dimensions/ROI, decoded frames, and exact touch mapping.
4. Exercise smaller/larger actions, rapid reversal, unsupported values,
   disconnect during update, and session reconnect.
5. Require atomic rollback or session restart when convergence is not proven.

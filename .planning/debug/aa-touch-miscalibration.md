---
status: diagnosed
trigger: "AA touch passthrough has wrong calibration"
created: 2026-03-03T00:00:00Z
updated: 2026-03-03T00:00:00Z
---

## Current Focus

hypothesis: touch_screen_config is reduced by margins but EvdevTouchReader maps to full aaWidth_/aaHeight_, creating a coordinate space mismatch
test: trace coordinate spaces through both paths
expecting: mismatch between what phone expects and what we send
next_action: return diagnosis

## Symptoms

expected: Tapping an AA button on screen should activate it on the phone
actual: Touch lands in wrong spot — miscalibrated
errors: none (functional bug, not a crash)
reproduction: tap any AA button with navbar visible during AA
started: after navbar-during-AA feature was added

## Evidence

- timestamp: 2026-03-03
  checked: ServiceDiscoveryBuilder::buildInputDescriptor (lines 288-324)
  found: touch_screen_config is set to (touchW - marginW) x (touchH - marginH). For 1280x720 with bottom navbar, margins are computed from viewport 1024x544. marginH = round(720 - (1280 / (1024/544))) = round(720 - 682.5) = 38. So touch_screen_config = 1280 x 682.
  implication: Phone expects touch coordinates in range [0..1280] x [0..682]

- timestamp: 2026-03-03
  checked: EvdevTouchReader::mapX/mapY (lines 104-120)
  found: mapX maps to [0..visibleAAWidth_] and mapY maps to [0..visibleAAHeight_]. visibleAAWidth_ = aaWidth_ = 1280, visibleAAHeight_ = aaHeight_ = 720 (set on line 43-44 in computeLetterbox).
  implication: Touch coordinates are sent in range [0..1280] x [0..720] -- but phone expects [0..1280] x [0..682]

- timestamp: 2026-03-03
  checked: EvdevTouchReader constructor and computeLetterbox
  found: aaWidth_ and aaHeight_ are always the full video resolution (1280x720). visibleAAWidth_ and visibleAAHeight_ are set to aaWidth_/aaHeight_ unconditionally on lines 43-44. The navbar is accounted for in the letterbox geometry (where the video rectangle sits on screen) but NOT in the output coordinate range.
  implication: The letterbox correctly maps evdev touches to the video area, but outputs in full video coords (0-720 Y) while phone expects content coords (0-682 Y).

## Eliminated

(none)

## Resolution

root_cause: Coordinate space mismatch between touch_screen_config and EvdevTouchReader output range. ServiceDiscoveryBuilder subtracts margins from touch_screen_config (1280x682 for bottom navbar), telling the phone to expect coordinates in that reduced range. But EvdevTouchReader::mapX/mapY always output coordinates in the full video resolution range (1280x720) because visibleAAWidth_/visibleAAHeight_ are set to aaWidth_/aaHeight_ unconditionally. The Y coordinates sent to the phone overshoot what the phone expects, causing vertical touch misalignment.

fix: (not applied -- diagnosis only)
verification: (not applied -- diagnosis only)
files_changed: []

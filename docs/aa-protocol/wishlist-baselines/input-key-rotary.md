# AA Key, Rotary, and Steering-Wheel Input

## Baseline conclusion

Prodigy sends all AA user input on the input channel. Buttons use Android or
AA-specific keycodes; rotary uses a relative-input event, not repeated D-pad
keys. Physical keyboard, HID, encoder, GPIO, or CAN devices should first map to
semantic ActionRegistry actions, which route either to native Prodigy focus or
to the canonical AA input handler according to current display/focus ownership.

## Confirmed input forms

`InputEventIndication` (`0x8001`, HU -> Phone) can carry one touchscreen,
button, absolute, relative, or touchpad event. The HU advertises supported
keycodes, touch/touchpad configuration, haptics, and the associated display ID
in service discovery.

Useful keycodes include:

| Keycode | Meaning |
|---:|---|
| 3 / 4 | Home / Back |
| 5 / 6 | Answer / End call |
| 19-23 | D-pad and center/select |
| 84 / 219 / 231 | Search, Assist, Voice Assist |
| 85-88, 126-127 | Media play/pause/stop/next/previous |
| 260-263 | Navigate previous/next/in/out |
| 65536 | AA rotary-controller capability/scan code |
| 65537-65540 | Media, Navigation, Radio, Telephone shortcuts |

Normal buttons should send press then release. Rotary uses:

```protobuf
RelativeInputEvent {
  scan_code = 65536;
  delta = signed_detent_count;
}
```

Positive and negative deltas scroll opposite directions; zero is ignored.
The phone converts this to an Android rotary `ACTION_SCROLL` event. Advertising
keycode 65536 tells the phone a rotary controller exists.

Do not simultaneously advertise rotary and a touchpad configured for UI
navigation without a specific compatibility test: the phone disables rotary
when that touchpad mode is present.

## Current Prodigy gap

**Code-confirmed — Prodigy:** service discovery advertises touch plus common
home/back/search/media/Assistant buttons. `InputChannelHandler` sends touch and
button events only; it has no relative-input sender, and keycode 65536 is not
advertised. The future change therefore needs both capability advertisement
and serialization, plus product-level focus routing.

For projected multi-display, every input descriptor must carry the matching
logical display ID and every event must travel on that display's input channel.
A global rotary may need an explicit target-display policy.

## Mapping and safety rules

- Physical adapters publish debounced semantic events; they do not call the AA
  transport directly.
- Native UI owns input when native focus/camera is active; AA owns only input
  intentionally routed to a projected display.
- Volume keys normally remain local HU volume actions rather than being sent to
  the phone unless product policy explicitly says otherwise.
- Long press, repeat, detent acceleration, and press-and-turn are distinct
  behaviors and need explicit mappings.
- Disconnect clears held/repeat state so reconnect cannot receive a stale key.

## Minimum rotary probe

Advertise only keycode 65536 in addition to the current baseline, send slow
single detents, fast multi-delta turns, direction reversal, zero, select, back,
and long press. Capture wire events and logcat across launcher, Maps, media,
dialer, Assistant, and keyboard. Verify focus-ring behavior, sign convention,
acceleration expectations, and whether current AA flags gate rotary UI before
designing the physical encoder mapping.

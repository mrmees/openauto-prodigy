# Android Auto Vehicle-Control Bridge

## Answer for the maintainer

AA's car-control channel is a typed semantic remote-control surface, not direct
CAN, GPIO, relay, or Android VHAL access. Prodigy advertises the properties and
control layout it is prepared to support. The phone renders controls from that
description, subscribes to values, and sends set/action requests. Prodigy is
the authoritative bridge that validates each request, invokes an allowlisted
external backend capability, waits for a real acknowledgment, and reports the
result and new state.

That means the wishlist boundary is correct: AA never receives arbitrary bus
or pin access, and an unmapped/disconnected control is unavailable rather than
optimistically successful.

## Confirmed 17.3 endpoint

- GAL service type: `19`
- Phone endpoint: `defpackage/ixb.java`
- Phone service/config consumer: `defpackage/iip.java`
- Log tag: `CAR.GAL.CAR_CONTROL`

The HU's service discovery descriptor contains three related models:

1. property configurations: semantic property, type, read/write access,
   change mode, supported areas, min/max, and availability details;
2. a control tree: property controls, action controls, nested groups, enabled
   state, compact/status-bar metadata, and side affinity; and
3. action entries used to launch named vehicle surfaces.

The phone consumes that model in `iip.java:308-482` and registers listeners for
properties referenced by control groups during connection
(`iip.java:709-723`).

## Wire flow

| ID | Direction | Purpose |
|---|---|---|
| `0x8001` | Phone -> HU | Set one property value; UUID-correlated |
| `0x8002` | HU -> Phone | Set result/status |
| `0x8003` | Phone -> HU | Register property listeners/subscriptions |
| `0x8004` | HU -> Phone | Listener-registration result |
| `0x8005` | HU -> Phone | Property value change |
| `0x8006` | Phone -> HU | Named `CarAction` request |
| `0x8007` | HU -> Phone | Control-group update |

Evidence: `iip.java:251-269`, `:565-592`, `:603-723`, and `:797-938` for phone
behavior; `ixb.java:31-179` for responses and updates received from the HU.

`CarPropertyValue` can carry scalar and array forms of integer, float, boolean,
long, and string values. Areas are masks representing global, window, seat,
mirror, wheel, or door targets. A seat-heater value is therefore not just an
integer: it is a typed property plus a specific supported seat area and
read/write policy.

## Property scope seen in 17.3

The closed semantic property enum includes HVAC temperature and automatic/dual
modes, fan speed/direction, AC and max AC, front/rear defrost, recirculation,
HVAC power, seat heat/ventilation, steering-wheel heat, mirror heat, toll card,
door lock, and a small set of OEM extensions.

This is not evidence that AA provides a generic arbitrary “aux switch” type.
An auxiliary device should only be mapped where one of the phone-supported
semantics is honest and safe. Do not relabel an unrelated relay as seat heat or
defrost merely to make a control appear.

The observed action enum includes launch HVAC, car media, control center, and
alerts. Treat these as requests to open an HU-native surface, not as actuator
commands.

## Recommended authority model

```text
AA UI
  |
  | typed set / subscribe / action request
  v
CarControlChannelHandler
  | schema validation, authorization, correlation, timeout
  v
VehicleControlProvider
  | allowlisted semantic capability + area
  v
User-selected external backend
  | CAN / GPIO / relay / MQTT / other implementation detail
  v
Backend acknowledgment + observed state
  |
  +--> AA response and property-change publication
  +--> native Prodigy UI/API state
```

Required rules:

- capabilities are deny-by-default and configured by semantic property/area;
- access flags match reality (`READ`, `WRITE`, or `READ_WRITE`);
- write success is returned only after backend acknowledgment;
- a successful write is followed by the authoritative property value/change;
- disconnect, stale data, timeout, and backend error become unavailable/failure;
- bounds and enum values are validated before dispatch;
- repeated requests are correlation-safe and do not let late acknowledgments
  complete a newer operation; and
- safety-critical local controls continue to work without AA.

The existing External API action path can trigger a command, but it is not a
complete two-way provider contract: car control also needs current state,
availability, acknowledgments, errors, subscription, and lifecycle ownership.

## Current Prodigy gap and protocol-library caution

**Code-confirmed — Prodigy:** no service type 19 descriptor, channel handler,
provider contract, or car-control state cache is registered. Recovered protos
exist only in the hands-off protocol submodule.

The later protocol reference types some status fields with the shared status
enum where the nested snapshot uses a raw integer; this is wire-compatible but
should be reconciled upstream before production. Do not edit the nested proto
directory in this repository.

## Minimum activation and safety probe

1. Build a disabled-by-default simulated backend with two low-risk properties,
   for example a readable cabin temperature and a bounded read/write fan speed.
2. Advertise only those configurations and a minimal control group.
3. Save service discovery, channel traffic, and logcat; confirm whether the
   current Pixel/Moto phone exposes the UI.
4. Verify initial subscription, current-value publication, accepted set,
   rejected out-of-range set, backend timeout, disconnect, reconnect, and a
   control-group enabled/disabled update.
5. Confirm the phone never gains access to backend identifiers, arbitrary
   payloads, physical addresses, CAN frames, or GPIO numbers.

Real hardware actuation should be a later, separately reviewed gate after the
semantic contract, authorization model, electrical safety, and fail-safe
behavior are explicit.

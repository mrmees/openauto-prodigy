# Vehicle Sensor Provider Bridge

## Baseline conclusion

The AA sensor service is subscription-based. Prodigy advertises only sensor
types it can truthfully supply; the phone requests a type and refresh interval;
Prodigy accepts or rejects the request and sends normalized readings or a
transient/permanent error. OBD-II, CAN, external GNSS, Companion GPS, and local
device APIs are backend sources, not protocol-visible identities.

## Protocol flow

| ID | Direction | Purpose |
|---|---|---|
| `0x8001` | Phone -> HU | Request/subscribe to one sensor type and interval |
| `0x8002` | HU -> Phone | Subscription result |
| `0x8003` | HU -> Phone | One or more batched sensor readings |
| `0x8004` | HU -> Phone | Sensor recovered, transient error, or permanent error |

Useful wishlist types include location, compass, vehicle speed, RPM, odometer,
fuel/range, parking brake, gear, night state, driving status, dead reckoning,
accelerometer, gyro, and GPS satellite data. AA defines more types than
Prodigy should advertise; availability must be real and source-specific.

## Provider architecture

```text
Companion / GNSS / OBD-II / CAN / device backend
                  |
                  v
SensorRegistry: normalized value, units, timestamp, quality,
                availability, source, freshness
          |                         |
          +--> native gauges/API    +--> AA SensorChannelHandler
```

The registry should choose one authoritative source per semantic value,
support deliberate fallback, reject stale/out-of-range data, and rate-limit to
the phone's requested interval plus a local safety ceiling. On source loss,
send the corresponding error and stop inventing values. Reconnect should begin
from a fresh provider snapshot.

Companion GPS is not automatically available to the HU merely because the AA
phone knows its own location. It needs an explicit additive Companion report,
then the same normalization/quality path as external GNSS.

## Current Prodigy gap and schema caution

**Code-confirmed — Prodigy:** service discovery advertises only night,
driving-status, and parking-brake. The handler can publish those three and
tracks phone subscriptions. It has no dynamic provider registry for the wider
sensor set.

The nested handler/proto naming still follows an older
`SensorStartRequestMessage` shape. The protocol-reference audit retracts that
duplicate in favor of the verified `SensorRequest` schema. Reconcile this in
the upstream protocol library before expanding the production handler; do not
edit the hands-off nested proto directory directly.

## Research matrix

For each candidate source/type, record raw units, normalized units, cadence,
timestamp origin, accuracy/quality, stale timeout, reconnect behavior, and a
known-reference comparison. Specifically A/B test:

- phone/Companion location versus external GNSS location;
- location containing speed versus a separate OBD/CAN `CAR_SPEED` report;
- gear `REVERSE` publication independently of local backup-camera ownership;
  and
- truthful unavailable/error behavior when an adapter is unplugged.

Capture the phone's requested intervals and Maps behavior. Do not infer that a
schema-valid location or speed report improves navigation until the live A/B
test shows it.

# CarLocalMedia: Prodigy Media inside Android Auto

## Baseline conclusion

CarLocalMedia service type 20 lets the HU expose locally playing media to a
phone-rendered Android Auto media surface. Prodigy publishes playback state,
source, position, available actions, and track metadata. The phone can request
only play, pause, previous, next, or stop. Audio remains local in Prodigy's
PipeWire path; this is a state/control service, not an audio transport.

## Confirmed 17.3 flow

The phone endpoint is `defpackage/ixi.java`, log tag
`CAR.GAL.CAR_LOCAL_MEDIA`; service implementation is `iiy.java`.

| ID | Direction | Message |
|---|---|---|
| `0x8001` | HU -> Phone | Playback status |
| `0x8002` | HU -> Phone | Playback metadata |
| `0x8003` | Phone -> HU | Playback request |

Status contains playback state, a media-source label, position in seconds, and
the currently available action list. Metadata contains title, artist, album,
album-art bytes, and duration in seconds. The action enum is exactly play,
pause, previous, next, and stop (`0..4`).

`ixi.java:24-137` accepts status/metadata and rejects an inbound request as the
wrong direction. `iiy.java:119-144` constructs and sends the request. The AA
app adapts this state to an Android `MediaSession`, which is why local media can
appear in the normal phone-rendered media experience.

## Prodigy integration

- Add a service type 20 descriptor and handler behind an experimental toggle.
- Adapt the canonical local-media provider; do not create a second player or
  state machine for AA.
- Publish a complete status plus metadata snapshot on channel open and every
  authoritative change.
- Route requests to the same canonical media actions used by native UI.
- Advertise actions dynamically. Do not claim next/previous when the current
  source cannot perform them.
- Use bounded album-art size and update rate; omit art rather than blocking
  the protocol/event thread.
- Keep existing local/AA media arbitration authoritative for actual audio.

**Code-confirmed — Prodigy:** relevant protobufs exist, but service discovery
and the orchestrator register no CarLocalMedia descriptor or handler.

## Minimum activation probe

Advertise only service type 20 on a test toggle, publish a deterministic
simulated track, and capture whether current phones create the local-media
surface. Exercise all five actions, metadata changes, unavailable source,
position progression, reconnect, and conflict with phone-sourced AA media.
Schema presence alone is not enough to assume the current phone UI activates.

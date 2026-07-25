# Broadcast Radio / FM through Android Auto

## Answer for the maintainer

Android Auto does not tune FM or carry the radio audio stream. Prodigy owns the
tuner, demodulation, RDS decoding, audio routing, persistence, and antenna/RF
health. AA service type 15 is a remote browse/control/state surface: the phone
renders the radio UI, sends tune/mute/seek/favorite/search requests, and
receives program lists, current-program metadata, favorites, and operation
results from the HU.

This cleanly supports the wishlist strategy: build one headless canonical
radio backend, probe the AA surface with a simulator, and add a native Prodigy
widget only if current phones do not expose the recovered service well enough.

## Confirmed 17.3 endpoint

- GAL service type: `15`
- Phone endpoint: `defpackage/jai.java`
- Phone service/config consumer: `defpackage/iji.java`
- Log tag: `CAR.GAL.RADIO-EP`
- Scope: terrestrial AM/FM, HD Radio, and DAB data models; no satellite-radio
  protocol was found.

The 17.3 direction and message map is:

| ID | Direction | Purpose |
|---|---|---|
| `0x801A` | HU -> Phone | Program/station list |
| `0x801B` | HU -> Phone | Current program info, mute state, and audio-focus state |
| `0x801C` | Phone -> HU | Mute request |
| `0x801D` | HU -> Phone | Mute response |
| `0x801E` | Phone -> HU | Tune request |
| `0x801F` | HU -> Phone | Tune response |
| `0x8020` | HU -> Phone | Favorites list/state |
| `0x8021` | Phone -> HU | Favorite toggle |
| `0x8022` | Phone -> HU | Seek/tune direction |
| `0x8023` | Phone -> HU | Search or configured band action |

Direct anchors are `jai.java:25-205` for inbound HU publications and responses,
and `iji.java:274-416` for phone requests. `iji.java:320-341` replays cached
mute, current program, program list, and favorites to newly attached phone UI
callbacks, so initial truthful state matters.

## Capability and metadata model

The HU advertises a radio channel configuration in service discovery. The
17.3 configuration describes supported terrestrial bands and per-band search
behavior. Program data can represent:

- station selectors/identifiers and display strings;
- AM/FM frequency and band;
- RDS/RBDS program type and radio text;
- station icon, song title/artist/album and artwork;
- HD Radio station/subchannel/status data;
- DAB identifiers and metadata; and
- favorites and current mute/audio-focus state.

The static schema proves representation, not that every current phone will
show every field. The first probe should use a small, internally consistent FM
station list with one current station and deterministic RDS metadata.

## Required Prodigy split

```text
AA RadioChannelHandler (service type 15)
  requests/responses, publication cache, session lifecycle
                    |
                    v
IRadioBackend (canonical semantic API)
  bands, stations, tune/seek, mute, favorite, state, errors
        |                         |
        +--> SimulatedBackend     +--> RtlSdrBackend
                                         |
                                         +--> PipeWire local audio
                                         +--> RDS metadata
```

Radio audio must enter Prodigy's local PipeWire/focus/EQ policy. It is not sent
to the phone and back over an AA audio channel. AA's radio UI controls the same
backend and state that a future native widget would use.

Backend operations should be asynchronous and explicit:

- request contains a correlation/operation identity where the wire schema
  supports it;
- success is sent only after the backend confirms the operation;
- timeouts and tuner loss produce a failure/unavailable response;
- current-program and list notifications follow successful state changes; and
- session reconnect republishes a complete current snapshot.

The earlier phone service includes a roughly six-second tune timeout. Treat
that as an upper compatibility bound, not a target latency.

## Current Prodigy gap and protocol-library caution

**Code-confirmed — Prodigy:** the service discovery builder and orchestrator do
not register a radio descriptor or handler. Radio protobufs are present in the
hands-off nested protocol submodule, but presence does not activate the phone
surface.

The 17.3 radio configuration has drifted from the nested snapshot: its outer
field 1 is absent in the current phone schema, and the per-band record has an
additional boolean field. The older structure is sufficient for a controlled
activation experiment, but production work should first land any required
schema correction in the upstream protocol-reference repository and then
update the submodule. Do not edit `libs/prodigy-oaa-protocol/proto/` directly.

## Minimum activation probe

No SDR hardware is required for this gate.

1. Add an experimental, disabled-by-default service type 15 descriptor and a
   simulated handler/backend.
2. Publish two or three valid FM stations, one current program, mute=false, and
   an empty favorites set immediately after channel open.
3. Capture service discovery, channel open, all 0x801A-0x8023 messages, and
   phone logcat.
4. Exercise tune, seek both directions, mute/unmute, favorite toggle, and a
   band/search action; verify response ordering and UI convergence.
5. Disconnect/reconnect and verify cached state is republished rather than
   invented by the phone.

Only after that surface is usable should the RTL-SDR spike answer Linux driver,
sample-rate, RF quality, station scan, RDS reliability, CPU, and PipeWire
routing questions. Do not advertise radio in a normal build until the backend
can publish truthful state and complete every request safely.

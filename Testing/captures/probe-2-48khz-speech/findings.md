# Probe 2 Findings: 48kHz Speech Audio

## Change
`src/core/aa/ServiceFactory.cpp`: SpeechAudioServiceStub sample rate from 16000 to 48000.

## Result: SUCCESS — Phone sends 48kHz speech audio

### Evidence
- **Frame size doubled:** 2058 bytes (16kHz) → 4106 bytes (48kHz) — exactly 2x
- Phone logcat: `Start capturing system audio with sampling rate: 48000, buffer size: 16384`
- Phone logcat: `expected frame duration: 42.666667 ms` (vs previous ~21.3ms at 16kHz)

### Phone's Internal Behavior
- Phone logs: `Failed to find 48kHz guidance config, fallback to 16KHz`
  - This message is misleading — it falls back for _config lookup_ but still captures at 48kHz
  - The "guidance config" likely refers to a specific protobuf field in SERVICE_DISCOVERY_RESPONSE
    that we're not setting yet. Adding it might eliminate the warning.
- Phone actually sends 48kHz audio regardless of the fallback message
- The SERVICE_DISCOVERY_RESPONSE advertised config controls what the phone _tries_,
  and our 48000Hz config makes it work.

### Audio Pipeline Latency
- TTS latency stats: `audioPipelineLatency: [196, 147.38] ms` — reasonable for nav guidance
- MAX_UNACK: 1 (single outstanding frame, flow-controlled)

## Verdict
**Keep 48kHz speech.** Higher quality TTS/navigation audio with no regressions.
Consider also bumping SystemAudioServiceStub to 48kHz for system sounds.

## TODO
- Investigate the "48kHz guidance config" protobuf field to eliminate the fallback warning
- Test SystemAudio at 48kHz too (currently still 16kHz mono)

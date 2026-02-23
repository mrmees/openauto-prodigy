# Probe 6 Findings: ALARM Audio Channel — SKIPPED

## Reason
Cannot test without knowing the correct channel ID for ALARM audio.

### Context
- `AudioType` enum has ALARM=4 (in addition to SPEECH=1, SYSTEM=2, MEDIA=3)
- aasdk `ChannelId` enum assigns: 4=MEDIA_AUDIO, 5=SPEECH_AUDIO, 6=SYSTEM_AUDIO, 7=AV_INPUT, 8=BLUETOOTH
- Channels 9-13 exist in production SDKs but are NOT mapped in aasdk
- Adding ALARM on a wrong channel would likely break the connection

### What we know from firmware analysis
Production SDKs (Kenwood, Sony) have 30+ additional service types beyond what aasdk implements.
The channel-to-service mapping for channels 9-13 is an open question.

### What would be needed
1. Discovery of ALARM audio's channel ID (likely 9-13 range)
2. Add the new ChannelId to aasdk's enum
3. Create AudioServiceChannel subclass for the new channel
4. Register in ServiceFactory

### Alternative approach
Could try advertising ALARM type on an existing channel (e.g. use channel 6 for both
SYSTEM and ALARM). But a single channel can only have one AudioType, so this would
replace SYSTEM audio, not add ALARM.

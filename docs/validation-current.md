# Current Milestone Validation

## Native turn-card remaining delivery questions

- **Multi-stop route:** Maps source establishes that destination and
  destination-distance index zero describe the next stop, but numeric
  `time_to_arrival_seconds` may describe the final route. Capture a moving
  multi-stop route before presenting that numeric duration when more than one
  destination exists.
- **Roundabout route:** Maps source establishes positive exit numbers and
  1–360 degree sweep angles, but the completed moving route did not exercise a
  roundabout. Capture one before promoting roundabout-specific presentation.
- **Other navigation providers:** Maps 26.30.05 intentionally publishes one
  current step and does not populate current road. Treat multi-step and
  current-road behavior from another provider as new evidence, not an implied
  Maps capability.

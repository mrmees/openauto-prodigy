# Current Milestone Validation

## Native turn-card trip summary delivery

- **2026-07-31, Samsung S25+, GAL 6.0, stationary active route:** repeated
  `NavigationNotification` delivery included a nonempty destination address,
  establishing that field for the adaptive destination footer. Two temporary
  structural probes observed no `NavigationNextTurnDistanceEvent` during the
  stationary route, so destination distance, ETA, and remaining time are not
  yet promoted. Recheck those fields during a moving route before exposing
  them; absence while stationary is not evidence that the phone never sends
  them.

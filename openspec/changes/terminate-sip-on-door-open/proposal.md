# Proposal: Terminate SIP when the door is opened

Opening the main door completes the intercom interaction, but the daemon
currently only sends the unlock UART command and leaves the SIP session alive.
The active call must be terminated immediately so Asterisk releases every
remaining forked ringing destination.

## Scope

- End an active SIP call after a successful MQTT door unlock when the
  configurable behavior is enabled.
- Keep DTMF `#` door unlock inside the current conversation so the caller can
  decide when to hang up.
- Expose the behavior as a retained MQTT/Home Assistant switch.
- Preserve the unlock pulse, telemetry and existing behavior when no SIP call
  is active.
- Add a regression assertion to the SIP media orchestration test.

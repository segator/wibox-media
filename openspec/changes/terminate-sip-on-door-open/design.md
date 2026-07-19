# Design: Terminate SIP when the door is opened

`unlock_door()` remains the single unlock path for DTMF and MQTT commands. Once
the UART unlock command succeeds and telemetry is published, it checks that the
source is MQTT, the `hangup_on_door_unlock` setting is enabled and a SIP call is
active. Only then does it call `sip_calling_terminate_call()`. The normal SIP
state callback then performs the existing media and intercom cleanup and sends
the SIP BYE/CANCEL to Asterisk. DTMF unlocks do not terminate the call.

The setting defaults to enabled, is accepted in the daemon config file and is
exposed as a retained MQTT/Home Assistant switch. No Home Assistant-specific
hangup service is needed: the MQTT button already routes to the daemon, so
this keeps door control and call cleanup atomic at the device boundary.

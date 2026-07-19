# Access-control delta

## ADDED Requirements

### Requirement: Door opening terminates the active SIP call

After a successful main-door unlock from MQTT, if a SIP call is active and the
behavior is enabled, the daemon SHALL terminate that SIP call immediately. A
DTMF `#` unlock SHALL NOT terminate the current SIP call. If no SIP call is
active, the unlock SHALL still succeed without creating a call.

#### Scenario: Opening from the Home Assistant notification

- GIVEN the doorbell SIP call is ringing or established
- WHEN the user presses the Home Assistant open-door action
- THEN the daemon sends the unlock command
- AND terminates its SIP session
- AND Asterisk receives the termination and stops all remaining ringing legs

#### Scenario: DTMF keeps the conversation open

- GIVEN an established SIP call
- WHEN the caller sends DTMF `#`
- THEN the daemon unlocks the door
- AND the SIP call remains established
- AND the caller may hang up normally afterwards

#### Scenario: Opening with no SIP call

- GIVEN no SIP call is active
- WHEN an authorized MQTT unlock command is accepted
- THEN the daemon sends the unlock command
- AND does not attempt SIP termination

# Proposal

## Why

The firmware updater verifies TLS certificate chains, but currently continues if OpenSSL cannot configure hostname verification. That can turn a setup failure into a connection that is authenticated only by its CA chain.

## What Changes

- Abort HTTPS requests when hostname verification cannot be configured.
- Add regression coverage for hostname-verification setup failure.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `firmware-updates`: Require hostname-verification setup to succeed before any HTTPS request.

## Impact

Changes the firmware updater TLS handshake, its host-side mock test, and the firmware-updates specification. No new dependency or hardware behavior.

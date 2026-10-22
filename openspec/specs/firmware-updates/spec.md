# Firmware Updates Specification

## Purpose

Define release discovery, authenticated download, guarded flash and recovery-
safe behavior for routine custom-firmware updates.

## Requirements

### Requirement: Release update discovery

When enabled, the daemon SHALL check the configured `owner/repo` latest GitHub
Release at startup, approximately daily and on explicit refresh. Only supported
semantic version tags SHALL be compared.

#### Scenario: A newer release exists

- GIVEN the remote semantic version is newer than the installed version
- WHEN an update check succeeds
- THEN update available becomes ON
- AND the remote version is published
- AND install availability becomes online

### Requirement: HTTPS and checksum download

The updater SHALL verify TLS peer certificates and hostnames, follow at most
five redirects, download the versioned image and `MD5SUMS`, and verify the image
MD5 and size before touching flash. It SHALL abort a connection if it cannot
configure hostname verification, and SHALL NOT send an HTTP request on that
connection.

#### Scenario: Hostname verification cannot be configured

- GIVEN the TLS library cannot configure hostname verification for the requested host
- WHEN the updater prepares an HTTPS connection
- THEN the connection is aborted before sending an HTTP request

#### Scenario: Untrusted certificate chain

- GIVEN the server certificate chain cannot be verified against the configured CA bundle
- WHEN the updater establishes an HTTPS connection
- THEN the TLS handshake fails
- AND the updater sends no HTTP request

#### Scenario: Download checksum differs

- GIVEN the downloaded image does not match its release MD5
- WHEN pre-flash verification runs
- THEN the updater deletes or rejects the image
- AND does not enter the flash phase

### Requirement: Character-device MTD flash

Routine OTA SHALL unmount `/usr`, erase and write `/dev/mtd4` through the MTD
character device, respecting MTD geometry and the updater image-size limit.

#### Scenario: Image exceeds the safe limit

- GIVEN an update image is empty or too large for the supported partition
- WHEN installation is requested
- THEN installation fails before erase

### Requirement: Mandatory read-back verification

After writing, the updater SHALL hash the flashed prefix and require it to match
the expected MD5. Unsupported `fsync` errors such as `EINVAL`, `ENOTTY`,
`EOPNOTSUPP` or `ENOSYS` MAY be tolerated only because read-back verification
remains mandatory; other `fsync` errors SHALL fail the update.

#### Scenario: MTD does not implement fsync

- GIVEN flash writing completed and `fsync` reports an explicitly unsupported error
- WHEN the updater continues
- THEN it reads back the flashed bytes
- AND success is possible only if the read-back MD5 matches

#### Scenario: Read-back checksum differs

- GIVEN flash has been touched
- WHEN read-back MD5 verification fails
- THEN the updater reports a critical failure
- AND does not clear the OTA guard or reboot automatically

### Requirement: OTA critical guard

Before flash, the updater SHALL publish a PREPARE guard, stop all media-daemon
processes and disarm the hardware watchdog. It SHALL transition the guard through
FLASHING, VERIFYING and COMPLETE as applicable.

#### Scenario: Preparation fails before flash

- GIVEN the daemon cannot be stopped or watchdog cannot be safely disarmed
- WHEN OTA preparation runs
- THEN flashing is aborted
- AND the PREPARE guard is cleared for daemon recovery

#### Scenario: Failure occurs after erase begins

- GIVEN flash has been touched
- WHEN installation fails
- THEN the guard remains present
- AND the app watchdog does not restart the daemon from an unmounted or partial image

### Requirement: Detached install execution

An updater accepted through MQTT SHALL run in a separate session with standard
input detached. Stopping the launching media daemon and closing its logging PTY
SHALL NOT terminate the updater during OTA preparation.

#### Scenario: Updater stops its launching daemon

- GIVEN the media daemon accepted an MQTT install request
- WHEN the updater enters PREPARE and stops all media-daemon processes
- THEN the updater remains alive outside the daemon session
- AND continues to watchdog disarm and guarded flash

### Requirement: Successful reboot and duplicate protection

A normal verified update SHALL sync and reboot. Home Assistant SHALL disable the
install button immediately after accepting a request and ignore duplicates while
the updater runs.

#### Scenario: Install is pressed twice

- GIVEN an update install is already running
- WHEN another install command arrives
- THEN the second command is ignored

### Requirement: Operator modes

The updater SHALL support status, forced reinstall, download-only, verified local
image installation and controlled no-reboot operation. Local-image installation
SHALL require an expected MD5.

#### Scenario: Local image lacks expected MD5

- GIVEN `--image` is supplied without a valid `--expected-md5`
- WHEN the updater starts
- THEN it exits before entering the OTA guard

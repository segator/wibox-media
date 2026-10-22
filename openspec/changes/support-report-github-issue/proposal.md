# Support report issue link

Add a diagnostic MQTT button that lets an administrator generate a sanitised
WiBox report and open a pre-filled GitHub issue from Home Assistant without
installing a GitHub token on the device.

## Why

Support requests currently require users to find and extract logs manually.
The device already has MQTT/Home Assistant discovery, so a diagnostic button
and a documented issue-link automation provide a low-friction workflow while
keeping credentials out of firmware and MQTT.

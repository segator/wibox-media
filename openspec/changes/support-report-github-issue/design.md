# Design

The daemon discovers a diagnostic MQTT button at
`<base-topic>/support/report/set`. A non-retained `PRESS` command collects a
bounded tail of daemon and firmware-updater logs, redacts lines containing
common secret markers, and publishes a non-retained JSON report at
`<base-topic>/support/report`.

Home Assistant remains responsible for the user-facing GitHub link. The
documented automation URL-encodes the report title and body into GitHub's
`issues/new` query parameters. No GitHub API request or credential is added to
the device.

The report is intentionally bounded because GitHub documents that an
overlong issue URL can fail with HTTP 414. The user reviews the pre-filled
issue before submitting it.

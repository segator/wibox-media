# MQTT/Home Assistant support reports

## ADDED Requirements

### Requirement: Support report button

The daemon SHALL publish a Home Assistant MQTT button discovery entity named
`Create Support Report` whose command is `<base-topic>/support/report/set`.

#### Scenario: Button is discovered

- **WHEN** MQTT discovery is published
- **THEN** the diagnostic button configuration is published with `PRESS` as
  its press payload and the WiBox availability topic

#### Scenario: User requests a report

- **WHEN** a non-retained `PRESS` command is received
- **THEN** the daemon publishes one non-retained JSON report to
  `<base-topic>/support/report`
- **AND** the report contains a bounded, sanitised diagnostic body
- **AND** the report does not require or contain a GitHub credential

#### Scenario: Retained command is replayed

- **WHEN** a retained `PRESS` command is received
- **THEN** the daemon ignores it and does not generate a report

### Requirement: Issue-link workflow

The project SHALL document a Home Assistant automation that URL-encodes the
report into GitHub's pre-filled `issues/new` link and presents it to the user
for review before submission.

#### Scenario: User opens a pre-filled issue

- **WHEN** the user presses the discovered support button and opens the HA
  notification link
- **THEN** GitHub opens a new issue form with the report title and body
  pre-filled
- **AND** the user can review and submit the issue without a GitHub token in
  the WiBox or Home Assistant

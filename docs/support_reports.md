# One-click WiBox support reports

The WiBox exposes a Home Assistant MQTT button named **Create Support Report**.
Pressing it publishes a short, sanitised diagnostic report to:

```text
wibox/<device-id>/support/report
```

The report is not retained and never contains the MQTT password, tokens or
other configuration secrets. The device does not need a GitHub token.

GitHub supports opening a new issue with its title and body pre-filled through
URL query parameters. The following automation turns the MQTT report into a
Home Assistant notification containing that link. Add it to the HA instance
that receives the WiBox MQTT discovery messages, replacing the device topic
with the topic shown by MQTT discovery:

```yaml
automation:
  - id: wibox_support_report_link
    alias: WiBox support report link
    triggers:
      - trigger: mqtt
        topic: wibox/DEVICE_ID/support/report
    actions:
      - action: persistent_notification.create
        data:
          title: WiBox support report
          message: >-
            [Open a pre-filled GitHub issue](https://github.com/segator/wibox-media/issues/new?labels=support&title={{ trigger.payload_json.title | urlencode }}&body={{ trigger.payload_json.body | urlencode }})
```

After pressing the button, open the notification link and submit the issue in
GitHub. This is intentionally a two-step flow (generate, then review/submit):
the user can remove anything private before sending it, and GitHub can reject
URLs that are too long. The report is deliberately capped so the normal case
fits in a browser URL; if a report is still too long, the notification link
should be opened and the body shortened before submitting.

Do not put a GitHub token in the WiBox configuration or in MQTT. A token is
only needed for a fully automatic server-side issue creation flow, which is a
separate, optional HA integration.

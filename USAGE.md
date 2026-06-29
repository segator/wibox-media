# Using Wibox patch

This firmware replaces the old web/script control path with
`wibox-media-daemon`, exposed primarily through SIP and MQTT/Home Assistant.

## MQTT / Home Assistant

MQTT/Home Assistant is handled directly by `wibox-media-daemon` using plain
MQTT on port 1883.

![](./docs/img/homeassistant.png)

Configure MQTT in `/mnt/mtd/sip_media.conf`:

```bash
mqtt_enabled=1
mqtt_host=192.168.10.2
mqtt_user=mqtt
mqtt_pass=password
```

## Keep application working

If you want to keep using Sofia original application,
you can tweak `post-run` script in order to boot it.

Beware that enabling Sofia will disable this patched application to work,
so controls will only work with original Wibox application.

Create file `/mnt/mtd/factory` (`touch`) to disable patch boot.

You can also update or create `/mnt/mtd/post.sh` with **executable permissions** and write:

```bash
#!/bin/sh

# run factory program
/usr/run-orig.sh
```

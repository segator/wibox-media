# Recovery

Use these methods in order.

## 1. Recovery Over SSH

Use this when the custom firmware boots and SSH works.

```bash
make flash-dry-run
make flash CONFIRM_FLASH=YES
reboot
```

## 2. Recovery Via Shell

Use this when Linux boots but networking or startup is broken and you still
have a shell.

```sh
/usr/bin/update_firmware.sh
reboot
```

If the updater is unavailable:

```sh
dd if=/tmp/update.img of=/dev/mtdblock4 bs=4096
sync
reboot
```

## 3. Recovery Via U-Boot

Use this when Linux does not boot far enough for a shell.

See [Serial TTL](serial_ttl.md) for how to enter U-Boot.

From U-Boot:

```sh
mw.b 0xC1000000 ff 00b10000
sf probe
loady 0xC1000000
sf erase 0x00460000 00b10000
sf write 0xC1000000 0x00460000 00b10000
reset
```

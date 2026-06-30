# Serial TTL

Use a USB-to-TTL adapter for console access when telnet is unavailable or when
the device needs recovery.

## Terminal

Use `picocom` or `minicom` at `115200` baud with hardware flow control off:

```bash
picocom -b 115200 /dev/ttyUSB0
```

or:

```bash
minicom -s
```

## Wiring

The WiBox console is `ttySGK2`.

| WiBox board | USB TTL adapter |
|-------------|-----------------|
| GND         | GND             |
| TX          | RX              |
| RX          | TX              |

## U-Boot Entry

Power the board and press Enter immediately. The bootloader wait window is
about one second.

If nothing appears on serial during boot, set:

```sh
setenv consoledev 'ttySGK0'
saveenv
reset
```

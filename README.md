# MeshCore Weather Station Gateway

This is a fork of [MeshCore](https://github.com/meshcore-dev/MeshCore) that turns a small ESP32 board into a bridge between a Fine Offset-protocol weather station and a MeshCore mesh network. It listens for the weather station's own 915MHz radio broadcast directly, with no gateway or hub in between, and makes the readings available over the mesh: as structured telemetry, and as a free-text `!weather` report you can request from a group channel or direct message.

## Scope

This project targets weather stations: sensors reporting temperature, humidity, barometric pressure, wind speed/direction, rainfall, solar irradiance, and UV index. Other Fine Offset-protocol devices, such as soil moisture sensors, lightning detectors, air-quality monitors, and water-leak sensors, are out of scope.

Fine Offset Electronics is the actual manufacturer behind many rebranded weather stations (Ecowitt, Ambient Weather, Froggit, Misol, and others). If your station shows up under one of those brand names, it's very likely compatible.

## How it works

The gateway is a dual-radio device. A Wio-SX1262 LoRa radio runs the actual MeshCore mesh protocol, the same as any other MeshCore node. A CC1101 sub-GHz receiver listens for the weather station's own FSK broadcast (915MHz in the US) and decodes it locally using a ported [rtl_433](https://github.com/merbanan/rtl_433) decoder. There's no Wi-Fi involved, no vendor gateway or cloud account, and nothing gets polled. The node only ever answers requests; it never pushes anything onto the mesh unprompted.

Once running, other nodes can request standard CayenneLPP telemetry (temperature, humidity, pressure) the same way as any MeshCore sensor node. They can also post `!weather` in a joined group channel (each device picks its own default channel, unique to it, on first boot) or send it as a direct message, and get back a full free-text report (temperature, humidity, pressure, wind, rain rate, solar, UV) that isn't limited by CayenneLPP's fixed type vocabulary.

## Hardware

- [Seeed Studio XIAO ESP32-S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- [Seeed Wio-SX1262](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html) LoRa module (stacks directly onto the XIAO, no wiring needed)
- A CC1101 sub-GHz transceiver breakout board
- A Fine Offset-protocol weather station transmitting on 915MHz (or the appropriate ISM band for your region)

### CC1101 wiring

The Wio-SX1262 stacks onto the XIAO directly and uses its own dedicated pins, so all of the XIAO's remaining GPIO (D0-D5) are free for the CC1101.

| CC1101 pin | XIAO pin | GPIO |
|---|---|---|
| SCK  | D0 | GPIO1 |
| MOSI | D1 | GPIO2 |
| MISO | D2 | GPIO3 |
| CSN  | D3 | GPIO4 |
| GDO0 | D4 | GPIO5 |
| GDO2 | D5 | GPIO6 |
| VCC  | 3V3 | n/a |
| GND  | GND | n/a |

## Flashing

The easiest way to get started is the web flasher. No build tools required, just Chrome or Edge.

**[Flash it now](https://mresnick.github.io/meshcore-weatherstation/)** (requires a browser with WebSerial support, and a USB cable)

## Configuring your node

Once flashed, use the [web configurator](https://mresnick.github.io/meshcore-weatherstation/configure/) over USB to set the node's name, the command it responds to, and which group channels it listens on, all without building firmware or using the mesh CLI.

Each device picks its own unique defaults on first boot (a name and a group channel both derived from its chip ID), so multiple freshly-flashed units aren't indistinguishable from each other. Everything you change is saved to flash and survives a reboot or power loss.

The configurator also has a raw serial log with an optional decode-log view (colorized: green for a successful decode, yellow for a capture that failed to decode, cyan for housekeeping) -- useful for judging reception quality without a separate serial terminal.

## Building from source

- Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com)
- Clone this repository and open it in VS Code
- Build/upload the [`Xiao_S3_WIO_weatherstation_sensor`](./variants/xiao_s3_wio_weatherstation/platformio.ini) environment, based on the [weatherstation_sensor](./examples/weatherstation_sensor) example

Key build flags (set in `variants/xiao_s3_wio_weatherstation/platformio.ini`):

| Flag | Purpose |
|---|---|
| `ADVERT_NAME` | Node name advertised on the mesh (leave unset for a per-device default) |
| `ADVERT_LAT` / `ADVERT_LON` | Node location (optional) |
| `ADMIN_PASSWORD` | Password for admin/config access over the mesh CLI |
| `RF_MODULE_FREQUENCY` | CC1101 receive frequency (915.00 for US; adjust for your region) |
| `WEATHER_COMMAND` | First-boot default trigger command (default `!weather`) |
| `FSK_DROPOUT_GATE_US` | How long into a signal before a brief RSSI dip is tolerated instead of ending reception (default 4000; the underlying library's own default is 30000, longer than a full Fine Offset packet) |
| `WEATHERSTATION_AGC_TARGET` | CC1101 AGC target amplitude, one of the `RADIOLIB_CC1101_MAGN_TARGET_*_DB` constants (default 42dB; the chip's own default is 33dB) |

## CLI commands

Telemetry queries are open to anyone (no login required, matching how any MeshCore sensor node works). The commands below are available locally over USB serial without a password, and remotely over the mesh with the admin password:

- `channel join #name`: join a "hashtag channel" (key derived from the name itself)
- `channel join name <psk-base64>`: join a private channel with an explicit key
- `channel leave name`
- `channel list`
- `get trigger`: show the current trigger command
- `set trigger <text>`: set the trigger command
- `set name <text>` / `get name`: set/view the node's name (built into MeshCore's CLI)

---

## About MeshCore

This project is built on [MeshCore](https://github.com/meshcore-dev/MeshCore), a lightweight, portable C++ library for multi-hop LoRa mesh networking, released under the MIT License (see [license.txt](./license.txt)). For everything not specific to this weather-station gateway, general MeshCore documentation, other firmware roles (companion radio, repeater, room server), client apps, and the wider hardware-compatibility list, see the [upstream README](https://github.com/meshcore-dev/MeshCore/blob/dev/README.md), [docs.meshcore.io](https://docs.meshcore.io), and the [MeshCore Discord](https://meshcore.gg).

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

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects using LoRa and other packet radios. It is designed for developers who want to create resilient, decentralized communication networks that work without the internet.

## 🔍 What is MeshCore?

MeshCore now supports a range of LoRa devices, allowing for easy flashing without the need to compile firmware manually. Users can flash a pre-built binary using tools like Adafruit ESPTool and interact with the network through a serial console.
MeshCore provides the ability to create wireless mesh networks, similar to Meshtastic and Reticulum but with a focus on lightweight multi-hop packet routing for embedded projects. Unlike Meshtastic, which is tailored for casual LoRa communication, or Reticulum, which offers advanced networking, MeshCore balances simplicity with scalability, making it ideal for custom embedded solutions, where devices (nodes) can communicate over long distances by relaying messages through intermediate nodes. This is especially useful in off-grid, emergency, or tactical situations where traditional communication infrastructure is unavailable.

## ⚡ Key Features

* Multi-Hop Packet Routing
  * Devices can forward messages across multiple nodes, extending range beyond a single radio's reach.
  * Supports up to a configurable number of hops to balance network efficiency and prevent excessive traffic.
  * Nodes use fixed roles where "Companion" nodes are not repeating messages at all to prevent adverse routing paths from being used.
* Supports LoRa Radios – Works with Heltec, RAK Wireless, and other LoRa-based hardware.
* Decentralized & Resilient – No central server or internet required; the network is self-healing.
* Low Power Consumption – Ideal for battery-powered or solar-powered devices.
* Simple to Deploy – Pre-built example applications make it easy to get started.

## 🎯 What Can You Use MeshCore For?

* Off-Grid Communication: Stay connected even in remote areas.
* Emergency Response & Disaster Recovery: Set up instant networks where infrastructure is down.
* Outdoor Activities: Hiking, camping, and adventure racing communication.
* Tactical & Security Applications: Military, law enforcement, and private security use cases.
* IoT & Sensor Networks: Collect data from remote sensors and relay it back to a central location.

## 🚀 How to Get Started

- Watch the [MeshCore QuickStart Playlist](https://www.youtube.com/watch?v=iaFltojJrAc&list=PLshzThxhw4O4WU_iZo3NmNZOv6KMrUuF9) by The Comms Channel
- Watch the [MeshCore Technical Presentation](https://www.youtube.com/watch?v=OwmkVkZQTf4) by Liam Cottle.
- Read through our [Frequently Asked Questions](./docs/faq.md) and [Documentation](https://docs.meshcore.io).
- Flash the MeshCore firmware on a supported device.
- Connect with a supported client.

For developers:

- Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com).
- Clone and open the MeshCore repository in Visual Studio Code.
- See the example applications you can modify and run:
  - [Companion Radio](./examples/companion_radio) - For use with an external chat app, over BLE, USB or Wi-Fi.
  - [KISS Modem](./examples/kiss_modem) - Serial KISS protocol bridge for host applications. ([protocol docs](./docs/kiss_modem_protocol.md))
  - [Simple Repeater](./examples/simple_repeater) - Extends network coverage by relaying messages.
  - [Simple Room Server](./examples/simple_room_server) - A simple BBS server for shared Posts.
  - [Simple Secure Chat](./examples/simple_secure_chat) - Secure terminal based text communication between devices.
  - [Simple Sensor](./examples/simple_sensor) - Remote sensor node with telemetry and alerting.

The Simple Secure Chat example can be interacted with through the Serial Monitor in Visual Studio Code, or with a Serial USB Terminal on Android.

## ⚡️ MeshCore Flasher

We have prebuilt firmware ready to flash on supported devices.

- Launch https://meshcore.io/flasher
- Select a supported device
- Flash one of the firmware types:
  - Companion, Repeater or Room Server
- Once flashing is complete, you can connect with one of the MeshCore clients below.

## 📱 MeshCore Clients

**Companion Firmware**

The companion firmware can be connected to via BLE, USB or Wi-Fi depending on the firmware type you flashed.

- Web: https://app.meshcore.nz
- Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
- iOS: https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone
- NodeJS: https://github.com/liamcottle/meshcore.js
- Python: https://github.com/fdlamotte/meshcore-cli

**Repeater and Room Server Firmware**

The repeater and room server firmware can be set up via USB in the web config tool.

- https://config.meshcore.io

They can also be managed via LoRa in the mobile app by using the Remote Management feature.

## 🛠 Hardware Compatibility

MeshCore is designed for devices listed in the [MeshCore Flasher](https://meshcore.io/flasher)

## 📜 License

MeshCore is open-source software released under the MIT License. You are free to use, modify, and distribute it for personal and commercial projects.

## Contributing

Please submit PR's using 'dev' as the base branch!
For minor changes just submit your PR and we'll try to review it, but for anything more 'impactful' please open an Issue first and start a discussion. It is better to sound out what it is you want to achieve first, and try to come to a consensus on what the best approach is, especially when it impacts the structure or architecture of this codebase.

Here are some general principles you should try to adhere to:
* Keep it simple. Please, don't think like a high-level lang programmer. Think embedded, and keep code concise, without any unnecessary layers.
* No dynamic memory allocation, except during setup/begin functions.
* Use the same brace and indenting style that's in the core source modules. (A .clang-format is probably going to be added soon, but please do NOT retroactively re-format existing code. This just creates unnecessary diffs that make finding problems harder)

Help us prioritize! Please react with thumbs-up to issues/PRs you care about most. We look at reaction counts when planning work.

### Running unit tests

To run unit tests, run the following command:

```bash
pio test --environment native --verbose
```

## Road-Map / To-Do

There are a number of fairly major features in the pipeline, with no particular time-frames attached yet. In very rough chronological order:
- [X] Companion radio: UI redesign
- [X] Repeater + Room Server: add ACL's (like Sensor Node has)
- [X] Standardise Bridge mode for repeaters
- [ ] Repeater/Bridge: Standardise the Transport Codes for zoning/filtering
- [X] Core + Repeater: enhanced zero-hop neighbour discovery
- [ ] Core: round-trip manual path support
- [ ] Companion + Apps: support for multiple sub-meshes (and 'off-grid' client repeat mode)
- [ ] Core + Apps: support for LZW message compression
- [ ] Core: dynamic CR (Coding Rate) for weak vs strong hops
- [ ] Core: new framework for hosting multiple virtual nodes on one physical device
- [ ] V2 protocol spec: discussion and consensus around V2 packet protocol, including path hashes, new encryption specs, etc

## 📞 Get Support

- Report bugs and request features on the [GitHub Issues](https://github.com/ripplebiz/MeshCore/issues) page.
- Find additional guides and components on [my site](https://buymeacoffee.com/ripplebiz).
- Join [MeshCore Discord](https://meshcore.gg) to chat with the developers and get help from the community.

# LoPy4 Wi-Fi OTA TCP Firmware

A minimal proof-of-concept for **remotely controlling embedded devices** over Wi-Fi.  
Built on top of ESP-IDF for the LoPy4 (ESP32-based), this project offers:

- TCP command interface over Wi-Fi
- LED control (representing any general device control)
- OTA firmware update pipeline via socket
- Internal command bus for clean inter-module communication

---

## Concept

The `led_on` and `led_off` commands don’t just flip a GPIO — they demonstrate **remote control capability**.

This project serves as a skeleton for:
- Smart actuators (relays, motors, locks, etc.)
- Remote device testing and reset triggers
- Configuration of industrial devices over-the-air
- OTA patching of deployed sensor nodes
- Secure remote operations on edge AI systems
- Lightweight command & telemetry for drones or robots

Replace the LED with anything — a water valve, an alarm, a gate opener — the infrastructure stays the same.

---

## TCP Commands

| Command       | Description                            |
|---------------|----------------------------------------|
| `PING`        | Responds with `PONG`                   |
| `AUTH <pwd>`  | Authenticates (hardcoded `hunter2`)    |
| `led_on`      | Sends `CMD_LED_ON` to internal bus     |
| `led_off`     | Sends `CMD_LED_OFF` to internal bus    |
| `OTA <sz> <crc>` | Begins OTA update via TCP socket    |

---

## Architecture Overview

```
Wi-Fi STA Mode
     │
     ▼
 TCP Server (port 8080)
     │
     ├── AUTH → validate password
     ├── OTA  → receive & verify firmware + reboot
     └── LED  → send command to command bus (FreeRTOS Queue)
                     │
                     ▼
                LED Task → GPIO22
```

The `command_bus` can be extended to talk to *any* module or peripheral.

---

## Build + Flash

Requires:
- ESP-IDF v5.x
- LoPy4 board
- Python 3 + required IDF tools

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## OTA Flow

The TCP server supports simple OTA updates:

```bash
# Example OTA binary push:
# <client> → TCP:
#   "OTA <size> <crc32>\n"
#   [binary data stream]
#   [4 bytes CRC]
```

Firmware will only flash if the CRC matches.

---

## Code Modules

| File              | Role                                        |
|-------------------|---------------------------------------------|
| `main.c`          | Initializes Wi-Fi, starts tasks             |
| `tcp_server.c/h`  | Parses TCP commands                         |
| `led.c/h`         | Handles GPIO via FreeRTOS task              |
| `command_bus.c/h` | Internal messaging queue (decouples logic)  |
| `ota_handler.c/h` | Validates and applies OTA update            |

---

## Next Steps (Future Work)

- Add **SSH-based OTA** (secure + encrypted)
- Enable **BLE-based command interface** for offline ops
- Expand `command_bus` with new commands (sensor reads, reboot, etc.)
- Add **configurable GPIO pin mapping** via remote command
- Web dashboard (Maybe, integrating it in range-detector project)

---

<p align="right">
  <img src="./vc.png" alt="V&C Logo" width="20"/>
</p>
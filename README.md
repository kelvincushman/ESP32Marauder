<!---[![License: MIT](https://img.shields.io/github/license/mashape/apistatus.svg)](https://github.com/justcallmekoko/ESP32Marauder/blob/master/LICENSE)--->
<!---[![Gitter](https://badges.gitter.im/justcallmekoko/ESP32Marauder.png)](https://gitter.im/justcallmekoko/ESP32Marauder)--->
<!---[![Build Status](https://travis-ci.com/justcallmekoko/ESP32Marauder.svg?branch=master)](https://travis-ci.com/justcallmekoko/ESP32Marauder)--->
<!---Shields/Badges https://shields.io/--->

# ESP32 Marauder
<p align="center"><img alt="Marauder logo" src="https://github.com/justcallmekoko/ESP32Marauder/blob/master/pictures/marauder_skull_patch_04_full_final.png?raw=true" width="300"></p>
<p align="center">
  <b>A suite of WiFi/Bluetooth offensive and defensive tools for the ESP32</b>
  <br><br>
  <a href="https://github.com/justcallmekoko/ESP32Marauder/blob/master/LICENSE"><img alt="License" src="https://img.shields.io/github/license/mashape/apistatus.svg"></a>
  <a href="https://gitter.im/justcallmekoko/ESP32Marauder"><img alt="Gitter" src="https://badges.gitter.im/justcallmekoko/ESP32Marauder.png"/></a>
  <a href="https://github.com/justcallmekoko/ESP32Marauder/releases/latest"><img src="https://img.shields.io/github/downloads/justcallmekoko/ESP32Marauder/total" alt="Downloads"/></a>
  <br>
  <a href="https://twitter.com/intent/follow?screen_name=jcmkyoutube"><img src="https://img.shields.io/twitter/follow/jcmkyoutube?style=social&logo=twitter" alt="Twitter"></a>
  <a href="https://www.instagram.com/just.call.me.koko"><img src="https://img.shields.io/badge/Follow%20Me-Instagram-orange" alt="Instagram"/></a>
  <br><br>
</p>

[![Build and Push](https://github.com/justcallmekoko/ESP32Marauder/actions/workflows/build_push.yml/badge.svg)](https://github.com/justcallmekoko/ESP32Marauder/actions/workflows/build_push.yml)

---

## Multi-Protocol Pentest Expansion

This fork extends ESP32 Marauder with additional penetration testing capabilities:

### New Modules

| Module | Hardware | Features |
|--------|----------|----------|
| **IR Interface** | IR LED + VS1838B | Capture, replay, TV-B-Gone, protocol decode |
| **Sub-GHz Radio** | CC1101 | 315/433/868/915 MHz, signal capture/replay |
| **RFID/NFC** | RC522/PN532 | Card reading, security assessment, key testing |

### Features

- **Saved Connections** - Persistent storage for all captured data (WiFi, BT, IR, Sub-GHz, RFID)
- **Matrix Theme** - Retro green hacker aesthetic with animated boot screen
- **Touch UI** - Full integration with existing Marauder menu system
- **Session Management** - Settings, history, favorites, and notes

### New Files

```
esp32_marauder/
├── IRInterface.h/cpp        # IR capture and replay
├── SubGHzInterface.h/cpp    # CC1101 radio control
├── RFIDInterface.h/cpp      # RFID/NFC security auditing
├── PentestModule.h/cpp      # Integration middleware
├── SavedConnections.h/cpp   # Persistent data storage
├── theme_matrix.h           # Matrix green theme
└── configs_pentest.h        # Hardware pin definitions
```

### Documentation

- [EXPANSION_PLAN.md](EXPANSION_PLAN.md) - Hardware requirements, wiring diagrams, BOM
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Step-by-step integration instructions

### Hardware Requirements

| Component | Purpose | Approx Cost |
|-----------|---------|-------------|
| ESP32-WROOM | Main MCU | $5-8 |
| ILI9341 2.8" TFT | Display + Touch | $8-12 |
| CC1101 Module | Sub-GHz radio | $3-5 |
| RC522 Module | RFID reader | $2-3 |
| IR LED + VS1838B | IR TX/RX | $1-2 |

**Total additional cost: ~$15-25**

---

## Getting Started
Download the [latest release](https://github.com/justcallmekoko/ESP32Marauder/releases/latest) of the firmware.

Check out the project [wiki](https://github.com/justcallmekoko/ESP32Marauder/wiki) for a full overview of the ESP32 Marauder

# For Sale Now
You can buy the ESP32 Marauder using [this link](https://www.justcallmekokollc.com)

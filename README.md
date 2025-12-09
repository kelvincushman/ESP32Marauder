# Marauder Neo - Multi-Protocol Pentest Platform

<p align="center">
  <img alt="Marauder Neo" src="https://github.com/justcallmekoko/ESP32Marauder/blob/master/pictures/marauder_skull_patch_04_full_final.png?raw=true" width="250">
</p>

<p align="center">
  <b>ESP32 Marauder + IR + Sub-GHz + RFID + NFC + LoRa</b>
  <br>
  <i>A comprehensive multi-protocol penetration testing platform</i>
</p>

<p align="center">
  <img alt="License" src="https://img.shields.io/github/license/mashape/apistatus.svg">
  <img alt="Platform" src="https://img.shields.io/badge/platform-ESP32-blue">
  <img alt="Protocols" src="https://img.shields.io/badge/protocols-7+-green">
</p>

---

## Overview

**Marauder Neo** extends the original ESP32 Marauder from a WiFi/Bluetooth security tool into a full multi-protocol penetration testing platform, similar to the Flipper Zero but on ESP32 hardware.

### Supported Protocols

| Protocol | Frequency | Hardware | Features |
|----------|-----------|----------|----------|
| **WiFi** | 2.4 GHz | ESP32 Built-in | Scan, deauth, beacon, probe, PMKID |
| **Bluetooth** | 2.4 GHz | ESP32 Built-in | BLE scan, skimmer detect, sour apple |
| **Infrared** | 38-56 kHz | IR LED + VS1838B | Capture, replay, TV-B-Gone, protocol decode |
| **Sub-GHz** | 300-928 MHz | CC1101 | Signal capture/replay, frequency analyzer |
| **RFID** | 13.56 MHz | RC522 | MIFARE read/write, key testing |
| **NFC/EMV** | 13.56 MHz | PN532 | EMV cards, APDU, FeliCa, card emulation |
| **LoRa** | 433/868/915 MHz | SX1276/SX1278 | Packet sniff, Meshtastic, LoRaWAN, pager |

---

## Features

### Core Capabilities

- **Multi-Protocol Scanning** - Unified interface for all radio protocols
- **Signal Capture & Replay** - Record and retransmit for authorized testing
- **Persistent Storage** - Save captured data to SD card (JSON format)
- **Session Management** - Favorites, notes, history, and targets
- **Matrix Theme** - Retro green hacker aesthetic with animated boot

### IR Module
- Learn and replay IR signals from any remote
- Protocol decode (NEC, Sony, RC5, RC6, Samsung, LG, and 100+ more)
- TV-B-Gone mode (educational demonstration)
- Save signals to SD card for later use

### Sub-GHz Module (CC1101)
- Frequency range: 300-928 MHz (433, 868, 915 MHz bands)
- Raw signal capture and replay
- Frequency analyzer/scanner
- Rolling code detection (warning only)
- Support for garage doors, car remotes, weather sensors, TPMS

### RFID Module (RC522)
- Read MIFARE Classic 1K/4K cards
- UID extraction and display
- Default key testing
- Sector dump and save
- Basic security assessment

### NFC/EMV Module (PN532)
- **ISO 14443A/B** - All common NFC cards
- **EMV Card Reading** - Bank card metadata (PAN masked, expiry, app label)
- **APDU Shell** - Send custom ISO 7816 commands
- **FeliCa Support** - Japanese transit cards (Suica, PASMO)
- **Card Emulation** - Limited UID emulation
- **Full Dump** - Export all readable data

### LoRa Module (SX1276/SX1278)
Inspired by the [Hacker Pager](https://shop.exploitee.rs/shop/p/the-hacker-pager):
- **Packet Sniffing** - Capture LoRa packets to PCAP
- **Meshtastic Decode** - Parse mesh network traffic
- **LoRaWAN Analysis** - Decode join requests and data frames
- **Spectrum Scan** - Find active channels
- **Pager Mode** - Meshtastic-compatible messaging
- **Beacon Mode** - Broadcast test packets

### Saved Connections
All captured data persists across reboots:
- WiFi networks (SSID, BSSID, channel, encryption, handshakes)
- Bluetooth devices (name, MAC, class, skimmer flags)
- IR signals (protocol, code, raw timing)
- Sub-GHz signals (frequency, modulation, raw data)
- RFID cards (UID, type, keys, sector dumps)
- NFC cards (UID, EMV data, FeliCa info)
- LoRa packets (raw data, decoded protocol)

---

## Hardware Requirements

### Base Platform
| Component | Description | Approx Cost |
|-----------|-------------|-------------|
| ESP32-WROOM-32 | Main microcontroller | $5-8 |
| ILI9341 2.8" TFT | Display with touch + SD slot | $8-12 |

### Expansion Modules (Choose what you need)
| Component | Purpose | Approx Cost |
|-----------|---------|-------------|
| IR LED (940nm) + VS1838B | Infrared TX/RX | $1-2 |
| CC1101 Module | Sub-GHz radio (433/868/915) | $3-5 |
| RC522 Module | Basic RFID (MIFARE) | $2-3 |
| **PN532 Module** | **Advanced NFC (EMV/APDU)** | **$5-8** |
| SX1276/SX1278 Module | LoRa radio | $4-8 |

**Choose RC522 OR PN532** - PN532 recommended for EMV research

**Total Cost:** $25-45 depending on modules selected

---

## Wiring Diagram

```
ESP32-WROOM Pinout:

                    ┌─────────────────┐
                    │   ESP32-WROOM   │
                    │                 │
    IR TX LED ──────│ GPIO 16         │
    IR RX (TSOP) ───│ GPIO 17         │
                    │                 │
    CC1101 CS ──────│ GPIO 27         │
    CC1101 GDO0 ────│ GPIO 25         │
    CC1101 GDO2 ────│ GPIO 26         │
                    │                 │
    RFID CS ────────│ GPIO 13         │  (RC522)
    RFID RST ───────│ GPIO 12         │  (RC522)
                    │                 │
    PN532 SDA ──────│ GPIO 22         │  (PN532 - I2C)
    PN532 SCL ──────│ GPIO 14         │  (PN532 - I2C)
    PN532 IRQ ──────│ GPIO 36         │  (PN532 - I2C)
                    │                 │
    LoRa CS ────────│ GPIO 33         │
    LoRa RST ───────│ GPIO 32         │
    LoRa IRQ ───────│ GPIO 35         │
                    │                 │
    TFT CS ─────────│ GPIO 5          │
    TFT DC ─────────│ GPIO 2          │
    TFT RST ────────│ GPIO 15         │
    SD CS ──────────│ GPIO 4          │
                    │                 │
    [Shared SPI]    │                 │
    MOSI ───────────│ GPIO 23         │
    MISO ───────────│ GPIO 19         │
    SCK ────────────│ GPIO 18         │
                    └─────────────────┘
```

### Pin Summary

| Module | Interface | Pins Used |
|--------|-----------|-----------|
| Display (ILI9341) | SPI | 5 (CS), 2 (DC), 15 (RST) |
| SD Card | SPI | 4 (CS) |
| Infrared | GPIO | 16 (TX), 17 (RX) |
| CC1101 | SPI | 27 (CS), 25 (GDO0), 26 (GDO2) |
| RC522 | SPI | 13 (CS), 12 (RST) |
| PN532 | I2C | 22 (SDA), 14 (SCL), 36 (IRQ) |
| LoRa | SPI | 33 (CS), 32 (RST), 35 (IRQ) |
| Shared SPI | - | 23 (MOSI), 19 (MISO), 18 (SCK) |

---

## Installation

### Prerequisites
- Arduino IDE or PlatformIO
- ESP32 board support package
- Required libraries (see below)

### Libraries Required
```
- TFT_eSPI (display)
- IRremoteESP8266 (IR)
- ELECHOUSE_CC1101_SRC_DRV (Sub-GHz)
- MFRC522 (RFID)
- Adafruit_PN532 (NFC)
- LoRa (SX127x)
- ArduinoJson (data storage)
- LinkedList (data structures)
```

### Configuration

1. In `esp32_marauder/configs.h`, add:
```cpp
#define MARAUDER_PENTEST
```

2. In `esp32_marauder/configs_pentest.h`, enable your modules:
```cpp
#define HAS_IR        // Infrared
#define HAS_SUBGHZ    // CC1101
#define HAS_RFID      // RC522 (basic RFID)
//#define HAS_PN532   // PN532 (advanced NFC) - uncomment if using
#define HAS_LORA      // LoRa radio
```

3. For Matrix theme, add in configs.h:
```cpp
#define MATRIX_THEME
```

### Building
```bash
# Arduino IDE
1. Open esp32_marauder/esp32_marauder.ino
2. Select board: ESP32 Dev Module
3. Upload

# PlatformIO
pio run -e esp32dev -t upload
```

---

## Project Structure

```
ESP32Marauder/
├── esp32_marauder/
│   ├── esp32_marauder.ino    # Main sketch
│   ├── configs.h             # Board configuration
│   ├── configs_pentest.h     # Pentest module pins
│   │
│   ├── # Core Marauder Files
│   ├── WiFiScan.h/cpp        # WiFi operations
│   ├── BLEScan.h/cpp         # Bluetooth scanning
│   ├── MenuFunctions.h/cpp   # UI system
│   ├── Display.h/cpp         # TFT display driver
│   │
│   ├── # New Pentest Modules
│   ├── IRInterface.h/cpp         # IR capture/replay
│   ├── SubGHzInterface.h/cpp     # CC1101 radio
│   ├── RFIDInterface.h/cpp       # RC522 RFID
│   ├── NFCInterface.h/cpp        # PN532 NFC/EMV
│   ├── LoRaInterface.h/cpp       # LoRa radio
│   ├── PentestModule.h/cpp       # Integration layer
│   ├── SavedConnections.h/cpp    # Persistent storage
│   └── theme_matrix.h            # Matrix green theme
│
├── EXPANSION_PLAN.md         # Hardware details & BOM
├── INTEGRATION_GUIDE.md      # Integration instructions
└── README.md                 # This file
```

---

## Menu Structure

```
Main Menu
├── WiFi                    # Original Marauder WiFi tools
├── Bluetooth               # Original Marauder BT tools
├── IR Tools                # NEW
│   ├── Learn Signal
│   ├── Replay Signal
│   ├── TV-B-Gone
│   └── Saved Signals
├── Sub-GHz                 # NEW
│   ├── Frequency Analyzer
│   ├── Capture Signal
│   ├── Replay Signal
│   └── Saved Signals
├── RFID Tools              # NEW (RC522)
│   ├── Read Card
│   ├── Card Info
│   ├── Test Keys
│   └── Saved Cards
├── NFC Tools               # NEW (PN532)
│   ├── Read Card
│   ├── EMV Info
│   ├── APDU Shell
│   ├── FeliCa Read
│   └── Saved Cards
├── LoRa Tools              # NEW
│   ├── Packet Sniffer
│   ├── Spectrum Scan
│   ├── Meshtastic Mode
│   ├── Pager Mode
│   └── Saved Packets
├── Saved Items             # NEW
│   ├── WiFi Networks
│   ├── BT Devices
│   ├── IR Signals
│   ├── Sub-GHz Signals
│   ├── RFID Cards
│   ├── NFC Cards
│   └── LoRa Packets
├── Device                  # Settings
└── Reboot
```

---

## EMV Card Protocol (Educational)

Bank cards use ISO 14443-4 with EMV contactless:

```
┌─────────────────────────────┐
│  Application (Visa/MC)      │  Payment app
├─────────────────────────────┤
│  EMV Contactless Kernel     │  Transaction processing
├─────────────────────────────┤
│  ISO 7816-4 APDU            │  Commands (SELECT, READ)
├─────────────────────────────┤
│  ISO 14443-4                │  Transport layer
├─────────────────────────────┤
│  ISO 14443-3A               │  Anti-collision
├─────────────────────────────┤
│  13.56 MHz RF               │  Physical layer
└─────────────────────────────┘
```

**Readable Data:**
- Application Label (e.g., "VISA CREDIT")
- PAN (masked for privacy)
- Expiration Date
- Application ID (AID)

**NOT Readable/Clonable:**
- CVV/CVC (never on NFC)
- PIN (never transmitted)
- Cryptographic keys
- Dynamic cryptograms (change each transaction)

---

## Legal & Ethical Use

**This tool is for authorized security testing and educational purposes only.**

### Permitted Uses
- Testing your own devices
- Authorized penetration testing (with written permission)
- Security research and education
- CTF competitions

### Prohibited Uses
- Unauthorized access to systems or buildings
- Jamming radio frequencies (FCC violations)
- Intercepting others' communications
- Cloning access cards without permission
- Any illegal activity

**You are responsible for complying with all applicable laws.**

---

## Credits

- **Original ESP32 Marauder** - [justcallmekoko](https://github.com/justcallmekoko/ESP32Marauder)
- **Flipper Zero** - Inspiration for multi-protocol design
- **Hacker Pager** - LoRa/Meshtastic inspiration
- **IRremoteESP8266** - IR protocol library
- **RadioLib** - Radio abstraction library

---

## Documentation

- [EXPANSION_PLAN.md](EXPANSION_PLAN.md) - Complete hardware guide with wiring diagrams
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Step-by-step integration instructions
- [Original Marauder Wiki](https://github.com/justcallmekoko/ESP32Marauder/wiki) - Base firmware documentation

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

<p align="center">
  <b>Marauder Neo</b> - Multi-Protocol Penetration Testing Platform
  <br>
  <i>For authorized security testing and education only</i>
</p>

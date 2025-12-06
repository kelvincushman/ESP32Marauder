# ESP32 Marauder Expansion Plan
## Building a Multi-Protocol Penetration Testing Device

### Overview

This document outlines how to expand ESP32 Marauder from a WiFi/Bluetooth pentesting tool into a multi-protocol device similar to the Flipper Zero, adding IR, Sub-GHz (433/866MHz), and other capabilities.

---

## Current Hardware Setup

**Your Configuration:**
- **MCU:** ESP32-WROOM-32
- **Display:** ILI9341 2.8" SPI with integrated SD card slot
- **Interface:** Touchscreen

**Current Pin Usage (after display + SD wiring):**

| Function | ESP32 GPIO | Notes |
|----------|------------|-------|
| TFT_MOSI | GPIO 23 | SPI shared |
| TFT_MISO | GPIO 19 | SPI shared |
| TFT_SCLK | GPIO 18 | SPI shared |
| TFT_CS | GPIO 5 | Display chip select |
| TFT_DC | GPIO 2 | Data/Command |
| TFT_RST | GPIO 15 | Reset |
| SD_CS | GPIO 4 | SD card chip select |
| Touch_CS | GPIO 21 | Touch chip select (optional) |

---

## Available GPIO Pins for Expansion

After display and SD wiring, these GPIOs are available:

### Freely Available (Recommended for Expansion):
| GPIO | Best Use | Notes |
|------|----------|-------|
| **GPIO 16** | IR TX | Good for output |
| **GPIO 17** | IR RX | Good for input |
| **GPIO 25** | CC1101 GDO0 | DAC capable, interrupt |
| **GPIO 26** | CC1101 GDO2 | DAC capable, interrupt |
| **GPIO 27** | CC1101 CS | SPI chip select |
| **GPIO 32** | Analog input | ADC1, battery monitoring |
| **GPIO 33** | NeoPixel LED | PWM capable |
| **GPIO 12** | RFID RST | Strapping pin (boot) |
| **GPIO 13** | RFID CS | SPI chip select |
| **GPIO 14** | Alternative SPI CLK | If needed |

### Input-Only (ADC readings, buttons):
| GPIO | Use |
|------|-----|
| GPIO 34 | Button / Analog input |
| GPIO 35 | Button / Analog input |
| GPIO 36 (VP) | Button / Analog input |
| GPIO 39 (VN) | Button / Analog input |

### Reserved/Avoid:
- GPIO 0, 1, 3: Boot/Serial
- GPIO 6-11: Internal flash (do not use)

---

## Expansion Modules

### Module 1: Infrared (IR) Transceiver

**Purpose:** Capture and replay IR signals from TVs, AC units, gates, etc.

**Hardware Required:**
- IR LED (940nm) - TX
- TSOP38238 or VS1838B - RX
- 100-220 ohm resistor for LED
- Optional: 2N2222 transistor for higher power TX

**Wiring:**
```
IR TX LED → GPIO 16 (through resistor/transistor)
IR RX (TSOP) → GPIO 17
VCC → 3.3V
GND → GND
```

**Library:** `IRremoteESP8266` (widely used, supports 100+ protocols)

**Features to Implement:**
1. **IR Learn Mode** - Capture raw signals
2. **IR Replay** - Transmit captured signals
3. **Protocol Decode** - NEC, Sony, RC5, RC6, Samsung, LG, etc.
4. **Signal Storage** - Save to SD card (JSON format)
5. **Universal Remote** - Pre-built codes for common devices
6. **Brute Force** - Cycle through common codes (educational)

**Scan Mode Constants to Add:**
```cpp
#define IR_SCAN_LEARN    80
#define IR_ATTACK_REPLAY 81
#define IR_ATTACK_BRUTE  82
```

---

### Module 2: Sub-GHz Radio (433/866/915 MHz)

**Purpose:** Capture and replay RF signals for garage doors, car keys, doorbells, weather stations, etc.

**Hardware Required:**
- **CC1101** module (most versatile, supports 300-928 MHz)
- Alternative: SX1278 LoRa module (longer range, 433/868/915 MHz)

**CC1101 Wiring (SPI):**
```
CC1101 Pin → ESP32 GPIO
MOSI       → GPIO 23 (shared with display SPI)
MISO       → GPIO 19 (shared with display SPI)
SCK        → GPIO 18 (shared with display SPI)
CS         → GPIO 27 (dedicated)
GDO0       → GPIO 25 (interrupt/data)
GDO2       → GPIO 26 (interrupt/status)
VCC        → 3.3V
GND        → GND
```

**Library:** `SmartRC-CC1101-Driver-Lib` or `RadioLib`

**Features to Implement:**
1. **Raw Signal Capture** - Record OOK/ASK modulated signals
2. **Signal Replay** - Transmit captured signals
3. **Frequency Scanner** - Detect active transmissions
4. **Protocol Decode:**
   - Rolling codes detection (warning only - can't replay)
   - Static codes (garage doors, older remotes)
   - Weather station protocols
   - Car TPMS sensors
5. **Signal Analysis** - Show timing, modulation type
6. **Jamming Detection** - Monitor for interference (passive)

**Supported Frequencies:**
| Region | Common Frequencies | Use Cases |
|--------|-------------------|-----------|
| EU | 433.92 MHz, 868 MHz | Garage doors, remotes, sensors |
| US | 315 MHz, 433.92 MHz, 915 MHz | Car remotes, LoRa |
| Global | 433.92 MHz | Universal ISM band |

**Scan Mode Constants:**
```cpp
#define SUBGHZ_SCAN_RAW     83
#define SUBGHZ_SCAN_HOPPING 84
#define SUBGHZ_ATTACK_REPLAY 85
#define SUBGHZ_ANALYZER     86
```

---

### Module 3: RFID/NFC (13.56 MHz)

**Purpose:** Read/write RFID cards (building access, hotel keys, etc.)

**Hardware Required:**
- **RC522** module (most common, cheap)
- Alternative: **PN532** (supports more protocols, NFC phones)

**RC522 Wiring (SPI):**
```
RC522 Pin → ESP32 GPIO
MOSI      → GPIO 23 (shared SPI)
MISO      → GPIO 19 (shared SPI)
SCK       → GPIO 18 (shared SPI)
SDA/CS    → GPIO 13 (dedicated)
RST       → GPIO 12
VCC       → 3.3V
GND       → GND
```

**Library:** `MFRC522` or `PN532` depending on module

**Features to Implement:**
1. **Card Reader** - Read UID and sector data
2. **Card Cloner** - Write UID to writable cards (UID changeable)
3. **Brute Force** - Default key attacks on MIFARE Classic
4. **Card Emulation** - Emulate cards (PN532 only)
5. **Card Storage** - Save card data to SD

**Supported Card Types:**
- MIFARE Classic 1K/4K
- MIFARE Ultralight
- NTAG213/215/216
- ISO 14443A/B

**Scan Mode Constants:**
```cpp
#define RFID_SCAN_READ   87
#define RFID_ATTACK_CLONE 88
#define RFID_ATTACK_BRUTE 89
```

---

### Module 4: 125 kHz RFID (Low Frequency)

**Purpose:** Clone older access cards (HID, EM4100, etc.)

**Hardware Required:**
- **RDM6300** module (read only)
- Or custom coil with analog reading

**Wiring (UART):**
```
RDM6300 → ESP32
TX      → GPIO 16 (conflicts with IR - choose one)
VCC     → 5V
GND     → GND
```

**Features:**
1. **Card Reader** - Read EM4100, HID ProxCard
2. **Card Logger** - Store read cards

**Note:** Write capability requires specialized hardware (Proxmark3-style)

---

### Module 5: GPS/Wardriving Enhancement

**Already Supported!** Just add hardware:

**Hardware:** NEO-6M, NEO-7M, or NEO-8M GPS module

**Wiring (UART):**
```
GPS TX → GPIO 16 (RX2)
GPS RX → GPIO 17 (TX2)
VCC    → 3.3V
GND    → GND
```

**Existing Features:**
- GPS coordinate logging
- Wardriving mode (correlate WiFi with location)
- GPX export for mapping

---

## Implementation Architecture

### New Files to Create:

```
esp32_marauder/
├── IRInterface.h          # IR module header
├── IRInterface.cpp        # IR implementation
├── SubGHzInterface.h      # CC1101/radio header
├── SubGHzInterface.cpp    # Radio implementation
├── RFIDInterface.h        # RFID/NFC header
├── RFIDInterface.cpp      # RFID implementation
└── protocols/             # Protocol decoders
    ├── ir_protocols.h
    ├── subghz_protocols.h
    └── rfid_protocols.h
```

### configs.h Additions:

```cpp
// New hardware feature flags
#define HAS_IR
#define HAS_SUBGHZ
#define HAS_RFID
#define HAS_LF_RFID

// IR Pin definitions
#define IR_TX_PIN 16
#define IR_RX_PIN 17

// CC1101 Pin definitions
#define CC1101_CS_PIN   27
#define CC1101_GDO0_PIN 25
#define CC1101_GDO2_PIN 26

// RFID Pin definitions
#define RFID_CS_PIN  13
#define RFID_RST_PIN 12
```

### Menu Structure Addition:

```
mainMenu
├── WiFi (existing)
├── Bluetooth (existing)
├── IR Tools (NEW)
│   ├── Learn Signal
│   ├── Replay Signal
│   ├── Saved Signals
│   └── Universal Remote
├── Sub-GHz (NEW)
│   ├── Frequency Analyzer
│   ├── Capture Signal
│   ├── Replay Signal
│   ├── Saved Signals
│   └── Read Raw
├── RFID (NEW)
│   ├── Read Card
│   ├── Card Info
│   ├── Clone Card
│   ├── Saved Cards
│   └── Brute Force Keys
├── GPS (existing)
└── Device (existing)
```

---

## Recommended Implementation Order

### Phase 1: IR Module (Easiest - 1-2 days work)
- Minimal hardware ($2-3)
- Well-documented libraries
- Immediate practical use
- Good learning project

### Phase 2: Sub-GHz Radio (Medium - 3-5 days work)
- CC1101 module ($5-8)
- More complex but powerful
- Many practical applications
- Requires RF knowledge

### Phase 3: RFID/NFC (Medium - 2-3 days work)
- RC522 module ($3-5)
- Common use case
- Ethical considerations important

### Phase 4: GPS Enhancement (Easy - already supported)
- Just wire up the GPS module
- Configuration only

---

## Complete Wiring Diagram

```
ESP32-WROOM Pinout for Multi-Protocol Build:

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
    RFID CS ────────│ GPIO 13         │
    RFID RST ───────│ GPIO 12         │
                    │                 │
    TFT CS ─────────│ GPIO 5          │
    TFT DC ─────────│ GPIO 2          │
    TFT RST ────────│ GPIO 15         │
                    │                 │
    SD CS ──────────│ GPIO 4          │
                    │                 │
    [Shared SPI Bus]│                 │
    MOSI ───────────│ GPIO 23         │──── TFT/SD/CC1101/RFID
    MISO ───────────│ GPIO 19         │──── TFT/SD/CC1101/RFID
    SCK ────────────│ GPIO 18         │──── TFT/SD/CC1101/RFID
                    │                 │
    LED (NeoPixel) ─│ GPIO 33         │
    Battery ADC ────│ GPIO 32         │
                    │                 │
    Touch CS ───────│ GPIO 21 (opt)   │
                    └─────────────────┘
```

---

## Bill of Materials (BOM)

| Component | Purpose | Price (approx) | Link |
|-----------|---------|----------------|------|
| IR LED 940nm | IR Transmit | $0.50 | AliExpress |
| TSOP38238 | IR Receive | $0.50 | AliExpress |
| CC1101 Module | Sub-GHz Radio | $5-8 | AliExpress |
| RC522 Module | RFID 13.56MHz | $2-4 | AliExpress |
| NEO-6M GPS | Location | $5-8 | AliExpress |
| Resistors (100-220 ohm) | IR circuit | $0.10 | - |
| Jumper wires | Connections | $2 | - |
| **Total** | | **~$15-25** | |

---

## Ethical & Legal Considerations

**IMPORTANT:** These tools should only be used:
- On devices you own
- With explicit permission from device owners
- For authorized security testing
- For educational purposes

**Illegal to use for:**
- Unauthorized access to buildings/systems
- Jamming radio frequencies (FCC violations)
- Intercepting others' communications
- Cloning access cards without permission

---

## Code Implementation Template

Here's the pattern for adding a new module (IR example):

### IRInterface.h
```cpp
#ifndef IRInterface_h
#define IRInterface_h

#include "configs.h"
#ifdef HAS_IR

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

class IRInterface {
private:
    IRrecv* irrecv;
    IRsend* irsend;
    decode_results results;
    bool capturing;

public:
    void RunSetup();
    void main(uint32_t currentTime);

    void startCapture();
    void stopCapture();
    bool hasSignal();
    String getLastSignal();
    void transmitRaw(uint16_t* data, uint16_t len);
    void transmitNEC(uint32_t code);
    void saveToSD(String filename);
    void loadFromSD(String filename);
};

extern IRInterface ir_obj;

#endif
#endif
```

### Adding to WiFiScan.h (scan modes):
```cpp
// IR Modes
#define IR_SCAN_LEARN    80
#define IR_ATTACK_REPLAY 81
#define IR_ATTACK_BRUTE  82
```

### Adding to MenuFunctions.cpp:
```cpp
#ifdef HAS_IR
Menu irMenu;

// In RunSetup():
this->addNodes(&mainMenu, "IR Tools", TFTRED, NULL, IR_ICON, [this]() {
    this->changeMenu(&irMenu, true);
});

this->addNodes(&irMenu, "Learn Signal", TFTCYAN, NULL, IR_SCAN_LEARN, [this]() {
    wifi_scan_obj.StartScan(IR_SCAN_LEARN, TFTCYAN);
});

this->addNodes(&irMenu, "Replay Signal", TFTORANGE, NULL, IR_ATTACK_REPLAY, [this]() {
    wifi_scan_obj.StartScan(IR_ATTACK_REPLAY, TFTORANGE);
});
#endif
```

---

## Next Steps

1. **Order Hardware** - Get IR components first (cheapest, easiest test)
2. **Create Board Definition** - New `MARAUDER_CUSTOM` section in configs.h
3. **Implement IR Module** - Start with capture/replay
4. **Test & Iterate** - Verify on real devices
5. **Add Sub-GHz** - Once IR works, move to CC1101
6. **Add RFID** - Final module

Would you like me to:
1. Create the actual implementation files for IR?
2. Create a custom board configuration for your ESP32-WROOM setup?
3. Detail any specific module further?

---

## References

- [IRremoteESP8266 Library](https://github.com/crankyoldgit/IRremoteESP8266)
- [SmartRC-CC1101 Library](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib)
- [RadioLib](https://github.com/jgromes/RadioLib)
- [MFRC522 Library](https://github.com/miguelbalboa/rfid)
- [Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware) (reference)

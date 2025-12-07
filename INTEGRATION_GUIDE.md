# ESP32 Marauder Pentest Module Integration Guide

This guide shows exactly what changes to make to the existing Marauder codebase to integrate the new pentest modules (IR, Sub-GHz, RFID).

## Files Overview

### New Files Created (ready to use):
- `RFIDInterface.h` / `RFIDInterface.cpp` - RFID security module
- `IRInterface.h` / `IRInterface.cpp` - IR capture/replay module
- `SubGHzInterface.h` / `SubGHzInterface.cpp` - 433/868MHz radio module
- `PentestModule.h` / `PentestModule.cpp` - Integration middleware
- `SavedConnections.h` / `SavedConnections.cpp` - Persistent storage for all connection data
- `theme_matrix.h` - Matrix green hacker theme
- `configs_pentest.h` - Board configuration for pentest build

### Existing Files to Modify:
1. `configs.h` - Add feature flags and board definition
2. `esp32_marauder.ino` - Add module initialization
3. `MenuFunctions.cpp` - Add menu entries
4. `MenuFunctions.h` - Add menu declarations
5. `WiFiScan.cpp` - Add scan mode handlers
6. `WiFiScan.h` - Add scan mode constants
7. `Assets.h` - Add new icons

---

## Step 1: Modify configs.h

Add at line ~33 (after other board defines):

```cpp
//#define MARAUDER_PENTEST    // Custom pentest build
```

Add at line ~400 (in board features section):

```cpp
#ifdef MARAUDER_PENTEST
  #include "configs_pentest.h"
#endif
```

---

## Step 2: Modify esp32_marauder.ino

### Add includes (after line ~50):

```cpp
// Pentest Modules
#if defined(HAS_IR) || defined(HAS_SUBGHZ) || defined(HAS_RFID)
  #include "PentestModule.h"
#endif

#ifdef HAS_IR
  #include "IRInterface.h"
  IRInterface ir_obj;
#endif

#ifdef HAS_SUBGHZ
  #include "SubGHzInterface.h"
  SubGHzInterface subghz_obj;
#endif

#ifdef HAS_RFID
  #include "RFIDInterface.h"
  RFIDInterface rfid_obj;
#endif
```

### Add to setup() function (after display init, around line ~150):

```cpp
  // Initialize pentest modules
  #if defined(HAS_IR) || defined(HAS_SUBGHZ) || defined(HAS_RFID)
    pentest_obj.RunSetup();

    #ifdef MATRIX_THEME
      pentest_obj.drawMatrixBoot();
      delay(1000);
    #endif
  #endif
```

### Add to loop() function (in the main loop, around line ~200):

```cpp
  // Run pentest module updates
  #if defined(HAS_IR) || defined(HAS_SUBGHZ) || defined(HAS_RFID)
    pentest_obj.main(currentTime);
  #endif
```

---

## Step 3: Modify WiFiScan.h

### Add scan mode constants (after line ~146, before existing defines end):

```cpp
// ============================================================================
// PENTEST MODULE SCAN MODES
// ============================================================================

// RFID Modes (80-99)
#define RFID_SCAN_OFF           0
#define RFID_SCAN_READ          80
#define RFID_SCAN_FULL_DUMP     81
#define RFID_SCAN_KEY_TEST      82
#define RFID_SCAN_CONTINUOUS    83
#define RFID_SCAN_CLONE         84
#define RFID_SCAN_WRITE         85
#define RFID_SCAN_ANALYZE       89

// IR Modes (100-119)
#define IR_SCAN_OFF             0
#define IR_SCAN_RECEIVE         100
#define IR_SCAN_ANALYZE         101
#define IR_ATTACK_REPLAY        102
#define IR_ATTACK_BRUTE         103
#define IR_SCAN_CONTINUOUS      105

// Sub-GHz Modes (120-139)
#define SUBGHZ_SCAN_OFF         0
#define SUBGHZ_SCAN_RAW         120
#define SUBGHZ_SCAN_ANALYZE     121
#define SUBGHZ_SCAN_HOPPING     122
#define SUBGHZ_ATTACK_REPLAY    123
#define SUBGHZ_ATTACK_BRUTE     124
#define SUBGHZ_SCAN_RSSI        126
```

---

## Step 4: Modify MenuFunctions.h

### Add menu declarations (around line ~165, with other menu declarations):

```cpp
    // Pentest module menus
    #ifdef HAS_IR
      Menu irMenu;
      Menu irCaptureMenu;
    #endif

    #ifdef HAS_SUBGHZ
      Menu subghzMenu;
      Menu subghzCaptureMenu;
    #endif

    #ifdef HAS_RFID
      Menu rfidMenu;
      Menu rfidScanMenu;
    #endif
```

---

## Step 5: Modify MenuFunctions.cpp

### Add includes at top of file:

```cpp
#if defined(HAS_IR) || defined(HAS_SUBGHZ) || defined(HAS_RFID)
  #include "PentestModule.h"
#endif

#ifdef HAS_IR
  extern IRInterface ir_obj;
#endif

#ifdef HAS_SUBGHZ
  extern SubGHzInterface subghz_obj;
#endif

#ifdef HAS_RFID
  extern RFIDInterface rfid_obj;
#endif
```

### Add menu initialization in RunSetup() (around line ~1400, after existing menu inits):

```cpp
  // ========== PENTEST MODULE MENUS ==========

  #ifdef HAS_IR
    irMenu.list = new LinkedList<MenuNode>();
    irMenu.name = "IR Tools";
  #endif

  #ifdef HAS_SUBGHZ
    subghzMenu.list = new LinkedList<MenuNode>();
    subghzMenu.name = "Sub-GHz";
  #endif

  #ifdef HAS_RFID
    rfidMenu.list = new LinkedList<MenuNode>();
    rfidMenu.name = "RFID Tools";
  #endif
```

### Add main menu entries (around line ~1470, after GPS menu entry):

```cpp
  // ========== PENTEST MAIN MENU ENTRIES ==========

  #ifdef HAS_IR
    this->addNodes(&mainMenu, "IR Tools", TFTRED, NULL, ATTACKS, [this]() {
      this->changeMenu(&irMenu, true);
    });
  #endif

  #ifdef HAS_SUBGHZ
    this->addNodes(&mainMenu, "Sub-GHz", TFTORANGE, NULL, ATTACKS, [this]() {
      this->changeMenu(&subghzMenu, true);
    });
  #endif

  #ifdef HAS_RFID
    this->addNodes(&mainMenu, "RFID Tools", TFTCYAN, NULL, ATTACKS, [this]() {
      this->changeMenu(&rfidMenu, true);
    });
  #endif
```

### Add IR menu items (after main menu entries):

```cpp
  #ifdef HAS_IR
    // Build IR Menu
    irMenu.parentMenu = &mainMenu;
    this->addNodes(&irMenu, text09, TFTLIGHTGREY, NULL, 0, [this]() {
      this->changeMenu(irMenu.parentMenu, true);
    });
    this->addNodes(&irMenu, "Capture Signal", TFTGREEN, NULL, PROBE_SNIFF, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      ir_obj.setCurrentMode(IR_SCAN_RECEIVE);
    });
    this->addNodes(&irMenu, "Analyze Signal", TFTCYAN, NULL, PACKET_MONITOR, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      ir_obj.setCurrentMode(IR_SCAN_ANALYZE);
    });
    this->addNodes(&irMenu, "Replay Signal", TFTYELLOW, NULL, ATTACKS, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      ir_obj.setCurrentMode(IR_ATTACK_REPLAY);
    });
    this->addNodes(&irMenu, "TV-B-Gone", TFTRED, NULL, BEACON_SPAM, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      ir_obj.startBruteForce("TV");
      ir_obj.setCurrentMode(IR_ATTACK_BRUTE);
    });
  #endif
```

### Add Sub-GHz menu items:

```cpp
  #ifdef HAS_SUBGHZ
    // Build Sub-GHz Menu
    subghzMenu.parentMenu = &mainMenu;
    this->addNodes(&subghzMenu, text09, TFTLIGHTGREY, NULL, 0, [this]() {
      this->changeMenu(subghzMenu.parentMenu, true);
    });
    this->addNodes(&subghzMenu, "Capture Signal", TFTGREEN, NULL, PROBE_SNIFF, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      subghz_obj.setCurrentMode(SUBGHZ_SCAN_RAW);
    });
    this->addNodes(&subghzMenu, "Frequency Analyzer", TFTCYAN, NULL, PACKET_MONITOR, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      subghz_obj.setCurrentMode(SUBGHZ_SCAN_ANALYZE);
    });
    this->addNodes(&subghzMenu, "Replay Signal", TFTYELLOW, NULL, ATTACKS, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      subghz_obj.setCurrentMode(SUBGHZ_ATTACK_REPLAY);
    });
    this->addNodes(&subghzMenu, "RSSI Monitor", TFTMAGENTA, NULL, PACKET_MONITOR, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      subghz_obj.setCurrentMode(SUBGHZ_SCAN_RSSI);
    });
    this->addNodes(&subghzMenu, "Set 433 MHz", TFTORANGE, NULL, GENERAL_APPS, [this]() {
      subghz_obj.setFrequencyPreset(FREQ_433_MHZ);
    });
    this->addNodes(&subghzMenu, "Set 868 MHz", TFTORANGE, NULL, GENERAL_APPS, [this]() {
      subghz_obj.setFrequencyPreset(FREQ_868_MHZ);
    });
  #endif
```

### Add RFID menu items:

```cpp
  #ifdef HAS_RFID
    // Build RFID Menu
    rfidMenu.parentMenu = &mainMenu;
    this->addNodes(&rfidMenu, text09, TFTLIGHTGREY, NULL, 0, [this]() {
      this->changeMenu(rfidMenu.parentMenu, true);
    });
    this->addNodes(&rfidMenu, "Read Card", TFTGREEN, NULL, PROBE_SNIFF, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      rfid_obj.setCurrentMode(RFID_SCAN_READ);
    });
    this->addNodes(&rfidMenu, "Full Dump", TFTCYAN, NULL, PACKET_MONITOR, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      rfid_obj.setCurrentMode(RFID_SCAN_FULL_DUMP);
    });
    this->addNodes(&rfidMenu, "Test Default Keys", TFTYELLOW, NULL, ATTACKS, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      rfid_obj.setCurrentMode(RFID_SCAN_KEY_TEST);
    });
    this->addNodes(&rfidMenu, "Security Audit", TFTRED, NULL, ATTACKS, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      rfid_obj.setCurrentMode(RFID_SCAN_ANALYZE);
    });
    this->addNodes(&rfidMenu, "Monitor Cards", TFTMAGENTA, NULL, PACKET_MONITOR, [this]() {
      display_obj.clearScreen();
      this->drawStatusBar();
      rfid_obj.setCurrentMode(RFID_SCAN_CONTINUOUS);
    });
  #endif
```

---

## Step 6: Add to WiFiScan.cpp main() loop

### Add pentest mode handling (in the main switch/if block, around line ~2000):

```cpp
  // Handle pentest module modes
  #ifdef HAS_IR
    if (isIRMode(this->currentScanMode)) {
      // IR modes are handled by ir_obj.main() in PentestModule
      return;
    }
  #endif

  #ifdef HAS_SUBGHZ
    if (isSubGHzMode(this->currentScanMode)) {
      // Sub-GHz modes are handled by subghz_obj.main()
      return;
    }
  #endif

  #ifdef HAS_RFID
    if (isRFIDMode(this->currentScanMode)) {
      // RFID modes are handled by rfid_obj.main()
      return;
    }
  #endif
```

---

## Step 7: Enable the Build

### Option A: Use MARAUDER_PENTEST board (recommended)

In `configs.h`, uncomment:
```cpp
#define MARAUDER_PENTEST
```

### Option B: Add modules to existing board

Add to your board's feature section in configs.h:
```cpp
#define HAS_IR
#define HAS_SUBGHZ
#define HAS_RFID
#define MATRIX_THEME  // Optional: for green hacker theme
```

And add pin definitions:
```cpp
// IR Pins
#define IR_TX_PIN   16
#define IR_RX_PIN   17

// CC1101 Pins
#define CC1101_CS_PIN   27
#define CC1101_GDO0_PIN 25
#define CC1101_GDO2_PIN 26
#define CC1101_MOSI_PIN 23
#define CC1101_MISO_PIN 19
#define CC1101_SCK_PIN  18

// RC522 Pins
#define RFID_CS_PIN  13
#define RFID_RST_PIN 12
```

---

## Step 8: Install Required Libraries

Add to Arduino IDE or platformio.ini:

```
IRremoteESP8266
MFRC522
ELECHOUSE_CC1101_SRC_DRV
```

---

## Quick Start Checklist

- [ ] Add new files to esp32_marauder folder
- [ ] Modify configs.h (add MARAUDER_PENTEST or feature flags)
- [ ] Modify esp32_marauder.ino (includes + setup + loop)
- [ ] Modify WiFiScan.h (add mode constants)
- [ ] Modify MenuFunctions.h (add menu declarations)
- [ ] Modify MenuFunctions.cpp (add menu setup + entries)
- [ ] Install required libraries
- [ ] Compile and upload

---

## Troubleshooting

### "HAS_IR not defined"
Make sure you've enabled the feature in configs.h

### "ir_obj not declared"
Add the extern declarations to your file

### Menu not appearing
Check that menu initialization runs before addNodes calls

### Hardware not responding
Verify GPIO pin assignments match your wiring

---

## Saved Connections System

The SavedConnections module provides persistent storage for ALL connection data:

### Supported Data Types

| Module | Data Saved |
|--------|------------|
| **WiFi** | SSIDs, BSSIDs, channels, passwords, handshake files |
| **Bluetooth** | Device names, MAC addresses, skimmer flags |
| **IR** | Protocols, codes, raw timings, replay data |
| **Sub-GHz** | Frequencies, modulations, codes, rolling code warnings |
| **RFID** | UIDs, card types, keys, security assessments |

### Storage Structure

```
/pentest/
├── wifi/
│   └── wifi_*.dat
├── bluetooth/
│   └── bt_*.dat
├── ir/
│   └── ir_*.dat
├── subghz/
│   └── subghz_*.dat
├── rfid/
│   └── rfid_*.dat
└── session/
    ├── settings.json
    └── history.json
```

### Usage Examples

#### Quick Save from Capture
```cpp
// Save captured IR signal
uint32_t id = saved_conn_obj.quickSaveIR(protocol, code, bits, "TV_Power");

// Save WiFi network
uint32_t id = saved_conn_obj.quickSaveWiFi(ssid, bssid, channel, rssi, encType);

// Save Bluetooth device
uint32_t id = saved_conn_obj.quickSaveBT(deviceName, macAddress, rssi, isClassic);

// Save RFID card
uint32_t id = saved_conn_obj.quickSaveRFID(uid, uidLength, cardType, "Office_Badge");

// Save Sub-GHz signal
uint32_t id = saved_conn_obj.quickSaveSubGHz(frequency, protocol, code, "Garage_Door");
```

#### Rename and Edit
```cpp
// Rename saved items
saved_conn_obj.renameIRSignal(id, "New Name");
saved_conn_obj.renameWiFiNetwork(id, "Home Router");
saved_conn_obj.renameBTDevice(id, "My Headphones");
saved_conn_obj.renameRFIDCard(id, "Work Badge");
saved_conn_obj.renameSubGHzSignal(id, "Front Gate");

// Add notes
saved_conn_obj.setIRSignalNotes(id, "Works with Samsung TV");
saved_conn_obj.setWiFiNetworkNotes(id, "Captured handshake on 2024-01-15");

// Mark favorites
saved_conn_obj.toggleWiFiFavorite(id);
saved_conn_obj.toggleBTFavorite(id);

// Mark as pentest target
saved_conn_obj.markWiFiAsTarget(id, true);
saved_conn_obj.markBTAsSkimmer(id, true);

// Store WiFi password
saved_conn_obj.setWiFiPassword(id, "cracked_password");
```

#### Load and List
```cpp
// Get all saved items
LinkedList<SavedIRSignal>* irSignals = saved_conn_obj.getAllIRSignals();
LinkedList<SavedWiFiNetwork>* networks = saved_conn_obj.getAllWiFiNetworks();
LinkedList<SavedBTDevice>* btDevices = saved_conn_obj.getAllBTDevices();
LinkedList<SavedRFIDCard>* cards = saved_conn_obj.getAllRFIDCards();
LinkedList<SavedSubGHzSignal>* subghzSignals = saved_conn_obj.getAllSubGHzSignals();

// Get favorites only
LinkedList<SavedWiFiNetwork>* favorites = saved_conn_obj.getFavoriteWiFiNetworks();

// Get pentest targets
LinkedList<SavedWiFiNetwork>* targets = saved_conn_obj.getTargetWiFiNetworks();

// Get potential skimmers
LinkedList<SavedBTDevice>* skimmers = saved_conn_obj.getSkimmerBTDevices();
```

#### Delete
```cpp
saved_conn_obj.deleteIRSignal(id);
saved_conn_obj.deleteWiFiNetwork(id);
saved_conn_obj.deleteBTDevice(id);
saved_conn_obj.deleteRFIDCard(id);
saved_conn_obj.deleteSubGHzSignal(id);
```

### Session Management

```cpp
// Save current session (settings + history)
saved_conn_obj.saveSession();

// Load session on startup
saved_conn_obj.loadSession();

// Access settings
SessionSettings* settings = saved_conn_obj.getSettings();
settings->irAutoSave = true;
settings->matrixThemeEnabled = true;
saved_conn_obj.saveSettings(settings);

// View history
LinkedList<SessionHistoryEntry>* history = saved_conn_obj.getHistory();
```

### Data Persistence

All data is stored in JSON format on the SD card for:
- Portability between devices
- Easy backup and restore
- Human-readable format for debugging
- Import/export capabilities

### Auto-Save Feature

Enable auto-save in settings to automatically save:
- Every captured IR signal
- Every captured Sub-GHz signal
- Every read RFID card
- Session history after each action

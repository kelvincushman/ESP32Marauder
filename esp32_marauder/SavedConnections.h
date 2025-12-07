/*
 * SavedConnections.h
 * Persistent Storage Manager for Pentest Modules
 *
 * Handles saving/loading of:
 * - IR signals (captured remote codes)
 * - Sub-GHz signals (433/868MHz captures)
 * - RFID card data (UIDs, keys, dumps)
 * - Session data (settings, history, preferences)
 *
 * Uses SD card storage with JSON format for portability
 */

#pragma once

#ifndef SavedConnections_h
#define SavedConnections_h

#include "configs.h"
#include <Arduino.h>
#include <FS.h>
#include "SD.h"
#include <LinkedList.h>

// ============================================================================
// STORAGE PATHS
// ============================================================================

#define PENTEST_DIR           "/pentest"
#define IR_SIGNALS_DIR        "/pentest/ir"
#define SUBGHZ_SIGNALS_DIR    "/pentest/subghz"
#define RFID_CARDS_DIR        "/pentest/rfid"
#define SESSION_DIR           "/pentest/session"

#define IR_SIGNALS_FILE       "/pentest/ir/signals.json"
#define SUBGHZ_SIGNALS_FILE   "/pentest/subghz/signals.json"
#define RFID_CARDS_FILE       "/pentest/rfid/cards.json"
#define SESSION_FILE          "/pentest/session/session.json"
#define SETTINGS_FILE         "/pentest/session/settings.json"
#define HISTORY_FILE          "/pentest/session/history.json"

// ============================================================================
// STORAGE LIMITS
// ============================================================================

#define MAX_SAVED_IR_SIGNALS      50
#define MAX_SAVED_SUBGHZ_SIGNALS  50
#define MAX_SAVED_RFID_CARDS      100
#define MAX_SESSION_HISTORY       200
#define MAX_NAME_LENGTH           32
#define MAX_NOTES_LENGTH          128

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Saved IR Signal
struct SavedIRSignal {
    uint32_t id;
    char name[MAX_NAME_LENGTH];
    char notes[MAX_NOTES_LENGTH];
    uint8_t protocol;           // IR protocol type
    uint32_t code;              // Decoded code
    uint8_t bits;               // Bit length
    uint16_t* rawData;          // Raw timing data (if needed)
    uint16_t rawLength;
    uint32_t timestamp;         // When captured
    uint8_t successCount;       // Times successfully replayed
    bool favorite;
};

// Saved Sub-GHz Signal
struct SavedSubGHzSignal {
    uint32_t id;
    char name[MAX_NAME_LENGTH];
    char notes[MAX_NOTES_LENGTH];
    uint32_t frequency;         // Frequency in Hz
    uint8_t modulation;         // ASK, FSK, etc.
    uint8_t protocol;           // Decoded protocol
    uint32_t code;              // Decoded code (if applicable)
    uint8_t* rawData;           // Raw signal data
    uint16_t rawLength;
    uint32_t timestamp;
    bool hasRollingCode;        // Warning flag
    uint8_t successCount;
    bool favorite;
};

// Saved RFID Card
struct SavedRFIDCard {
    uint32_t id;
    char name[MAX_NAME_LENGTH];
    char notes[MAX_NOTES_LENGTH];
    uint8_t uid[10];            // Card UID (up to 10 bytes)
    uint8_t uidLength;
    uint8_t cardType;           // MIFARE Classic, DESFire, etc.
    uint8_t sak;                // SAK byte for identification
    uint8_t atqa[2];            // ATQA bytes
    uint8_t* keyA;              // Known Key A values (6 bytes per sector)
    uint8_t* keyB;              // Known Key B values
    uint8_t* sectorData;        // Full dump data (optional)
    uint16_t dataLength;
    uint8_t securityLevel;      // Security assessment
    uint32_t timestamp;
    bool favorite;
};

// Saved WiFi Network
struct SavedWiFiNetwork {
    uint32_t id;
    char name[MAX_NAME_LENGTH];         // User-defined name
    char notes[MAX_NOTES_LENGTH];
    char ssid[33];                      // Network SSID
    char bssid[18];                     // MAC address string
    char password[65];                  // Password if known (encrypted)
    uint8_t channel;
    int8_t rssi;                        // Signal strength when saved
    uint8_t encryptionType;             // Open, WEP, WPA, WPA2, etc.
    bool hasHandshake;                  // EAPOL captured
    char handshakeFile[32];             // Path to PCAP file
    uint32_t timestamp;
    uint32_t lastSeen;                  // Last time detected
    uint8_t timesFound;                 // How many times discovered
    bool favorite;
    bool isTarget;                      // Mark as pentest target
};

// Saved Bluetooth Device
struct SavedBTDevice {
    uint32_t id;
    char name[MAX_NAME_LENGTH];         // User-defined name
    char notes[MAX_NOTES_LENGTH];
    char deviceName[33];                // Advertised device name
    char macAddress[18];                // MAC address
    uint8_t deviceClass[3];             // Class of device
    int8_t rssi;                        // Signal strength
    bool isClassic;                     // Classic BT vs BLE
    bool isSkimmer;                     // Flagged as potential skimmer
    uint8_t* serviceUUIDs;              // Discovered service UUIDs
    uint8_t serviceCount;
    uint32_t timestamp;
    uint32_t lastSeen;
    uint8_t timesFound;
    bool favorite;
    bool isTarget;
};

// Session History Entry
struct SessionHistoryEntry {
    uint32_t timestamp;
    uint8_t moduleType;         // 1=IR, 2=SubGHz, 3=RFID
    uint8_t actionType;         // Capture, Replay, Read, etc.
    char details[64];           // Action details
    bool success;
};

// Session Settings
struct SessionSettings {
    // IR Settings
    bool irAutoSave;
    uint8_t irDefaultProtocol;

    // Sub-GHz Settings
    bool subghzAutoSave;
    uint32_t subghzDefaultFreq;
    uint8_t subghzDefaultModulation;

    // RFID Settings
    bool rfidAutoSave;
    bool rfidAutoKeyTest;

    // Display Settings
    bool matrixThemeEnabled;
    uint8_t displayBrightness;

    // General
    bool soundEnabled;
    bool vibrationEnabled;
    uint32_t autoSleepMs;
};

// ============================================================================
// SAVED CONNECTIONS MANAGER CLASS
// ============================================================================

class SavedConnections {
public:
    // Lifecycle
    void RunSetup();
    bool isReady();

    // Directory Management
    bool createDirectories();
    bool checkSDCard();

    // ========== IR SIGNAL OPERATIONS ==========
    bool saveIRSignal(SavedIRSignal* signal);
    bool loadIRSignal(uint32_t id, SavedIRSignal* signal);
    bool deleteIRSignal(uint32_t id);
    bool updateIRSignal(SavedIRSignal* signal);
    bool renameIRSignal(uint32_t id, const char* newName);
    bool setIRSignalNotes(uint32_t id, const char* notes);
    bool toggleIRFavorite(uint32_t id);
    LinkedList<SavedIRSignal>* getAllIRSignals();
    LinkedList<SavedIRSignal>* getFavoriteIRSignals();
    uint8_t getIRSignalCount();

    // Quick save from capture
    uint32_t quickSaveIR(uint8_t protocol, uint32_t code, uint8_t bits, const char* autoName = NULL);

    // ========== SUB-GHZ SIGNAL OPERATIONS ==========
    bool saveSubGHzSignal(SavedSubGHzSignal* signal);
    bool loadSubGHzSignal(uint32_t id, SavedSubGHzSignal* signal);
    bool deleteSubGHzSignal(uint32_t id);
    bool updateSubGHzSignal(SavedSubGHzSignal* signal);
    bool renameSubGHzSignal(uint32_t id, const char* newName);
    bool setSubGHzSignalNotes(uint32_t id, const char* notes);
    bool toggleSubGHzFavorite(uint32_t id);
    LinkedList<SavedSubGHzSignal>* getAllSubGHzSignals();
    LinkedList<SavedSubGHzSignal>* getFavoriteSubGHzSignals();
    uint8_t getSubGHzSignalCount();

    // Quick save from capture
    uint32_t quickSaveSubGHz(uint32_t frequency, uint8_t protocol, uint32_t code, const char* autoName = NULL);

    // ========== RFID CARD OPERATIONS ==========
    bool saveRFIDCard(SavedRFIDCard* card);
    bool loadRFIDCard(uint32_t id, SavedRFIDCard* card);
    bool deleteRFIDCard(uint32_t id);
    bool updateRFIDCard(SavedRFIDCard* card);
    bool renameRFIDCard(uint32_t id, const char* newName);
    bool setRFIDCardNotes(uint32_t id, const char* notes);
    bool toggleRFIDFavorite(uint32_t id);
    LinkedList<SavedRFIDCard>* getAllRFIDCards();
    LinkedList<SavedRFIDCard>* getFavoriteRFIDCards();
    uint8_t getRFIDCardCount();

    // Quick save from read
    uint32_t quickSaveRFID(uint8_t* uid, uint8_t uidLength, uint8_t cardType, const char* autoName = NULL);

    // ========== WIFI NETWORK OPERATIONS ==========
    bool saveWiFiNetwork(SavedWiFiNetwork* network);
    bool loadWiFiNetwork(uint32_t id, SavedWiFiNetwork* network);
    bool deleteWiFiNetwork(uint32_t id);
    bool updateWiFiNetwork(SavedWiFiNetwork* network);
    bool renameWiFiNetwork(uint32_t id, const char* newName);
    bool setWiFiNetworkNotes(uint32_t id, const char* notes);
    bool setWiFiPassword(uint32_t id, const char* password);
    bool toggleWiFiFavorite(uint32_t id);
    bool markWiFiAsTarget(uint32_t id, bool isTarget);
    LinkedList<SavedWiFiNetwork>* getAllWiFiNetworks();
    LinkedList<SavedWiFiNetwork>* getFavoriteWiFiNetworks();
    LinkedList<SavedWiFiNetwork>* getTargetWiFiNetworks();
    uint8_t getWiFiNetworkCount();

    // Quick save from scan
    uint32_t quickSaveWiFi(const char* ssid, const char* bssid, uint8_t channel, int8_t rssi, uint8_t encType);

    // ========== BLUETOOTH DEVICE OPERATIONS ==========
    bool saveBTDevice(SavedBTDevice* device);
    bool loadBTDevice(uint32_t id, SavedBTDevice* device);
    bool deleteBTDevice(uint32_t id);
    bool updateBTDevice(SavedBTDevice* device);
    bool renameBTDevice(uint32_t id, const char* newName);
    bool setBTDeviceNotes(uint32_t id, const char* notes);
    bool toggleBTFavorite(uint32_t id);
    bool markBTAsTarget(uint32_t id, bool isTarget);
    bool markBTAsSkimmer(uint32_t id, bool isSkimmer);
    LinkedList<SavedBTDevice>* getAllBTDevices();
    LinkedList<SavedBTDevice>* getFavoriteBTDevices();
    LinkedList<SavedBTDevice>* getSkimmerBTDevices();
    uint8_t getBTDeviceCount();

    // Quick save from scan
    uint32_t quickSaveBT(const char* deviceName, const char* macAddress, int8_t rssi, bool isClassic);

    // ========== SESSION MANAGEMENT ==========
    bool saveSession();
    bool loadSession();
    bool clearSession();

    // Settings
    bool saveSettings(SessionSettings* settings);
    bool loadSettings(SessionSettings* settings);
    SessionSettings* getSettings();

    // History
    bool addHistoryEntry(uint8_t moduleType, uint8_t actionType, const char* details, bool success);
    LinkedList<SessionHistoryEntry>* getHistory();
    bool clearHistory();

    // ========== EXPORT/IMPORT ==========
    bool exportToFile(const char* filename);
    bool importFromFile(const char* filename);
    bool exportIRSignal(uint32_t id, const char* filename);
    bool exportSubGHzSignal(uint32_t id, const char* filename);
    bool exportRFIDCard(uint32_t id, const char* filename);

    // ========== UTILITY ==========
    uint32_t generateUniqueId();
    String formatTimestamp(uint32_t timestamp);
    uint32_t getTotalStorageUsed();
    uint32_t getAvailableStorage();

private:
    bool initialized;
    bool sdCardReady;

    // Cached data
    LinkedList<SavedIRSignal>* irSignals;
    LinkedList<SavedSubGHzSignal>* subghzSignals;
    LinkedList<SavedRFIDCard>* rfidCards;
    LinkedList<SessionHistoryEntry>* sessionHistory;
    SessionSettings currentSettings;

    // Internal helpers
    bool writeFile(const char* path, const char* data);
    String readFile(const char* path);
    bool fileExists(const char* path);
    bool deleteFile(const char* path);

    // JSON helpers (simple implementation without external library)
    String irSignalToJson(SavedIRSignal* signal);
    bool jsonToIRSignal(String json, SavedIRSignal* signal);
    String subghzSignalToJson(SavedSubGHzSignal* signal);
    bool jsonToSubGHzSignal(String json, SavedSubGHzSignal* signal);
    String rfidCardToJson(SavedRFIDCard* card);
    bool jsonToRFIDCard(String json, SavedRFIDCard* card);

    // Auto-naming
    String generateAutoName(uint8_t moduleType, uint8_t protocol = 0);
};

extern SavedConnections saved_conn_obj;

// ============================================================================
// ACTION TYPE CONSTANTS (for history)
// ============================================================================

#define ACTION_CAPTURE      1
#define ACTION_REPLAY       2
#define ACTION_READ         3
#define ACTION_WRITE        4
#define ACTION_ANALYZE      5
#define ACTION_BRUTE_FORCE  6
#define ACTION_SAVE         7
#define ACTION_LOAD         8
#define ACTION_DELETE       9
#define ACTION_EXPORT       10

// ============================================================================
// MODULE TYPE CONSTANTS
// ============================================================================

#define MODULE_IR       1
#define MODULE_SUBGHZ   2
#define MODULE_RFID     3
#define MODULE_WIFI     4
#define MODULE_BT       5

#endif // SavedConnections_h

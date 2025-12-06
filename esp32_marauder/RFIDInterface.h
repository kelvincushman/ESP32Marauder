/*
 * RFIDInterface.h
 * RFID/NFC Security Auditing Module for ESP32 Marauder
 *
 * Purpose: Ethical penetration testing of RFID access control systems
 *
 * LEGAL NOTICE: This module is intended for authorized security testing only.
 * Unauthorized access to computer systems or cloning of access credentials
 * without permission is illegal. Always obtain written authorization before
 * testing any system you do not own.
 *
 * Supported Hardware:
 *   - MFRC522 (RC522) - 13.56 MHz RFID reader
 *   - PN532 - 13.56 MHz NFC reader (more features)
 *
 * Capabilities:
 *   - Card identification and type detection
 *   - UID reading and analysis
 *   - Default key vulnerability testing
 *   - Sector dump (with valid keys)
 *   - Security posture assessment
 *   - Card cloning (to Magic cards, for authorized testing)
 *   - Pentest report generation
 */

#pragma once

#ifndef RFIDInterface_h
#define RFIDInterface_h

#include "configs.h"

#ifdef HAS_RFID

#include <SPI.h>
#include <MFRC522.h>
#include <LinkedList.h>
#include <ArduinoJson.h>

// ============================================================================
// RFID Scan Mode Constants (80-99 reserved for RFID)
// ============================================================================
#define RFID_SCAN_OFF           0
#define RFID_SCAN_READ          80   // Read card UID and basic info
#define RFID_SCAN_FULL_DUMP     81   // Attempt full sector dump
#define RFID_SCAN_KEY_TEST      82   // Test for default/weak keys
#define RFID_SCAN_CONTINUOUS    83   // Continuous card monitoring
#define RFID_SCAN_CLONE         84   // Clone to Magic card
#define RFID_SCAN_WRITE         85   // Write data to card
#define RFID_SCAN_COMPARE       86   // Compare two cards
#define RFID_SCAN_BRUTE_KEYS    87   // Brute force sector keys
#define RFID_SCAN_EMULATE       88   // Card emulation (PN532 only)
#define RFID_SCAN_ANALYZE       89   // Deep security analysis

// ============================================================================
// Card Type Identifiers (based on SAK byte)
// ============================================================================
enum RFIDCardType {
    CARD_UNKNOWN = 0,
    CARD_MIFARE_MINI,           // 320 bytes
    CARD_MIFARE_1K,             // 1024 bytes, 16 sectors
    CARD_MIFARE_4K,             // 4096 bytes, 40 sectors
    CARD_MIFARE_ULTRALIGHT,     // 64 bytes
    CARD_MIFARE_ULTRALIGHT_C,   // 192 bytes, 3DES auth
    CARD_MIFARE_PLUS_2K,        // Security Level 1-3
    CARD_MIFARE_PLUS_4K,
    CARD_MIFARE_DESFIRE,        // AES encryption
    CARD_MIFARE_DESFIRE_EV1,
    CARD_MIFARE_DESFIRE_EV2,
    CARD_NTAG_213,              // 144 bytes
    CARD_NTAG_215,              // 504 bytes
    CARD_NTAG_216,              // 888 bytes
    CARD_ISO14443_4,            // Smart card protocol
    CARD_ISO18092               // NFC peer-to-peer
};

// ============================================================================
// Security Level Assessment
// ============================================================================
enum SecurityLevel {
    SEC_CRITICAL = 0,    // No security, easily cloned
    SEC_LOW,             // Weak encryption, default keys
    SEC_MEDIUM,          // Some protection, may have vulns
    SEC_HIGH,            // Strong encryption, properly configured
    SEC_UNKNOWN          // Cannot determine
};

// ============================================================================
// Structures for Card Data
// ============================================================================

// Sector key information
struct SectorKey {
    uint8_t sector;
    uint8_t keyA[6];
    uint8_t keyB[6];
    bool keyAFound;
    bool keyBFound;
    uint8_t accessBits[4];
};

// Complete card dump
struct CardData {
    // Identification
    uint8_t uid[10];        // UID (4, 7, or 10 bytes)
    uint8_t uidLength;
    uint8_t sak;            // Select Acknowledge byte
    uint8_t atqa[2];        // Answer To Request type A

    // Classification
    RFIDCardType cardType;
    SecurityLevel secLevel;
    String cardTypeName;
    String manufacturer;

    // Memory
    uint16_t memorySize;
    uint8_t numSectors;

    // Security findings
    uint8_t defaultKeysFound;
    uint8_t sectorsReadable;
    bool uidChangeable;     // Magic card detection

    // Timestamps
    uint32_t firstSeen;
    uint32_t lastSeen;
    uint8_t readCount;

    // Raw data (for cloning)
    uint8_t* sectorData;    // Dynamically allocated
    bool fullDumpAvailable;
};

// Audit log entry
struct AuditEntry {
    uint32_t timestamp;
    String action;
    String uid;
    String result;
    String details;
};

// ============================================================================
// Default Keys Database
// Common keys found in the wild - for security testing only
// ============================================================================
const uint8_t DEFAULT_KEYS[][6] PROGMEM = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // Factory default (most common)
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},  // MAD key (MIFARE Application Directory)
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},  // Common default
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},  // Public transit (some systems)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // Null key
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},  // Common in access control
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},  // Found in hotel systems
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},  // Sequential pattern
    {0x71, 0x4C, 0x5C, 0x88, 0x6E, 0x97},  // Common weak key
    {0x58, 0x7E, 0xE5, 0xF9, 0x35, 0x0F},  // Found in parking systems
    {0xA0, 0x47, 0x8C, 0xC3, 0x90, 0x91},  // Common in Europe
    {0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6},  // Industrial systems
    {0x8F, 0xD0, 0xA4, 0xF2, 0x56, 0xE9},  // Access control variant
};
const uint8_t NUM_DEFAULT_KEYS = 13;

// ============================================================================
// RFID Interface Class
// ============================================================================
class RFIDInterface {
public:
    // Lifecycle
    void RunSetup();
    void main(uint32_t currentTime);
    void shutdown();

    // Card Operations
    bool detectCard();
    bool readCard(CardData* card);
    bool readFullDump(CardData* card);
    bool writeCard(CardData* source, bool writeUID = false);
    bool verifyWrite(CardData* source);

    // Security Testing
    SecurityLevel assessSecurity(CardData* card);
    uint8_t testDefaultKeys(CardData* card, LinkedList<SectorKey>* foundKeys);
    bool testSectorKey(uint8_t sector, uint8_t* key, bool keyA);
    bool detectMagicCard();

    // Analysis
    RFIDCardType identifyCardType(uint8_t sak, uint8_t* atqa);
    String getCardTypeName(RFIDCardType type);
    String getManufacturer(uint8_t* uid);
    String getSecurityLevelName(SecurityLevel level);

    // Storage & Export
    bool saveCardToSD(CardData* card, String filename);
    bool loadCardFromSD(CardData* card, String filename);
    String generateReport(CardData* card);
    String generatePentestReport(LinkedList<CardData>* cards);

    // Audit Logging
    void logAction(String action, String uid, String result, String details);
    String getAuditLog();
    void clearAuditLog();

    // UI Helpers
    String getUIDString(uint8_t* uid, uint8_t length);
    String getSAKDescription(uint8_t sak);
    String formatMemoryDump(uint8_t* data, uint16_t length);

    // State
    bool isCardPresent();
    CardData* getLastCard();
    uint8_t getCurrentMode();
    void setCurrentMode(uint8_t mode);

    // Continuous monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring();

    // Statistics
    uint32_t getTotalCardsRead();
    uint32_t getVulnerableCardsFound();

private:
    MFRC522* mfrc522;

    // State
    uint8_t currentMode;
    bool cardPresent;
    bool monitoring;
    uint32_t lastScanTime;
    uint32_t scanInterval;

    // Data
    CardData lastCard;
    LinkedList<CardData>* scannedCards;
    LinkedList<AuditEntry>* auditLog;
    LinkedList<SectorKey>* foundKeys;

    // Statistics
    uint32_t totalCardsRead;
    uint32_t vulnerableCards;
    uint32_t cloneAttempts;

    // Internal helpers
    bool authenticateSector(uint8_t sector, uint8_t* key, bool keyA);
    bool readSector(uint8_t sector, uint8_t* buffer);
    bool writeSector(uint8_t sector, uint8_t* data, uint8_t* key, bool keyA);
    bool readBlock(uint8_t block, uint8_t* buffer);
    bool writeBlock(uint8_t block, uint8_t* data);
    void haltCard();
    void resetReader();

    // Magic card operations
    bool unlockMagicCard();
    bool writeUID(uint8_t* newUID, uint8_t length);

    // Utility
    uint8_t getSectorForBlock(uint8_t block);
    uint8_t getFirstBlockOfSector(uint8_t sector);
    uint8_t getNumBlocksInSector(uint8_t sector);
    void copyKey(uint8_t* dest, const uint8_t* src);
    bool compareKeys(uint8_t* key1, uint8_t* key2);
    String bytesToHex(uint8_t* data, uint8_t length);
    void hexToBytes(String hex, uint8_t* bytes, uint8_t length);
};

extern RFIDInterface rfid_obj;

#endif // HAS_RFID
#endif // RFIDInterface_h

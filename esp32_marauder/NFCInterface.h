/*
 * NFCInterface.h - PN532 NFC Module for ESP32 Marauder
 *
 * Advanced NFC interface with EMV/APDU support
 * Supports ISO 14443A/B, FeliCa, and card emulation
 *
 * Hardware: PN532 module via I2C
 * Library: Adafruit_PN532
 */

#ifndef NFCInterface_h
#define NFCInterface_h

#include "configs.h"
#include "configs_pentest.h"

#ifdef HAS_PN532

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "lang_var.h"

// ========== NFC Scan Mode Constants ==========
// NFC modes use range 160-179
#define NFC_SCAN_READ           160
#define NFC_SCAN_EMV            161
#define NFC_SCAN_FELICA         162
#define NFC_SCAN_EMULATE        163
#define NFC_SCAN_APDU           164
#define NFC_SCAN_POLL           165
#define NFC_SCAN_DUMP           166
#define NFC_ATTACK_RELAY        167  // Educational - detect relay attacks

// ========== EMV Application IDs (AIDs) ==========
// Common payment application identifiers
const uint8_t AID_VISA[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10};
const uint8_t AID_VISA_ELECTRON[] = {0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10};
const uint8_t AID_MASTERCARD[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10};
const uint8_t AID_MAESTRO[] = {0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60};
const uint8_t AID_AMEX[] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0x25};
const uint8_t AID_DISCOVER[] = {0xA0, 0x00, 0x00, 0x01, 0x52, 0x30, 0x10};
const uint8_t AID_JCB[] = {0xA0, 0x00, 0x00, 0x00, 0x65, 0x10, 0x10};
const uint8_t AID_UNIONPAY[] = {0xA0, 0x00, 0x00, 0x03, 0x33, 0x01, 0x01};
const uint8_t AID_PPSE[] = {0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31}; // 2PAY.SYS.DDF01

// ========== APDU Commands ==========
#define APDU_CLA_ISO          0x00
#define APDU_INS_SELECT       0xA4
#define APDU_INS_READ_RECORD  0xB2
#define APDU_INS_GET_DATA     0xCA
#define APDU_INS_GPO          0xA8  // Get Processing Options

// SELECT command P1/P2
#define SELECT_BY_NAME        0x04
#define SELECT_FIRST          0x00

// ========== EMV Tags ==========
#define EMV_TAG_PAN           0x5A    // Primary Account Number
#define EMV_TAG_EXPIRY        0x5F24  // Application Expiration Date
#define EMV_TAG_TRACK2        0x57    // Track 2 Equivalent Data
#define EMV_TAG_CARDHOLDER    0x5F20  // Cardholder Name
#define EMV_TAG_AID           0x4F    // Application Identifier
#define EMV_TAG_APP_LABEL     0x50    // Application Label
#define EMV_TAG_PDOL          0x9F38  // Processing Options Data Object List
#define EMV_TAG_AFL           0x94    // Application File Locator
#define EMV_TAG_AIP           0x82    // Application Interchange Profile

// ========== Card Types ==========
enum NFCCardType {
    NFC_CARD_UNKNOWN = 0,
    NFC_CARD_MIFARE_CLASSIC_1K,
    NFC_CARD_MIFARE_CLASSIC_4K,
    NFC_CARD_MIFARE_ULTRALIGHT,
    NFC_CARD_MIFARE_DESFIRE,
    NFC_CARD_NTAG213,
    NFC_CARD_NTAG215,
    NFC_CARD_NTAG216,
    NFC_CARD_ISO14443_4,  // Smart cards, bank cards
    NFC_CARD_FELICA,
    NFC_CARD_ISO15693
};

// ========== Data Structures ==========
struct APDUCommand {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
    uint8_t data[255];
    uint8_t le;
    bool hasLe;
};

struct APDUResponse {
    uint8_t data[256];
    uint16_t dataLen;
    uint8_t sw1;
    uint8_t sw2;
    bool success;
};

struct EMVApplication {
    uint8_t aid[16];
    uint8_t aidLen;
    char label[32];
    uint8_t priority;
};

struct EMVCardData {
    char pan[20];           // Card number (if readable)
    char expiry[8];         // YYMM format
    char cardholderName[32];
    char applicationLabel[32];
    uint8_t aid[16];
    uint8_t aidLen;
    bool panFound;
    bool expiryFound;
    bool nameFound;
};

struct NFCCardInfo {
    uint8_t uid[10];
    uint8_t uidLength;
    NFCCardType cardType;
    uint8_t sak;            // Select Acknowledge
    uint8_t atqa[2];        // Answer To Request Type A
    bool iso14443_4;        // Supports ISO 14443-4 (smart cards)
    uint32_t timestamp;
    EMVCardData emvData;
};

struct FeliCaCard {
    uint8_t idm[8];         // Manufacture ID
    uint8_t pmm[8];         // Manufacture Parameter
    uint16_t systemCode;
    uint32_t timestamp;
};

// ========== NFC Interface Class ==========
class NFCInterface {
private:
    Adafruit_PN532* pn532;
    bool initialized;
    bool scanning;
    uint8_t currentMode;

    // Card data
    NFCCardInfo lastCard;
    FeliCaCard lastFeliCa;
    LinkedList<NFCCardInfo>* scannedCards;
    LinkedList<EMVApplication>* emvApps;

    // APDU buffer
    uint8_t apduBuffer[300];
    uint16_t apduLen;

    // Display reference
    TFT_eSPI* tft;

    // Internal methods
    NFCCardType detectCardType(uint8_t sak, uint8_t* atqa);
    String cardTypeToString(NFCCardType type);

    // APDU methods
    APDUResponse sendAPDU(APDUCommand& cmd);
    APDUResponse sendRawAPDU(uint8_t* apdu, uint16_t len);
    bool selectApplication(const uint8_t* aid, uint8_t aidLen);

    // EMV methods
    bool selectPPSE();
    bool parseEMVResponse(uint8_t* data, uint16_t len, EMVCardData& cardData);
    bool parseTLV(uint8_t* data, uint16_t len, uint16_t tag, uint8_t* value, uint8_t* valueLen);
    bool readEMVRecords(EMVCardData& cardData);
    String formatPAN(const char* pan);

    // FeliCa methods
    bool pollFeliCa(uint16_t systemCode);
    bool readFeliCaBlock(uint8_t* idm, uint16_t serviceCode, uint8_t blockNum, uint8_t* data);

    // Display methods
    void displayCardInfo(NFCCardInfo& card);
    void displayEMVData(EMVCardData& data);
    void displayFeliCaInfo(FeliCaCard& card);
    void displayAPDUResponse(APDUResponse& resp);
    void drawProgressBar(int x, int y, int w, int h, int progress);

    // File operations
    String generateFilename(NFCCardInfo& card);

public:
    NFCInterface();
    ~NFCInterface();

    // Core methods
    void RunSetup();
    void main(uint32_t currentTime);
    void shutdown();

    // Mode control
    void startScan(uint8_t mode);
    void stopScan();
    bool isScanning() { return scanning; }
    uint8_t getMode() { return currentMode; }

    // Card reading
    bool pollCard();
    bool readCard(NFCCardInfo& card);
    NFCCardInfo getLastCard() { return lastCard; }

    // EMV operations
    bool readEMV(EMVCardData& data);
    bool listEMVApplications();
    LinkedList<EMVApplication>* getEMVApps() { return emvApps; }

    // APDU operations
    APDUResponse executeAPDU(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                             uint8_t* data = nullptr, uint8_t dataLen = 0, uint8_t le = 0);
    APDUResponse selectByAID(const uint8_t* aid, uint8_t aidLen);
    APDUResponse readRecord(uint8_t sfi, uint8_t record);
    APDUResponse getData(uint16_t tag);
    APDUResponse getProcessingOptions(uint8_t* pdol = nullptr, uint8_t pdolLen = 0);

    // FeliCa operations
    bool readFeliCa(FeliCaCard& card);
    FeliCaCard getLastFeliCa() { return lastFeliCa; }

    // Card emulation
    bool startEmulation(uint8_t* uid, uint8_t uidLen);
    void stopEmulation();

    // Utility
    int getScannedCount() { return scannedCards->size(); }
    void clearScanned();

    // Storage operations
    bool saveCard(NFCCardInfo& card, const char* filename = nullptr);
    bool loadCard(const char* filename, NFCCardInfo& card);
    bool exportAPDULog(const char* filename);

    // Status
    bool isInitialized() { return initialized; }
    String getStatusString();

    // Menu helpers
    void buildMenu(LinkedList<String>* menuItems);
    String getMenuTitle();
};

extern NFCInterface nfc_obj;

#endif // HAS_PN532
#endif // NFCInterface_h

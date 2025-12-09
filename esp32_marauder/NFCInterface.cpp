/*
 * NFCInterface.cpp - PN532 NFC Module Implementation
 *
 * Advanced NFC interface with EMV/APDU support
 * For educational and authorized security testing only
 */

#include "NFCInterface.h"

#ifdef HAS_PN532

NFCInterface nfc_obj;

// ========== Constructor / Destructor ==========

NFCInterface::NFCInterface() {
    pn532 = nullptr;
    initialized = false;
    scanning = false;
    currentMode = 0;
    scannedCards = new LinkedList<NFCCardInfo>();
    emvApps = new LinkedList<EMVApplication>();
    tft = nullptr;
    memset(&lastCard, 0, sizeof(NFCCardInfo));
    memset(&lastFeliCa, 0, sizeof(FeliCaCard));
}

NFCInterface::~NFCInterface() {
    shutdown();
    delete scannedCards;
    delete emvApps;
    if (pn532) delete pn532;
}

// ========== Core Methods ==========

void NFCInterface::RunSetup() {
    Serial.println(F("[NFC] Initializing PN532..."));

    // Initialize I2C with custom pins
    Wire.begin(PN532_SDA_PIN, PN532_SCL_PIN);

    // Create PN532 instance (I2C mode)
    pn532 = new Adafruit_PN532(PN532_IRQ_PIN, PN532_RST_PIN);

    pn532->begin();

    uint32_t versiondata = pn532->getFirmwareVersion();
    if (!versiondata) {
        Serial.println(F("[NFC] PN532 not found!"));
        initialized = false;
        return;
    }

    // Print chip info
    Serial.print(F("[NFC] Found PN5"));
    Serial.print((versiondata >> 24) & 0xFF, HEX);
    Serial.print(F(" Firmware: "));
    Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.');
    Serial.println((versiondata >> 8) & 0xFF, DEC);

    // Configure for ISO14443A
    pn532->SAMConfig();

    // Set max retries for passive activation
    pn532->setPassiveActivationRetries(0xFF);

    initialized = true;
    Serial.println(F("[NFC] PN532 initialized successfully"));
}

void NFCInterface::main(uint32_t currentTime) {
    if (!initialized || !scanning) return;

    static uint32_t lastPoll = 0;
    const uint32_t POLL_INTERVAL = 250; // Poll every 250ms

    if (currentTime - lastPoll < POLL_INTERVAL) return;
    lastPoll = currentTime;

    switch (currentMode) {
        case NFC_SCAN_READ:
            // Basic card reading
            if (pollCard()) {
                if (readCard(lastCard)) {
                    displayCardInfo(lastCard);
                    scannedCards->add(lastCard);
                }
            }
            break;

        case NFC_SCAN_EMV:
            // EMV card reading
            if (pollCard()) {
                if (lastCard.iso14443_4) {
                    EMVCardData emvData;
                    if (readEMV(emvData)) {
                        lastCard.emvData = emvData;
                        displayEMVData(emvData);
                        scannedCards->add(lastCard);
                    }
                } else {
                    Serial.println(F("[NFC] Card does not support ISO 14443-4"));
                }
            }
            break;

        case NFC_SCAN_FELICA:
            // FeliCa polling
            {
                FeliCaCard felica;
                if (readFeliCa(felica)) {
                    lastFeliCa = felica;
                    displayFeliCaInfo(felica);
                }
            }
            break;

        case NFC_SCAN_POLL:
            // Continuous polling - show any card type
            if (pollCard()) {
                readCard(lastCard);
                displayCardInfo(lastCard);
            }
            break;

        case NFC_SCAN_DUMP:
            // Full card dump
            if (pollCard()) {
                if (readCard(lastCard)) {
                    displayCardInfo(lastCard);
                    // If smart card, try EMV
                    if (lastCard.iso14443_4) {
                        EMVCardData emvData;
                        if (readEMV(emvData)) {
                            lastCard.emvData = emvData;
                            displayEMVData(emvData);
                        }
                    }
                    saveCard(lastCard);
                    scannedCards->add(lastCard);
                }
            }
            break;

        default:
            break;
    }
}

void NFCInterface::shutdown() {
    stopScan();
    clearScanned();
    initialized = false;
    Serial.println(F("[NFC] Shutdown complete"));
}

// ========== Mode Control ==========

void NFCInterface::startScan(uint8_t mode) {
    if (!initialized) {
        Serial.println(F("[NFC] Not initialized!"));
        return;
    }

    currentMode = mode;
    scanning = true;
    Serial.print(F("[NFC] Starting scan mode: "));
    Serial.println(mode);

    // Configure PN532 based on mode
    switch (mode) {
        case NFC_SCAN_FELICA:
            // Configure for FeliCa
            Serial.println(F("[NFC] Configuring for FeliCa"));
            break;

        default:
            // Default ISO14443A configuration
            pn532->SAMConfig();
            break;
    }
}

void NFCInterface::stopScan() {
    scanning = false;
    currentMode = 0;
    Serial.println(F("[NFC] Scan stopped"));
}

// ========== Card Reading ==========

bool NFCInterface::pollCard() {
    uint8_t uid[7];
    uint8_t uidLength;

    // Try to read ISO14443A card
    if (pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
        memcpy(lastCard.uid, uid, uidLength);
        lastCard.uidLength = uidLength;
        lastCard.timestamp = millis();

        // Get additional card info
        // SAK and ATQA are set during anti-collision
        // The Adafruit library doesn't expose these directly, but we can infer from behavior

        return true;
    }

    return false;
}

bool NFCInterface::readCard(NFCCardInfo& card) {
    uint8_t uid[7];
    uint8_t uidLength;

    if (!pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500)) {
        return false;
    }

    memcpy(card.uid, uid, uidLength);
    card.uidLength = uidLength;
    card.timestamp = millis();

    // Try to get more info via RATS (Request Answer To Select)
    // This tells us if the card supports ISO 14443-4
    uint8_t atr[32];
    uint8_t atrLen;

    // Send RATS command
    uint8_t rats[] = {0xE0, 0x80}; // RATS with CID=0
    uint8_t response[64];
    uint8_t responseLen;

    if (pn532->inDataExchange(rats, 2, response, &responseLen)) {
        card.iso14443_4 = true;
        card.cardType = NFC_CARD_ISO14443_4;
        Serial.println(F("[NFC] Card supports ISO 14443-4 (smart card)"));
    } else {
        card.iso14443_4 = false;
        // Determine type based on UID length and other factors
        if (uidLength == 4) {
            card.cardType = NFC_CARD_MIFARE_CLASSIC_1K; // Assume classic, could be others
        } else if (uidLength == 7) {
            card.cardType = NFC_CARD_MIFARE_ULTRALIGHT; // Could be NTAG
        }
    }

    Serial.print(F("[NFC] Card UID: "));
    for (int i = 0; i < uidLength; i++) {
        Serial.print(uid[i], HEX);
        Serial.print(' ');
    }
    Serial.println();

    return true;
}

NFCCardType NFCInterface::detectCardType(uint8_t sak, uint8_t* atqa) {
    // SAK (Select Acknowledge) tells us the card type
    switch (sak) {
        case 0x00:
            return NFC_CARD_MIFARE_ULTRALIGHT;
        case 0x08:
            return NFC_CARD_MIFARE_CLASSIC_1K;
        case 0x09:
            return NFC_CARD_MIFARE_CLASSIC_1K; // Mini
        case 0x18:
            return NFC_CARD_MIFARE_CLASSIC_4K;
        case 0x20:
            return NFC_CARD_ISO14443_4; // Smart card
        case 0x28:
            return NFC_CARD_MIFARE_CLASSIC_1K; // Emulated
        default:
            return NFC_CARD_UNKNOWN;
    }
}

String NFCInterface::cardTypeToString(NFCCardType type) {
    switch (type) {
        case NFC_CARD_MIFARE_CLASSIC_1K: return "MIFARE Classic 1K";
        case NFC_CARD_MIFARE_CLASSIC_4K: return "MIFARE Classic 4K";
        case NFC_CARD_MIFARE_ULTRALIGHT: return "MIFARE Ultralight";
        case NFC_CARD_MIFARE_DESFIRE: return "MIFARE DESFire";
        case NFC_CARD_NTAG213: return "NTAG213";
        case NFC_CARD_NTAG215: return "NTAG215";
        case NFC_CARD_NTAG216: return "NTAG216";
        case NFC_CARD_ISO14443_4: return "ISO 14443-4 (Smart Card)";
        case NFC_CARD_FELICA: return "FeliCa";
        default: return "Unknown";
    }
}

// ========== APDU Operations ==========

APDUResponse NFCInterface::sendAPDU(APDUCommand& cmd) {
    APDUResponse response;
    memset(&response, 0, sizeof(APDUResponse));

    // Build APDU
    uint8_t apdu[261];
    uint8_t apduLen = 0;

    apdu[apduLen++] = cmd.cla;
    apdu[apduLen++] = cmd.ins;
    apdu[apduLen++] = cmd.p1;
    apdu[apduLen++] = cmd.p2;

    if (cmd.lc > 0) {
        apdu[apduLen++] = cmd.lc;
        memcpy(&apdu[apduLen], cmd.data, cmd.lc);
        apduLen += cmd.lc;
    }

    if (cmd.hasLe) {
        apdu[apduLen++] = cmd.le;
    }

    return sendRawAPDU(apdu, apduLen);
}

APDUResponse NFCInterface::sendRawAPDU(uint8_t* apdu, uint16_t len) {
    APDUResponse response;
    memset(&response, 0, sizeof(APDUResponse));

    uint8_t responseBuffer[256];
    uint8_t responseLen = sizeof(responseBuffer);

    Serial.print(F("[NFC] APDU TX: "));
    for (int i = 0; i < len; i++) {
        Serial.printf("%02X ", apdu[i]);
    }
    Serial.println();

    if (!pn532->inDataExchange(apdu, len, responseBuffer, &responseLen)) {
        Serial.println(F("[NFC] APDU exchange failed"));
        response.success = false;
        return response;
    }

    if (responseLen < 2) {
        response.success = false;
        return response;
    }

    // Parse response - last 2 bytes are SW1/SW2
    response.dataLen = responseLen - 2;
    if (response.dataLen > 0) {
        memcpy(response.data, responseBuffer, response.dataLen);
    }
    response.sw1 = responseBuffer[responseLen - 2];
    response.sw2 = responseBuffer[responseLen - 1];
    response.success = (response.sw1 == 0x90 && response.sw2 == 0x00);

    Serial.print(F("[NFC] APDU RX: "));
    for (int i = 0; i < responseLen; i++) {
        Serial.printf("%02X ", responseBuffer[i]);
    }
    Serial.printf("(SW: %02X%02X)\n", response.sw1, response.sw2);

    return response;
}

APDUResponse NFCInterface::executeAPDU(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                                        uint8_t* data, uint8_t dataLen, uint8_t le) {
    APDUCommand cmd;
    cmd.cla = cla;
    cmd.ins = ins;
    cmd.p1 = p1;
    cmd.p2 = p2;
    cmd.lc = dataLen;
    if (dataLen > 0 && data != nullptr) {
        memcpy(cmd.data, data, dataLen);
    }
    cmd.le = le;
    cmd.hasLe = (le > 0);

    return sendAPDU(cmd);
}

APDUResponse NFCInterface::selectByAID(const uint8_t* aid, uint8_t aidLen) {
    return executeAPDU(APDU_CLA_ISO, APDU_INS_SELECT, SELECT_BY_NAME, SELECT_FIRST,
                       (uint8_t*)aid, aidLen, 0x00);
}

APDUResponse NFCInterface::readRecord(uint8_t sfi, uint8_t record) {
    // P2 = (SFI << 3) | 0x04 for "record number given in P1"
    uint8_t p2 = (sfi << 3) | 0x04;
    return executeAPDU(APDU_CLA_ISO, APDU_INS_READ_RECORD, record, p2, nullptr, 0, 0x00);
}

APDUResponse NFCInterface::getData(uint16_t tag) {
    uint8_t p1 = (tag >> 8) & 0xFF;
    uint8_t p2 = tag & 0xFF;
    return executeAPDU(APDU_CLA_ISO, APDU_INS_GET_DATA, p1, p2, nullptr, 0, 0x00);
}

APDUResponse NFCInterface::getProcessingOptions(uint8_t* pdol, uint8_t pdolLen) {
    // GPO command with PDOL data
    uint8_t data[256];
    uint8_t dataLen = 0;

    // Command template tag 0x83
    data[dataLen++] = 0x83;
    data[dataLen++] = pdolLen;
    if (pdolLen > 0 && pdol != nullptr) {
        memcpy(&data[dataLen], pdol, pdolLen);
        dataLen += pdolLen;
    }

    return executeAPDU(0x80, APDU_INS_GPO, 0x00, 0x00, data, dataLen, 0x00);
}

// ========== EMV Operations ==========

bool NFCInterface::selectApplication(const uint8_t* aid, uint8_t aidLen) {
    APDUResponse resp = selectByAID(aid, aidLen);
    return resp.success;
}

bool NFCInterface::selectPPSE() {
    // Select Payment System Environment (2PAY.SYS.DDF01)
    return selectApplication(AID_PPSE, sizeof(AID_PPSE));
}

bool NFCInterface::readEMV(EMVCardData& data) {
    memset(&data, 0, sizeof(EMVCardData));

    Serial.println(F("[NFC] Starting EMV read..."));

    // First, select PPSE to list available applications
    APDUResponse resp = selectByAID(AID_PPSE, sizeof(AID_PPSE));
    if (!resp.success) {
        Serial.println(F("[NFC] PPSE selection failed, trying direct AID selection"));

        // Try common AIDs directly
        const uint8_t* aids[] = {AID_VISA, AID_MASTERCARD, AID_AMEX, AID_DISCOVER};
        const uint8_t aidLens[] = {7, 7, 6, 7};
        const char* aidNames[] = {"Visa", "Mastercard", "Amex", "Discover"};

        bool found = false;
        for (int i = 0; i < 4; i++) {
            resp = selectByAID(aids[i], aidLens[i]);
            if (resp.success) {
                Serial.printf("[NFC] Selected %s application\n", aidNames[i]);
                strcpy(data.applicationLabel, aidNames[i]);
                memcpy(data.aid, aids[i], aidLens[i]);
                data.aidLen = aidLens[i];
                found = true;
                break;
            }
        }

        if (!found) {
            Serial.println(F("[NFC] No payment application found"));
            return false;
        }
    } else {
        // Parse PPSE response to find applications
        Serial.println(F("[NFC] PPSE selected, parsing applications..."));
        // The response contains FCI template with application list
        // For now, try selecting first common AID
        resp = selectByAID(AID_VISA, 7);
        if (!resp.success) {
            resp = selectByAID(AID_MASTERCARD, 7);
        }

        if (resp.success) {
            parseEMVResponse(resp.data, resp.dataLen, data);
        }
    }

    // Get Processing Options (GPO)
    Serial.println(F("[NFC] Sending GPO..."));
    resp = getProcessingOptions(nullptr, 0);
    if (resp.success) {
        Serial.println(F("[NFC] GPO successful"));
        // Parse AFL (Application File Locator) from response
        // Then read records specified by AFL
        readEMVRecords(data);
    } else {
        // Try without GPO - some cards allow direct record reading
        Serial.println(F("[NFC] GPO failed, trying direct record read"));
        readEMVRecords(data);
    }

    return data.panFound || data.applicationLabel[0] != '\0';
}

bool NFCInterface::readEMVRecords(EMVCardData& cardData) {
    Serial.println(F("[NFC] Reading EMV records..."));

    // Try reading common SFIs and record numbers
    // SFI 1-3, Records 1-5 typically contain card data
    for (uint8_t sfi = 1; sfi <= 3; sfi++) {
        for (uint8_t rec = 1; rec <= 5; rec++) {
            APDUResponse resp = readRecord(sfi, rec);
            if (resp.success && resp.dataLen > 0) {
                Serial.printf("[NFC] Record SFI %d Rec %d: %d bytes\n", sfi, rec, resp.dataLen);

                // Parse TLV data looking for interesting tags
                uint8_t value[64];
                uint8_t valueLen;

                // Look for PAN (tag 5A)
                if (parseTLV(resp.data, resp.dataLen, 0x5A, value, &valueLen)) {
                    // Convert BCD to ASCII
                    for (int i = 0; i < valueLen && i < 10; i++) {
                        cardData.pan[i * 2] = '0' + ((value[i] >> 4) & 0x0F);
                        cardData.pan[i * 2 + 1] = '0' + (value[i] & 0x0F);
                    }
                    // Remove trailing F padding
                    for (int i = strlen(cardData.pan) - 1; i >= 0; i--) {
                        if (cardData.pan[i] == 'F' || cardData.pan[i] == 'f') {
                            cardData.pan[i] = '\0';
                        } else {
                            break;
                        }
                    }
                    cardData.panFound = true;
                    Serial.printf("[NFC] Found PAN: %s\n", formatPAN(cardData.pan).c_str());
                }

                // Look for expiry date (tag 5F24)
                if (parseTLV(resp.data, resp.dataLen, 0x5F24, value, &valueLen)) {
                    sprintf(cardData.expiry, "%02X/%02X", value[1], value[0]); // MM/YY
                    cardData.expiryFound = true;
                    Serial.printf("[NFC] Found Expiry: %s\n", cardData.expiry);
                }

                // Look for cardholder name (tag 5F20)
                if (parseTLV(resp.data, resp.dataLen, 0x5F20, value, &valueLen)) {
                    memcpy(cardData.cardholderName, value, min((int)valueLen, 31));
                    cardData.nameFound = true;
                    Serial.printf("[NFC] Found Name: %s\n", cardData.cardholderName);
                }

                // Look for application label (tag 50)
                if (parseTLV(resp.data, resp.dataLen, 0x50, value, &valueLen)) {
                    memcpy(cardData.applicationLabel, value, min((int)valueLen, 31));
                    Serial.printf("[NFC] Found Label: %s\n", cardData.applicationLabel);
                }
            }
        }
    }

    return cardData.panFound;
}

bool NFCInterface::parseTLV(uint8_t* data, uint16_t len, uint16_t tag, uint8_t* value, uint8_t* valueLen) {
    uint16_t i = 0;

    while (i < len) {
        // Get tag
        uint16_t currentTag = data[i++];
        if ((currentTag & 0x1F) == 0x1F) {
            // Two-byte tag
            if (i >= len) return false;
            currentTag = (currentTag << 8) | data[i++];
        }

        if (i >= len) return false;

        // Get length
        uint16_t length = data[i++];
        if (length == 0x81) {
            if (i >= len) return false;
            length = data[i++];
        } else if (length == 0x82) {
            if (i + 1 >= len) return false;
            length = (data[i] << 8) | data[i + 1];
            i += 2;
        }

        if (i + length > len) return false;

        // Check if this is our tag
        if (currentTag == tag) {
            *valueLen = min((int)length, 64);
            memcpy(value, &data[i], *valueLen);
            return true;
        }

        // Check inside constructed tags (0x70, 0x77, 0x61, etc.)
        if (currentTag == 0x70 || currentTag == 0x77 || currentTag == 0x61 ||
            currentTag == 0x6F || currentTag == 0xA5) {
            if (parseTLV(&data[i], length, tag, value, valueLen)) {
                return true;
            }
        }

        i += length;
    }

    return false;
}

bool NFCInterface::listEMVApplications() {
    emvApps->clear();

    // Select PPSE
    APDUResponse resp = selectByAID(AID_PPSE, sizeof(AID_PPSE));
    if (!resp.success) {
        Serial.println(F("[NFC] PPSE not available"));
        return false;
    }

    // Parse response for application templates (tag 61)
    // This is simplified - full implementation would parse the complete FCI
    uint8_t value[64];
    uint8_t valueLen;

    // Look for application entries
    // Full implementation would iterate through all 61 templates

    return emvApps->size() > 0;
}

String NFCInterface::formatPAN(const char* pan) {
    String formatted = "";
    int len = strlen(pan);

    for (int i = 0; i < len; i++) {
        if (i > 0 && i < len - 4 && i % 4 == 0) {
            formatted += " ";
        }
        // Mask middle digits for privacy
        if (i >= 4 && i < len - 4) {
            formatted += "*";
        } else {
            formatted += pan[i];
        }
    }

    return formatted;
}

// ========== FeliCa Operations ==========

bool NFCInterface::readFeliCa(FeliCaCard& card) {
    memset(&card, 0, sizeof(FeliCaCard));

    uint8_t idm[8];
    uint8_t pmm[8];
    uint16_t systemCode;

    // Poll for FeliCa card (system code 0xFFFF = any)
    if (!pn532->felica_Polling(0xFFFF, 0x00, idm, pmm, &systemCode, 500)) {
        return false;
    }

    memcpy(card.idm, idm, 8);
    memcpy(card.pmm, pmm, 8);
    card.systemCode = systemCode;
    card.timestamp = millis();

    Serial.print(F("[NFC] FeliCa IDm: "));
    for (int i = 0; i < 8; i++) {
        Serial.printf("%02X ", idm[i]);
    }
    Serial.println();

    Serial.printf("[NFC] FeliCa System Code: %04X\n", systemCode);

    return true;
}

bool NFCInterface::readFeliCaBlock(uint8_t* idm, uint16_t serviceCode, uint8_t blockNum, uint8_t* data) {
    // Read without encryption (service code with RW permission)
    uint8_t blockList[2] = {0x80, blockNum}; // 2-byte block list element

    return pn532->felica_ReadWithoutEncryption(1, &serviceCode, 1, blockList, data);
}

// ========== Card Emulation ==========

bool NFCInterface::startEmulation(uint8_t* uid, uint8_t uidLen) {
    // PN532 supports card emulation mode
    // This requires specific configuration

    Serial.println(F("[NFC] Starting card emulation..."));
    Serial.println(F("[NFC] Note: Full emulation requires target mode configuration"));

    // For basic UID emulation, we need to set the PN532 into target mode
    // This is limited on the PN532 compared to something like a Proxmark

    // The Adafruit library doesn't fully support emulation mode
    // Full implementation would use low-level PN532 commands

    return false; // Not fully implemented
}

void NFCInterface::stopEmulation() {
    Serial.println(F("[NFC] Stopping emulation"));
    // Return to normal reader mode
    pn532->SAMConfig();
}

// ========== Display Methods ==========

void NFCInterface::displayCardInfo(NFCCardInfo& card) {
    if (!tft) return;

    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(MATRIX_GREEN, TFT_BLACK);

    tft->setCursor(10, 10);
    tft->setTextSize(2);
    tft->println(F("NFC Card Detected"));

    tft->setTextSize(1);
    tft->setCursor(10, 40);
    tft->print(F("Type: "));
    tft->println(cardTypeToString(card.cardType));

    tft->setCursor(10, 55);
    tft->print(F("UID: "));
    for (int i = 0; i < card.uidLength; i++) {
        tft->printf("%02X ", card.uid[i]);
    }

    tft->setCursor(10, 70);
    tft->print(F("ISO 14443-4: "));
    tft->println(card.iso14443_4 ? "Yes" : "No");

    if (card.iso14443_4) {
        tft->setCursor(10, 85);
        tft->setTextColor(MATRIX_CYAN, TFT_BLACK);
        tft->println(F("Smart card - EMV capable"));
    }
}

void NFCInterface::displayEMVData(EMVCardData& data) {
    if (!tft) return;

    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(MATRIX_GREEN, TFT_BLACK);

    tft->setCursor(10, 10);
    tft->setTextSize(2);
    tft->println(F("EMV Card Data"));

    tft->setTextSize(1);

    if (data.applicationLabel[0] != '\0') {
        tft->setCursor(10, 40);
        tft->print(F("App: "));
        tft->println(data.applicationLabel);
    }

    if (data.panFound) {
        tft->setCursor(10, 55);
        tft->print(F("PAN: "));
        tft->println(formatPAN(data.pan));
    }

    if (data.expiryFound) {
        tft->setCursor(10, 70);
        tft->print(F("Exp: "));
        tft->println(data.expiry);
    }

    if (data.nameFound) {
        tft->setCursor(10, 85);
        tft->print(F("Name: "));
        tft->println(data.cardholderName);
    }

    // Security notice
    tft->setCursor(10, 110);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->println(F("Note: Dynamic auth prevents cloning"));
}

void NFCInterface::displayFeliCaInfo(FeliCaCard& card) {
    if (!tft) return;

    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(MATRIX_GREEN, TFT_BLACK);

    tft->setCursor(10, 10);
    tft->setTextSize(2);
    tft->println(F("FeliCa Card"));

    tft->setTextSize(1);
    tft->setCursor(10, 40);
    tft->print(F("IDm: "));
    for (int i = 0; i < 8; i++) {
        tft->printf("%02X", card.idm[i]);
    }

    tft->setCursor(10, 55);
    tft->print(F("PMm: "));
    for (int i = 0; i < 8; i++) {
        tft->printf("%02X", card.pmm[i]);
    }

    tft->setCursor(10, 70);
    tft->printf("System: %04X", card.systemCode);
}

void NFCInterface::displayAPDUResponse(APDUResponse& resp) {
    if (!tft) return;

    tft->setTextColor(resp.success ? MATRIX_GREEN : TFT_RED, TFT_BLACK);
    tft->printf("SW: %02X%02X ", resp.sw1, resp.sw2);

    if (resp.dataLen > 0) {
        tft->printf("(%d bytes)", resp.dataLen);
    }
    tft->println();
}

void NFCInterface::drawProgressBar(int x, int y, int w, int h, int progress) {
    if (!tft) return;

    tft->drawRect(x, y, w, h, MATRIX_GREEN);
    int fillWidth = (w - 2) * progress / 100;
    tft->fillRect(x + 1, y + 1, fillWidth, h - 2, MATRIX_GREEN);
}

// ========== Storage Operations ==========

String NFCInterface::generateFilename(NFCCardInfo& card) {
    String filename = "/nfc/card_";
    for (int i = 0; i < card.uidLength; i++) {
        filename += String(card.uid[i], HEX);
    }
    filename += ".json";
    return filename;
}

bool NFCInterface::saveCard(NFCCardInfo& card, const char* filename) {
    // Ensure directory exists
    if (!SD.exists("/nfc")) {
        SD.mkdir("/nfc");
    }

    String fname = filename ? String(filename) : generateFilename(card);

    File file = SD.open(fname, FILE_WRITE);
    if (!file) {
        Serial.println(F("[NFC] Failed to create file"));
        return false;
    }

    StaticJsonDocument<1024> doc;

    // UID
    String uidStr = "";
    for (int i = 0; i < card.uidLength; i++) {
        if (i > 0) uidStr += ":";
        uidStr += String(card.uid[i], HEX);
    }
    doc["uid"] = uidStr;
    doc["uid_length"] = card.uidLength;
    doc["card_type"] = (int)card.cardType;
    doc["card_type_name"] = cardTypeToString(card.cardType);
    doc["iso14443_4"] = card.iso14443_4;
    doc["timestamp"] = card.timestamp;

    // EMV data if present
    if (card.emvData.panFound || card.emvData.applicationLabel[0] != '\0') {
        JsonObject emv = doc.createNestedObject("emv");
        if (card.emvData.applicationLabel[0] != '\0') {
            emv["app_label"] = card.emvData.applicationLabel;
        }
        if (card.emvData.panFound) {
            emv["pan_masked"] = formatPAN(card.emvData.pan);
        }
        if (card.emvData.expiryFound) {
            emv["expiry"] = card.emvData.expiry;
        }
        if (card.emvData.nameFound) {
            emv["cardholder"] = card.emvData.cardholderName;
        }
    }

    serializeJsonPretty(doc, file);
    file.close();

    Serial.printf("[NFC] Saved card to %s\n", fname.c_str());
    return true;
}

bool NFCInterface::loadCard(const char* filename, NFCCardInfo& card) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println(F("[NFC] JSON parse error"));
        return false;
    }

    memset(&card, 0, sizeof(NFCCardInfo));

    // Parse UID
    String uidStr = doc["uid"];
    int idx = 0;
    int uidIdx = 0;
    while (idx < uidStr.length() && uidIdx < 10) {
        int colonPos = uidStr.indexOf(':', idx);
        if (colonPos < 0) colonPos = uidStr.length();
        String byteStr = uidStr.substring(idx, colonPos);
        card.uid[uidIdx++] = strtol(byteStr.c_str(), NULL, 16);
        idx = colonPos + 1;
    }
    card.uidLength = doc["uid_length"];
    card.cardType = (NFCCardType)(int)doc["card_type"];
    card.iso14443_4 = doc["iso14443_4"];
    card.timestamp = doc["timestamp"];

    return true;
}

bool NFCInterface::exportAPDULog(const char* filename) {
    // Export APDU command/response log for analysis
    // Would need to implement logging during APDU exchange
    return false;
}

// ========== Utility ==========

void NFCInterface::clearScanned() {
    scannedCards->clear();
    emvApps->clear();
}

String NFCInterface::getStatusString() {
    if (!initialized) return "Not initialized";
    if (scanning) {
        switch (currentMode) {
            case NFC_SCAN_READ: return "Reading cards...";
            case NFC_SCAN_EMV: return "EMV scan...";
            case NFC_SCAN_FELICA: return "FeliCa scan...";
            case NFC_SCAN_POLL: return "Polling...";
            case NFC_SCAN_DUMP: return "Full dump...";
            default: return "Scanning...";
        }
    }
    return "Ready";
}

void NFCInterface::buildMenu(LinkedList<String>* menuItems) {
    menuItems->add("Read Card");
    menuItems->add("EMV Info");
    menuItems->add("FeliCa Read");
    menuItems->add("APDU Shell");
    menuItems->add("Card Dump");
    menuItems->add("Saved Cards");
}

String NFCInterface::getMenuTitle() {
    return "NFC (PN532)";
}

#endif // HAS_PN532

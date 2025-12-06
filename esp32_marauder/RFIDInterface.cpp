/*
 * RFIDInterface.cpp
 * RFID/NFC Security Auditing Module Implementation
 *
 * For authorized penetration testing only.
 */

#include "RFIDInterface.h"

#ifdef HAS_RFID

RFIDInterface rfid_obj;

// ============================================================================
// Lifecycle Methods
// ============================================================================

void RFIDInterface::RunSetup() {
    Serial.println(F("[RFID] Initializing RFID security auditor..."));

    // Initialize SPI for RFID (shares bus with display/SD)
    mfrc522 = new MFRC522(RFID_CS_PIN, RFID_RST_PIN);
    mfrc522->PCD_Init();

    // Verify reader is responding
    byte version = mfrc522->PCD_ReadRegister(mfrc522->VersionReg);
    if (version == 0x00 || version == 0xFF) {
        Serial.println(F("[RFID] ERROR: Reader not detected!"));
        return;
    }

    Serial.print(F("[RFID] Reader detected, firmware version: 0x"));
    Serial.println(version, HEX);

    // Initialize state
    currentMode = RFID_SCAN_OFF;
    cardPresent = false;
    monitoring = false;
    lastScanTime = 0;
    scanInterval = 250;  // 250ms between scans

    // Initialize data structures
    scannedCards = new LinkedList<CardData>();
    auditLog = new LinkedList<AuditEntry>();
    foundKeys = new LinkedList<SectorKey>();

    // Clear last card
    memset(&lastCard, 0, sizeof(CardData));

    // Statistics
    totalCardsRead = 0;
    vulnerableCards = 0;
    cloneAttempts = 0;

    // Set antenna gain to maximum
    mfrc522->PCD_SetAntennaGain(mfrc522->RxGain_max);

    Serial.println(F("[RFID] Security auditor ready"));
    logAction("INIT", "", "SUCCESS", "RFID module initialized");
}

void RFIDInterface::main(uint32_t currentTime) {
    if (currentMode == RFID_SCAN_OFF) {
        return;
    }

    // Rate limiting
    if (currentTime - lastScanTime < scanInterval) {
        return;
    }
    lastScanTime = currentTime;

    switch (currentMode) {
        case RFID_SCAN_READ:
            if (detectCard()) {
                CardData card;
                if (readCard(&card)) {
                    totalCardsRead++;
                    lastCard = card;
                    logAction("READ", getUIDString(card.uid, card.uidLength),
                             "SUCCESS", "Card type: " + card.cardTypeName);
                }
                haltCard();
            }
            break;

        case RFID_SCAN_FULL_DUMP:
            if (detectCard()) {
                CardData card;
                if (readCard(&card)) {
                    readFullDump(&card);
                    lastCard = card;
                    logAction("DUMP", getUIDString(card.uid, card.uidLength),
                             card.fullDumpAvailable ? "SUCCESS" : "PARTIAL",
                             String(card.sectorsReadable) + " sectors read");
                }
                haltCard();
            }
            break;

        case RFID_SCAN_KEY_TEST:
            if (detectCard()) {
                CardData card;
                if (readCard(&card)) {
                    uint8_t keysFound = testDefaultKeys(&card, foundKeys);
                    card.defaultKeysFound = keysFound;
                    if (keysFound > 0) {
                        vulnerableCards++;
                        card.secLevel = SEC_LOW;
                    }
                    lastCard = card;
                    logAction("KEY_TEST", getUIDString(card.uid, card.uidLength),
                             keysFound > 0 ? "VULNERABLE" : "SECURE",
                             String(keysFound) + " default keys found");
                }
                haltCard();
            }
            break;

        case RFID_SCAN_CONTINUOUS:
            if (detectCard()) {
                CardData card;
                if (readCard(&card)) {
                    // Check if this is a new card
                    bool isNew = true;
                    for (int i = 0; i < scannedCards->size(); i++) {
                        CardData existing = scannedCards->get(i);
                        if (memcmp(existing.uid, card.uid, card.uidLength) == 0) {
                            isNew = false;
                            // Update last seen
                            existing.lastSeen = currentTime;
                            existing.readCount++;
                            break;
                        }
                    }
                    if (isNew) {
                        card.firstSeen = currentTime;
                        card.lastSeen = currentTime;
                        card.readCount = 1;
                        scannedCards->add(card);
                        totalCardsRead++;
                        logAction("DISCOVERED", getUIDString(card.uid, card.uidLength),
                                 "NEW", card.cardTypeName);
                    }
                    lastCard = card;
                }
                haltCard();
            }
            break;

        case RFID_SCAN_ANALYZE:
            if (detectCard()) {
                CardData card;
                if (readCard(&card)) {
                    // Comprehensive security analysis
                    testDefaultKeys(&card, foundKeys);
                    card.secLevel = assessSecurity(&card);
                    card.uidChangeable = detectMagicCard();
                    readFullDump(&card);
                    lastCard = card;
                    logAction("ANALYZE", getUIDString(card.uid, card.uidLength),
                             getSecurityLevelName(card.secLevel),
                             generateReport(&card));
                }
                haltCard();
            }
            break;

        default:
            break;
    }
}

void RFIDInterface::shutdown() {
    currentMode = RFID_SCAN_OFF;
    monitoring = false;

    // Cleanup
    delete scannedCards;
    delete auditLog;
    delete foundKeys;
    delete mfrc522;

    Serial.println(F("[RFID] Module shutdown"));
}

// ============================================================================
// Card Detection & Reading
// ============================================================================

bool RFIDInterface::detectCard() {
    // Check for new card
    if (!mfrc522->PICC_IsNewCardPresent()) {
        cardPresent = false;
        return false;
    }

    // Select the card
    if (!mfrc522->PICC_ReadCardSerial()) {
        cardPresent = false;
        return false;
    }

    cardPresent = true;
    return true;
}

bool RFIDInterface::readCard(CardData* card) {
    if (!cardPresent) {
        return false;
    }

    // Clear the structure
    memset(card, 0, sizeof(CardData));

    // Copy UID
    card->uidLength = mfrc522->uid.size;
    memcpy(card->uid, mfrc522->uid.uidByte, card->uidLength);

    // Copy SAK and ATQA
    card->sak = mfrc522->uid.sak;
    // ATQA is available after REQA/WUPA

    // Identify card type
    card->cardType = identifyCardType(card->sak, card->atqa);
    card->cardTypeName = getCardTypeName(card->cardType);

    // Get manufacturer from UID
    card->manufacturer = getManufacturer(card->uid);

    // Determine memory size and sectors
    switch (card->cardType) {
        case CARD_MIFARE_MINI:
            card->memorySize = 320;
            card->numSectors = 5;
            break;
        case CARD_MIFARE_1K:
            card->memorySize = 1024;
            card->numSectors = 16;
            break;
        case CARD_MIFARE_4K:
            card->memorySize = 4096;
            card->numSectors = 40;
            break;
        case CARD_MIFARE_ULTRALIGHT:
            card->memorySize = 64;
            card->numSectors = 0;  // Pages, not sectors
            break;
        case CARD_NTAG_213:
            card->memorySize = 144;
            card->numSectors = 0;
            break;
        case CARD_NTAG_215:
            card->memorySize = 504;
            card->numSectors = 0;
            break;
        case CARD_NTAG_216:
            card->memorySize = 888;
            card->numSectors = 0;
            break;
        default:
            card->memorySize = 0;
            card->numSectors = 0;
            break;
    }

    // Initial security assessment based on card type
    if (card->cardType == CARD_MIFARE_DESFIRE ||
        card->cardType == CARD_MIFARE_DESFIRE_EV1 ||
        card->cardType == CARD_MIFARE_DESFIRE_EV2) {
        card->secLevel = SEC_HIGH;
    } else if (card->cardType == CARD_MIFARE_1K ||
               card->cardType == CARD_MIFARE_4K ||
               card->cardType == CARD_MIFARE_MINI) {
        card->secLevel = SEC_MEDIUM;  // Until we test keys
    } else if (card->cardType == CARD_MIFARE_ULTRALIGHT) {
        card->secLevel = SEC_CRITICAL;  // No encryption
    } else {
        card->secLevel = SEC_UNKNOWN;
    }

    Serial.print(F("[RFID] Card detected: "));
    Serial.print(card->cardTypeName);
    Serial.print(F(" UID: "));
    Serial.println(getUIDString(card->uid, card->uidLength));

    return true;
}

bool RFIDInterface::readFullDump(CardData* card) {
    if (card->numSectors == 0) {
        // NTAG/Ultralight - read pages
        // TODO: Implement page reading
        return false;
    }

    // Allocate memory for sector data
    uint16_t dataSize = card->numSectors * 64;  // Max 64 bytes per sector
    card->sectorData = (uint8_t*)malloc(dataSize);
    if (!card->sectorData) {
        Serial.println(F("[RFID] Memory allocation failed"));
        return false;
    }
    memset(card->sectorData, 0, dataSize);

    uint8_t sectorsRead = 0;
    uint8_t buffer[18];  // 16 bytes + 2 CRC

    // Try to read each sector
    for (uint8_t sector = 0; sector < card->numSectors; sector++) {
        bool sectorReadable = false;

        // Try default keys first
        for (uint8_t k = 0; k < NUM_DEFAULT_KEYS && !sectorReadable; k++) {
            uint8_t key[6];
            memcpy_P(key, DEFAULT_KEYS[k], 6);

            // Try key A
            if (authenticateSector(sector, key, true)) {
                sectorReadable = true;
            }
            // Try key B if key A failed
            else if (authenticateSector(sector, key, false)) {
                sectorReadable = true;
            }
        }

        if (sectorReadable) {
            // Read all blocks in this sector
            uint8_t firstBlock = getFirstBlockOfSector(sector);
            uint8_t numBlocks = getNumBlocksInSector(sector);

            for (uint8_t b = 0; b < numBlocks; b++) {
                uint8_t block = firstBlock + b;
                if (readBlock(block, buffer)) {
                    // Store in sector data
                    memcpy(&card->sectorData[sector * 64 + b * 16], buffer, 16);
                }
            }
            sectorsRead++;
        }
    }

    card->sectorsReadable = sectorsRead;
    card->fullDumpAvailable = (sectorsRead == card->numSectors);

    Serial.print(F("[RFID] Dump complete: "));
    Serial.print(sectorsRead);
    Serial.print(F("/"));
    Serial.print(card->numSectors);
    Serial.println(F(" sectors readable"));

    return card->fullDumpAvailable;
}

// ============================================================================
// Security Testing
// ============================================================================

uint8_t RFIDInterface::testDefaultKeys(CardData* card, LinkedList<SectorKey>* keys) {
    if (card->numSectors == 0) {
        return 0;
    }

    uint8_t totalKeysFound = 0;
    keys->clear();

    Serial.println(F("[RFID] Testing default keys..."));

    for (uint8_t sector = 0; sector < card->numSectors; sector++) {
        SectorKey sk;
        sk.sector = sector;
        sk.keyAFound = false;
        sk.keyBFound = false;

        for (uint8_t k = 0; k < NUM_DEFAULT_KEYS; k++) {
            uint8_t key[6];
            memcpy_P(key, DEFAULT_KEYS[k], 6);

            // Test key A
            if (!sk.keyAFound && authenticateSector(sector, key, true)) {
                memcpy(sk.keyA, key, 6);
                sk.keyAFound = true;
                totalKeysFound++;
                Serial.print(F("[RFID] Sector "));
                Serial.print(sector);
                Serial.print(F(" KeyA: "));
                Serial.println(bytesToHex(key, 6));
            }

            // Test key B
            if (!sk.keyBFound && authenticateSector(sector, key, false)) {
                memcpy(sk.keyB, key, 6);
                sk.keyBFound = true;
                totalKeysFound++;
                Serial.print(F("[RFID] Sector "));
                Serial.print(sector);
                Serial.print(F(" KeyB: "));
                Serial.println(bytesToHex(key, 6));
            }

            // Both keys found for this sector
            if (sk.keyAFound && sk.keyBFound) {
                break;
            }
        }

        if (sk.keyAFound || sk.keyBFound) {
            keys->add(sk);
        }
    }

    return totalKeysFound;
}

bool RFIDInterface::testSectorKey(uint8_t sector, uint8_t* key, bool keyA) {
    return authenticateSector(sector, key, keyA);
}

SecurityLevel RFIDInterface::assessSecurity(CardData* card) {
    // Card type-based initial assessment
    if (card->cardType == CARD_MIFARE_DESFIRE ||
        card->cardType == CARD_MIFARE_DESFIRE_EV1 ||
        card->cardType == CARD_MIFARE_DESFIRE_EV2) {
        return SEC_HIGH;  // AES encryption
    }

    if (card->cardType == CARD_MIFARE_ULTRALIGHT) {
        return SEC_CRITICAL;  // No encryption at all
    }

    // For MIFARE Classic, check key vulnerability
    if (card->defaultKeysFound > 0) {
        if (card->defaultKeysFound >= card->numSectors) {
            return SEC_CRITICAL;  // All sectors use default keys
        } else if (card->defaultKeysFound > card->numSectors / 2) {
            return SEC_LOW;  // More than half use default keys
        } else {
            return SEC_MEDIUM;  // Some default keys found
        }
    }

    // Check if magic card (UID changeable)
    if (card->uidChangeable) {
        return SEC_CRITICAL;  // Can be fully cloned
    }

    // If we couldn't read any sectors with default keys
    if (card->sectorsReadable == 0 && card->numSectors > 0) {
        return SEC_HIGH;  // Properly configured
    }

    return SEC_MEDIUM;
}

bool RFIDInterface::detectMagicCard() {
    // Magic cards (Chinese clones) respond to special commands
    // that allow UID modification

    // Attempt to unlock with Gen1a magic card sequence
    mfrc522->PCD_StopCrypto1();

    // Send HALT
    byte atqa[2];
    mfrc522->PICC_WakeupA(atqa, &atqa[0]);

    // Try magic backdoor command (0x40, 0x43)
    byte cmd = 0x40;
    byte response;
    byte responseLen = 1;

    // This is a simplified check - full implementation would
    // require raw bit-level communication
    // For now, we mark as unknown
    return false;
}

// ============================================================================
// Card Cloning (for authorized testing only)
// ============================================================================

bool RFIDInterface::writeCard(CardData* source, bool writeUID) {
    if (!source->fullDumpAvailable) {
        Serial.println(F("[RFID] Cannot write: source dump incomplete"));
        logAction("WRITE", getUIDString(source->uid, source->uidLength),
                 "FAILED", "Incomplete source dump");
        return false;
    }

    // Detect target card
    if (!detectCard()) {
        Serial.println(F("[RFID] No target card detected"));
        return false;
    }

    CardData target;
    if (!readCard(&target)) {
        return false;
    }

    // Verify compatible card types
    if (target.cardType != source->cardType) {
        Serial.println(F("[RFID] Card type mismatch"));
        logAction("WRITE", getUIDString(target.uid, target.uidLength),
                 "FAILED", "Type mismatch");
        return false;
    }

    cloneAttempts++;

    // If writing UID, need magic card
    if (writeUID) {
        if (!unlockMagicCard()) {
            Serial.println(F("[RFID] Target is not a magic card, cannot write UID"));
            logAction("WRITE", getUIDString(target.uid, target.uidLength),
                     "FAILED", "Not a magic card");
            return false;
        }

        if (!writeUID(source->uid, source->uidLength)) {
            Serial.println(F("[RFID] UID write failed"));
            return false;
        }
    }

    // Write sector data
    uint8_t sectorsWritten = 0;
    for (uint8_t sector = 0; sector < source->numSectors; sector++) {
        // Skip sector 0 block 0 if not writing UID (manufacturer block)
        uint8_t startBlock = (sector == 0 && !writeUID) ? 1 : 0;
        uint8_t firstBlock = getFirstBlockOfSector(sector);
        uint8_t numBlocks = getNumBlocksInSector(sector);

        // Need valid key to write
        bool authenticated = false;
        for (uint8_t k = 0; k < NUM_DEFAULT_KEYS && !authenticated; k++) {
            uint8_t key[6];
            memcpy_P(key, DEFAULT_KEYS[k], 6);
            authenticated = authenticateSector(sector, key, true);
        }

        if (!authenticated) {
            Serial.print(F("[RFID] Cannot authenticate sector "));
            Serial.println(sector);
            continue;
        }

        bool sectorOK = true;
        for (uint8_t b = startBlock; b < numBlocks - 1; b++) {  // Skip trailer block
            uint8_t block = firstBlock + b;
            uint8_t* blockData = &source->sectorData[sector * 64 + b * 16];

            if (!writeBlock(block, blockData)) {
                sectorOK = false;
                Serial.print(F("[RFID] Write failed at block "));
                Serial.println(block);
            }
        }

        if (sectorOK) {
            sectorsWritten++;
        }
    }

    bool success = (sectorsWritten == source->numSectors);
    logAction("WRITE", getUIDString(target.uid, target.uidLength),
             success ? "SUCCESS" : "PARTIAL",
             String(sectorsWritten) + "/" + String(source->numSectors) + " sectors");

    haltCard();
    return success;
}

bool RFIDInterface::verifyWrite(CardData* source) {
    if (!detectCard()) {
        return false;
    }

    CardData target;
    readFullDump(&target);

    // Compare data
    bool match = true;
    for (uint16_t i = 0; i < source->memorySize && match; i++) {
        // Skip manufacturer block
        if (i < 16) continue;
        // Skip sector trailers
        if ((i % 64) >= 48 && (i % 64) < 64) continue;

        if (source->sectorData[i] != target.sectorData[i]) {
            match = false;
        }
    }

    haltCard();
    return match;
}

// ============================================================================
// Card Type Identification
// ============================================================================

RFIDCardType RFIDInterface::identifyCardType(uint8_t sak, uint8_t* atqa) {
    // SAK (Select Acknowledge) byte determines card type
    // Reference: AN10833 - MIFARE Type Identification Procedure

    switch (sak) {
        case 0x00:
            return CARD_MIFARE_ULTRALIGHT;  // Or NTAG
        case 0x08:
            return CARD_MIFARE_1K;          // Classic 1K
        case 0x09:
            return CARD_MIFARE_MINI;
        case 0x18:
            return CARD_MIFARE_4K;          // Classic 4K
        case 0x10:
        case 0x11:
            return CARD_MIFARE_PLUS_2K;
        case 0x20:
            // Could be DESFire or Plus in SL3
            return CARD_MIFARE_DESFIRE;
        case 0x28:
            return CARD_MIFARE_1K;          // Emulated
        case 0x38:
            return CARD_MIFARE_4K;          // Emulated
        default:
            if (sak & 0x04) {
                return CARD_ISO14443_4;     // ISO 14443-4 compliant
            }
            return CARD_UNKNOWN;
    }
}

String RFIDInterface::getCardTypeName(RFIDCardType type) {
    switch (type) {
        case CARD_MIFARE_MINI:         return "MIFARE Mini";
        case CARD_MIFARE_1K:           return "MIFARE Classic 1K";
        case CARD_MIFARE_4K:           return "MIFARE Classic 4K";
        case CARD_MIFARE_ULTRALIGHT:   return "MIFARE Ultralight";
        case CARD_MIFARE_ULTRALIGHT_C: return "MIFARE Ultralight C";
        case CARD_MIFARE_PLUS_2K:      return "MIFARE Plus 2K";
        case CARD_MIFARE_PLUS_4K:      return "MIFARE Plus 4K";
        case CARD_MIFARE_DESFIRE:      return "MIFARE DESFire";
        case CARD_MIFARE_DESFIRE_EV1:  return "MIFARE DESFire EV1";
        case CARD_MIFARE_DESFIRE_EV2:  return "MIFARE DESFire EV2";
        case CARD_NTAG_213:            return "NTAG213";
        case CARD_NTAG_215:            return "NTAG215";
        case CARD_NTAG_216:            return "NTAG216";
        case CARD_ISO14443_4:          return "ISO 14443-4";
        case CARD_ISO18092:            return "ISO 18092 (NFC)";
        default:                       return "Unknown";
    }
}

String RFIDInterface::getManufacturer(uint8_t* uid) {
    // First byte of UID is manufacturer code (IC Manufacturer Code)
    // Reference: ISO/IEC 7816-6
    switch (uid[0]) {
        case 0x04: return "NXP Semiconductors";
        case 0x05: return "Infineon";
        case 0x16: return "Texas Instruments";
        case 0x2A: return "STMicroelectronics";
        case 0x34: return "Atmel";
        case 0x88: return "Infineon (legacy)";
        default:   return "Unknown (0x" + String(uid[0], HEX) + ")";
    }
}

String RFIDInterface::getSecurityLevelName(SecurityLevel level) {
    switch (level) {
        case SEC_CRITICAL: return "CRITICAL - Easily Cloned";
        case SEC_LOW:      return "LOW - Default Keys Found";
        case SEC_MEDIUM:   return "MEDIUM - Partial Protection";
        case SEC_HIGH:     return "HIGH - Strong Encryption";
        default:           return "UNKNOWN";
    }
}

// ============================================================================
// Report Generation
// ============================================================================

String RFIDInterface::generateReport(CardData* card) {
    String report = "\n";
    report += "═══════════════════════════════════════\n";
    report += "       RFID SECURITY AUDIT REPORT      \n";
    report += "═══════════════════════════════════════\n\n";

    report += "CARD IDENTIFICATION\n";
    report += "───────────────────\n";
    report += "  UID:          " + getUIDString(card->uid, card->uidLength) + "\n";
    report += "  Type:         " + card->cardTypeName + "\n";
    report += "  Manufacturer: " + card->manufacturer + "\n";
    report += "  SAK:          0x" + String(card->sak, HEX) + "\n";
    report += "  Memory:       " + String(card->memorySize) + " bytes\n";
    if (card->numSectors > 0) {
        report += "  Sectors:      " + String(card->numSectors) + "\n";
    }
    report += "\n";

    report += "SECURITY ASSESSMENT\n";
    report += "───────────────────\n";
    report += "  Level:        " + getSecurityLevelName(card->secLevel) + "\n";
    report += "  Default Keys: " + String(card->defaultKeysFound) + " found\n";
    report += "  Readable:     " + String(card->sectorsReadable) + "/" +
              String(card->numSectors) + " sectors\n";
    report += "  UID Locked:   " + String(card->uidChangeable ? "NO (Magic)" : "YES") + "\n";
    report += "\n";

    // Vulnerability summary
    report += "VULNERABILITIES\n";
    report += "───────────────\n";

    if (card->secLevel == SEC_CRITICAL) {
        report += "  [!] CRITICAL: Card can be fully cloned\n";
        if (card->cardType == CARD_MIFARE_ULTRALIGHT) {
            report += "      - No encryption (Ultralight)\n";
        }
        if (card->defaultKeysFound == card->numSectors * 2) {
            report += "      - All sectors use default keys\n";
        }
        if (card->uidChangeable) {
            report += "      - UID is changeable (magic card)\n";
        }
    } else if (card->secLevel == SEC_LOW) {
        report += "  [!] LOW: Multiple default keys found\n";
        report += "      - " + String(card->defaultKeysFound) + " sectors vulnerable\n";
    } else if (card->secLevel == SEC_MEDIUM) {
        report += "  [*] MEDIUM: Some protection in place\n";
        if (card->defaultKeysFound > 0) {
            report += "      - " + String(card->defaultKeysFound) + " default keys\n";
        }
    } else if (card->secLevel == SEC_HIGH) {
        report += "  [+] HIGH: Strong protection detected\n";
    }
    report += "\n";

    // Recommendations
    report += "RECOMMENDATIONS\n";
    report += "───────────────\n";

    if (card->cardType == CARD_MIFARE_1K || card->cardType == CARD_MIFARE_4K) {
        if (card->defaultKeysFound > 0) {
            report += "  • Change all default keys immediately\n";
            report += "  • Use unique keys per sector\n";
        }
        report += "  • Consider upgrading to MIFARE DESFire\n";
        report += "  • MIFARE Classic Crypto-1 is broken\n";
    } else if (card->cardType == CARD_MIFARE_ULTRALIGHT) {
        report += "  • This card type has no encryption\n";
        report += "  • Not suitable for access control\n";
        report += "  • Consider MIFARE DESFire or Plus\n";
    } else if (card->cardType == CARD_MIFARE_DESFIRE ||
               card->cardType == CARD_MIFARE_DESFIRE_EV1 ||
               card->cardType == CARD_MIFARE_DESFIRE_EV2) {
        report += "  • Good choice of card technology\n";
        report += "  • Ensure AES keys are properly managed\n";
        report += "  • Enable authentication logging\n";
    }

    report += "\n═══════════════════════════════════════\n";
    report += "        END OF SECURITY REPORT         \n";
    report += "═══════════════════════════════════════\n";

    return report;
}

String RFIDInterface::generatePentestReport(LinkedList<CardData>* cards) {
    String report = "\n";
    report += "╔═══════════════════════════════════════╗\n";
    report += "║   RFID PENETRATION TEST SUMMARY       ║\n";
    report += "╚═══════════════════════════════════════╝\n\n";

    report += "STATISTICS\n";
    report += "──────────\n";
    report += "  Cards Scanned:    " + String(cards->size()) + "\n";
    report += "  Vulnerable Cards: " + String(vulnerableCards) + "\n";
    report += "  Clone Attempts:   " + String(cloneAttempts) + "\n\n";

    // Summary by security level
    uint8_t critical = 0, low = 0, medium = 0, high = 0;
    for (int i = 0; i < cards->size(); i++) {
        CardData c = cards->get(i);
        switch (c.secLevel) {
            case SEC_CRITICAL: critical++; break;
            case SEC_LOW:      low++; break;
            case SEC_MEDIUM:   medium++; break;
            case SEC_HIGH:     high++; break;
            default: break;
        }
    }

    report += "SECURITY DISTRIBUTION\n";
    report += "─────────────────────\n";
    report += "  CRITICAL: " + String(critical) + "\n";
    report += "  LOW:      " + String(low) + "\n";
    report += "  MEDIUM:   " + String(medium) + "\n";
    report += "  HIGH:     " + String(high) + "\n\n";

    // Individual card summaries
    report += "CARD DETAILS\n";
    report += "────────────\n";
    for (int i = 0; i < cards->size(); i++) {
        CardData c = cards->get(i);
        report += "  " + String(i + 1) + ". " + getUIDString(c.uid, c.uidLength) + "\n";
        report += "     Type: " + c.cardTypeName + "\n";
        report += "     Risk: " + getSecurityLevelName(c.secLevel) + "\n\n";
    }

    return report;
}

// ============================================================================
// Storage & Audit
// ============================================================================

bool RFIDInterface::saveCardToSD(CardData* card, String filename) {
    #ifdef HAS_SD
    // Create JSON document
    DynamicJsonDocument doc(4096);

    doc["uid"] = getUIDString(card->uid, card->uidLength);
    doc["uidLength"] = card->uidLength;
    doc["sak"] = card->sak;
    doc["cardType"] = (int)card->cardType;
    doc["cardTypeName"] = card->cardTypeName;
    doc["manufacturer"] = card->manufacturer;
    doc["memorySize"] = card->memorySize;
    doc["numSectors"] = card->numSectors;
    doc["securityLevel"] = (int)card->secLevel;
    doc["defaultKeysFound"] = card->defaultKeysFound;
    doc["sectorsReadable"] = card->sectorsReadable;

    // Save sector data as base64 if available
    if (card->fullDumpAvailable && card->sectorData) {
        // For simplicity, save as hex string
        String dataHex = "";
        for (uint16_t i = 0; i < card->memorySize; i++) {
            if (card->sectorData[i] < 0x10) dataHex += "0";
            dataHex += String(card->sectorData[i], HEX);
        }
        doc["sectorData"] = dataHex;
    }

    // Write to file
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        return false;
    }
    serializeJson(doc, file);
    file.close();

    logAction("SAVE", getUIDString(card->uid, card->uidLength),
             "SUCCESS", "Saved to " + filename);
    return true;
    #else
    return false;
    #endif
}

void RFIDInterface::logAction(String action, String uid, String result, String details) {
    AuditEntry entry;
    entry.timestamp = millis();
    entry.action = action;
    entry.uid = uid;
    entry.result = result;
    entry.details = details;

    auditLog->add(entry);

    // Also log to serial
    Serial.print(F("[RFID AUDIT] "));
    Serial.print(action);
    Serial.print(F(" | "));
    Serial.print(uid);
    Serial.print(F(" | "));
    Serial.print(result);
    Serial.print(F(" | "));
    Serial.println(details);
}

String RFIDInterface::getAuditLog() {
    String log = "RFID AUDIT LOG\n";
    log += "══════════════\n\n";

    for (int i = 0; i < auditLog->size(); i++) {
        AuditEntry e = auditLog->get(i);
        log += "[" + String(e.timestamp / 1000) + "s] ";
        log += e.action + " | ";
        log += e.uid + " | ";
        log += e.result + "\n";
        if (e.details.length() > 0) {
            log += "    " + e.details + "\n";
        }
    }

    return log;
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool RFIDInterface::authenticateSector(uint8_t sector, uint8_t* key, bool keyA) {
    uint8_t trailerBlock = getFirstBlockOfSector(sector) + getNumBlocksInSector(sector) - 1;

    MFRC522::MIFARE_Key mfkey;
    memcpy(mfkey.keyByte, key, 6);

    MFRC522::StatusCode status;
    if (keyA) {
        status = mfrc522->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                            trailerBlock, &mfkey, &(mfrc522->uid));
    } else {
        status = mfrc522->PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B,
                                            trailerBlock, &mfkey, &(mfrc522->uid));
    }

    return (status == MFRC522::STATUS_OK);
}

bool RFIDInterface::readBlock(uint8_t block, uint8_t* buffer) {
    byte size = 18;
    MFRC522::StatusCode status = mfrc522->MIFARE_Read(block, buffer, &size);
    return (status == MFRC522::STATUS_OK);
}

bool RFIDInterface::writeBlock(uint8_t block, uint8_t* data) {
    // Never write to block 0 (manufacturer block) unless magic card
    if (block == 0) {
        return false;
    }

    MFRC522::StatusCode status = mfrc522->MIFARE_Write(block, data, 16);
    return (status == MFRC522::STATUS_OK);
}

void RFIDInterface::haltCard() {
    mfrc522->PICC_HaltA();
    mfrc522->PCD_StopCrypto1();
}

uint8_t RFIDInterface::getSectorForBlock(uint8_t block) {
    if (block < 128) {
        return block / 4;
    }
    return 32 + (block - 128) / 16;
}

uint8_t RFIDInterface::getFirstBlockOfSector(uint8_t sector) {
    if (sector < 32) {
        return sector * 4;
    }
    return 128 + (sector - 32) * 16;
}

uint8_t RFIDInterface::getNumBlocksInSector(uint8_t sector) {
    if (sector < 32) {
        return 4;
    }
    return 16;
}

String RFIDInterface::getUIDString(uint8_t* uid, uint8_t length) {
    return bytesToHex(uid, length);
}

String RFIDInterface::bytesToHex(uint8_t* data, uint8_t length) {
    String hex = "";
    for (uint8_t i = 0; i < length; i++) {
        if (data[i] < 0x10) hex += "0";
        hex += String(data[i], HEX);
        if (i < length - 1) hex += ":";
    }
    hex.toUpperCase();
    return hex;
}

// State accessors
bool RFIDInterface::isCardPresent() { return cardPresent; }
CardData* RFIDInterface::getLastCard() { return &lastCard; }
uint8_t RFIDInterface::getCurrentMode() { return currentMode; }
void RFIDInterface::setCurrentMode(uint8_t mode) { currentMode = mode; }
void RFIDInterface::startMonitoring() { monitoring = true; }
void RFIDInterface::stopMonitoring() { monitoring = false; }
bool RFIDInterface::isMonitoring() { return monitoring; }
uint32_t RFIDInterface::getTotalCardsRead() { return totalCardsRead; }
uint32_t RFIDInterface::getVulnerableCardsFound() { return vulnerableCards; }

#endif // HAS_RFID

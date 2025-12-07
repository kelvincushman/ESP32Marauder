/*
 * SavedConnections.cpp
 * Persistent Storage Manager Implementation
 *
 * Manages saving/loading connection data for ALL modules:
 * - WiFi networks and access points
 * - Bluetooth devices
 * - IR signals
 * - Sub-GHz signals
 * - RFID cards
 * - Session data and preferences
 */

#include "SavedConnections.h"

// Global instance
SavedConnections saved_conn_obj;

// ============================================================================
// LIFECYCLE
// ============================================================================

void SavedConnections::RunSetup() {
    Serial.println(F(""));
    Serial.println(F("[STORAGE] Initializing connection storage..."));

    initialized = false;
    sdCardReady = false;

    // Initialize linked lists
    irSignals = new LinkedList<SavedIRSignal>();
    subghzSignals = new LinkedList<SavedSubGHzSignal>();
    rfidCards = new LinkedList<SavedRFIDCard>();
    sessionHistory = new LinkedList<SessionHistoryEntry>();

    // Check SD card
    if (!checkSDCard()) {
        Serial.println(F("[STORAGE] WARNING: SD card not available"));
        Serial.println(F("[STORAGE] Data will not persist between sessions"));
        return;
    }

    // Create directory structure
    if (!createDirectories()) {
        Serial.println(F("[STORAGE] WARNING: Could not create directories"));
        return;
    }

    // Load saved data
    loadSession();

    initialized = true;
    Serial.println(F("[STORAGE] Connection storage ready"));
    Serial.println(F(""));
}

bool SavedConnections::isReady() {
    return initialized && sdCardReady;
}

bool SavedConnections::checkSDCard() {
    #ifdef HAS_SD
        if (SD.begin()) {
            sdCardReady = true;
            Serial.println(F("[STORAGE] SD card detected"));

            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            Serial.print(F("[STORAGE] Card size: "));
            Serial.print((uint32_t)cardSize);
            Serial.println(F(" MB"));

            return true;
        }
    #endif

    sdCardReady = false;
    return false;
}

bool SavedConnections::createDirectories() {
    if (!sdCardReady) return false;

    // Create main pentest directory
    if (!SD.exists(PENTEST_DIR)) {
        if (!SD.mkdir(PENTEST_DIR)) {
            Serial.println(F("[STORAGE] Failed to create pentest directory"));
            return false;
        }
    }

    // Create subdirectories
    const char* dirs[] = {
        IR_SIGNALS_DIR,
        SUBGHZ_SIGNALS_DIR,
        RFID_CARDS_DIR,
        SESSION_DIR,
        "/pentest/wifi",
        "/pentest/bluetooth"
    };

    for (int i = 0; i < 6; i++) {
        if (!SD.exists(dirs[i])) {
            if (!SD.mkdir(dirs[i])) {
                Serial.print(F("[STORAGE] Failed to create: "));
                Serial.println(dirs[i]);
            }
        }
    }

    Serial.println(F("[STORAGE] Directory structure ready"));
    return true;
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

bool SavedConnections::writeFile(const char* path, const char* data) {
    if (!sdCardReady) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.print(F("[STORAGE] Failed to open for write: "));
        Serial.println(path);
        return false;
    }

    bool success = file.print(data);
    file.close();

    return success;
}

String SavedConnections::readFile(const char* path) {
    if (!sdCardReady) return "";

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return "";
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();

    return content;
}

bool SavedConnections::fileExists(const char* path) {
    if (!sdCardReady) return false;
    return SD.exists(path);
}

bool SavedConnections::deleteFile(const char* path) {
    if (!sdCardReady) return false;
    return SD.remove(path);
}

// ============================================================================
// IR SIGNAL OPERATIONS
// ============================================================================

bool SavedConnections::saveIRSignal(SavedIRSignal* signal) {
    if (!sdCardReady) return false;

    // Generate ID if new
    if (signal->id == 0) {
        signal->id = generateUniqueId();
    }

    // Set timestamp
    signal->timestamp = millis();

    // Create filename
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/ir_%lu.dat", IR_SIGNALS_DIR, signal->id);

    // Serialize to JSON
    String json = irSignalToJson(signal);

    if (writeFile(filename, json.c_str())) {
        Serial.print(F("[STORAGE] Saved IR signal: "));
        Serial.println(signal->name);

        // Add to history
        addHistoryEntry(MODULE_IR, ACTION_SAVE, signal->name, true);
        return true;
    }

    return false;
}

bool SavedConnections::loadIRSignal(uint32_t id, SavedIRSignal* signal) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/ir_%lu.dat", IR_SIGNALS_DIR, id);

    String json = readFile(filename);
    if (json.length() == 0) return false;

    return jsonToIRSignal(json, signal);
}

bool SavedConnections::deleteIRSignal(uint32_t id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/ir_%lu.dat", IR_SIGNALS_DIR, id);

    if (deleteFile(filename)) {
        addHistoryEntry(MODULE_IR, ACTION_DELETE, "Signal deleted", true);
        return true;
    }
    return false;
}

uint32_t SavedConnections::quickSaveIR(uint8_t protocol, uint32_t code, uint8_t bits, const char* autoName) {
    SavedIRSignal signal;
    memset(&signal, 0, sizeof(SavedIRSignal));

    signal.id = generateUniqueId();
    signal.protocol = protocol;
    signal.code = code;
    signal.bits = bits;
    signal.timestamp = millis();
    signal.favorite = false;
    signal.successCount = 0;

    // Generate name
    if (autoName != NULL) {
        strncpy(signal.name, autoName, MAX_NAME_LENGTH - 1);
    } else {
        String name = generateAutoName(MODULE_IR, protocol);
        strncpy(signal.name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    if (saveIRSignal(&signal)) {
        return signal.id;
    }
    return 0;
}

LinkedList<SavedIRSignal>* SavedConnections::getAllIRSignals() {
    irSignals->clear();

    if (!sdCardReady) return irSignals;

    File dir = SD.open(IR_SIGNALS_DIR);
    if (!dir) return irSignals;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String filename = entry.name();
            if (filename.endsWith(".dat")) {
                SavedIRSignal signal;
                String content = "";
                while (entry.available()) {
                    content += (char)entry.read();
                }
                if (jsonToIRSignal(content, &signal)) {
                    irSignals->add(signal);
                }
            }
        }
        entry.close();
    }
    dir.close();

    return irSignals;
}

uint8_t SavedConnections::getIRSignalCount() {
    getAllIRSignals();
    return irSignals->size();
}

// ============================================================================
// SUB-GHZ SIGNAL OPERATIONS
// ============================================================================

bool SavedConnections::saveSubGHzSignal(SavedSubGHzSignal* signal) {
    if (!sdCardReady) return false;

    if (signal->id == 0) {
        signal->id = generateUniqueId();
    }
    signal->timestamp = millis();

    char filename[64];
    snprintf(filename, sizeof(filename), "%s/subghz_%lu.dat", SUBGHZ_SIGNALS_DIR, signal->id);

    String json = subghzSignalToJson(signal);

    if (writeFile(filename, json.c_str())) {
        Serial.print(F("[STORAGE] Saved Sub-GHz signal: "));
        Serial.println(signal->name);
        addHistoryEntry(MODULE_SUBGHZ, ACTION_SAVE, signal->name, true);
        return true;
    }
    return false;
}

bool SavedConnections::loadSubGHzSignal(uint32_t id, SavedSubGHzSignal* signal) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/subghz_%lu.dat", SUBGHZ_SIGNALS_DIR, id);

    String json = readFile(filename);
    if (json.length() == 0) return false;

    return jsonToSubGHzSignal(json, signal);
}

bool SavedConnections::deleteSubGHzSignal(uint32_t id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/subghz_%lu.dat", SUBGHZ_SIGNALS_DIR, id);

    if (deleteFile(filename)) {
        addHistoryEntry(MODULE_SUBGHZ, ACTION_DELETE, "Signal deleted", true);
        return true;
    }
    return false;
}

uint32_t SavedConnections::quickSaveSubGHz(uint32_t frequency, uint8_t protocol, uint32_t code, const char* autoName) {
    SavedSubGHzSignal signal;
    memset(&signal, 0, sizeof(SavedSubGHzSignal));

    signal.id = generateUniqueId();
    signal.frequency = frequency;
    signal.protocol = protocol;
    signal.code = code;
    signal.timestamp = millis();
    signal.favorite = false;
    signal.hasRollingCode = false;

    if (autoName != NULL) {
        strncpy(signal.name, autoName, MAX_NAME_LENGTH - 1);
    } else {
        String name = generateAutoName(MODULE_SUBGHZ, protocol);
        strncpy(signal.name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    if (saveSubGHzSignal(&signal)) {
        return signal.id;
    }
    return 0;
}

LinkedList<SavedSubGHzSignal>* SavedConnections::getAllSubGHzSignals() {
    subghzSignals->clear();

    if (!sdCardReady) return subghzSignals;

    File dir = SD.open(SUBGHZ_SIGNALS_DIR);
    if (!dir) return subghzSignals;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String filename = entry.name();
            if (filename.endsWith(".dat")) {
                SavedSubGHzSignal signal;
                String content = "";
                while (entry.available()) {
                    content += (char)entry.read();
                }
                if (jsonToSubGHzSignal(content, &signal)) {
                    subghzSignals->add(signal);
                }
            }
        }
        entry.close();
    }
    dir.close();

    return subghzSignals;
}

uint8_t SavedConnections::getSubGHzSignalCount() {
    getAllSubGHzSignals();
    return subghzSignals->size();
}

// ============================================================================
// RFID CARD OPERATIONS
// ============================================================================

bool SavedConnections::saveRFIDCard(SavedRFIDCard* card) {
    if (!sdCardReady) return false;

    if (card->id == 0) {
        card->id = generateUniqueId();
    }
    card->timestamp = millis();

    char filename[64];
    snprintf(filename, sizeof(filename), "%s/rfid_%lu.dat", RFID_CARDS_DIR, card->id);

    String json = rfidCardToJson(card);

    if (writeFile(filename, json.c_str())) {
        Serial.print(F("[STORAGE] Saved RFID card: "));
        Serial.println(card->name);
        addHistoryEntry(MODULE_RFID, ACTION_SAVE, card->name, true);
        return true;
    }
    return false;
}

bool SavedConnections::loadRFIDCard(uint32_t id, SavedRFIDCard* card) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/rfid_%lu.dat", RFID_CARDS_DIR, id);

    String json = readFile(filename);
    if (json.length() == 0) return false;

    return jsonToRFIDCard(json, card);
}

bool SavedConnections::deleteRFIDCard(uint32_t id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/rfid_%lu.dat", RFID_CARDS_DIR, id);

    if (deleteFile(filename)) {
        addHistoryEntry(MODULE_RFID, ACTION_DELETE, "Card deleted", true);
        return true;
    }
    return false;
}

uint32_t SavedConnections::quickSaveRFID(uint8_t* uid, uint8_t uidLength, uint8_t cardType, const char* autoName) {
    SavedRFIDCard card;
    memset(&card, 0, sizeof(SavedRFIDCard));

    card.id = generateUniqueId();
    card.uidLength = uidLength;
    memcpy(card.uid, uid, uidLength);
    card.cardType = cardType;
    card.timestamp = millis();
    card.favorite = false;

    if (autoName != NULL) {
        strncpy(card.name, autoName, MAX_NAME_LENGTH - 1);
    } else {
        String name = generateAutoName(MODULE_RFID, cardType);
        strncpy(card.name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    if (saveRFIDCard(&card)) {
        return card.id;
    }
    return 0;
}

LinkedList<SavedRFIDCard>* SavedConnections::getAllRFIDCards() {
    rfidCards->clear();

    if (!sdCardReady) return rfidCards;

    File dir = SD.open(RFID_CARDS_DIR);
    if (!dir) return rfidCards;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String filename = entry.name();
            if (filename.endsWith(".dat")) {
                SavedRFIDCard card;
                String content = "";
                while (entry.available()) {
                    content += (char)entry.read();
                }
                if (jsonToRFIDCard(content, &card)) {
                    rfidCards->add(card);
                }
            }
        }
        entry.close();
    }
    dir.close();

    return rfidCards;
}

uint8_t SavedConnections::getRFIDCardCount() {
    getAllRFIDCards();
    return rfidCards->size();
}

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

bool SavedConnections::saveSession() {
    if (!sdCardReady) return false;

    // Save settings
    saveSettings(&currentSettings);

    // Save history (keep last MAX_SESSION_HISTORY entries)
    String historyJson = "[\n";
    int start = sessionHistory->size() > MAX_SESSION_HISTORY ?
                sessionHistory->size() - MAX_SESSION_HISTORY : 0;

    for (int i = start; i < sessionHistory->size(); i++) {
        SessionHistoryEntry entry = sessionHistory->get(i);
        if (i > start) historyJson += ",\n";
        historyJson += "{";
        historyJson += "\"ts\":" + String(entry.timestamp) + ",";
        historyJson += "\"mod\":" + String(entry.moduleType) + ",";
        historyJson += "\"act\":" + String(entry.actionType) + ",";
        historyJson += "\"det\":\"" + String(entry.details) + "\",";
        historyJson += "\"ok\":" + String(entry.success ? "true" : "false");
        historyJson += "}";
    }
    historyJson += "\n]";

    writeFile(HISTORY_FILE, historyJson.c_str());

    Serial.println(F("[STORAGE] Session saved"));
    return true;
}

bool SavedConnections::loadSession() {
    // Load settings
    loadSettings(&currentSettings);

    // Load history
    String historyJson = readFile(HISTORY_FILE);
    if (historyJson.length() > 0) {
        // Parse history entries (simplified parsing)
        sessionHistory->clear();
        // TODO: Implement JSON array parsing
    }

    Serial.println(F("[STORAGE] Session loaded"));
    return true;
}

bool SavedConnections::clearSession() {
    sessionHistory->clear();
    deleteFile(HISTORY_FILE);
    return true;
}

bool SavedConnections::saveSettings(SessionSettings* settings) {
    if (!sdCardReady) return false;

    String json = "{\n";
    json += "  \"irAutoSave\": " + String(settings->irAutoSave ? "true" : "false") + ",\n";
    json += "  \"irDefaultProtocol\": " + String(settings->irDefaultProtocol) + ",\n";
    json += "  \"subghzAutoSave\": " + String(settings->subghzAutoSave ? "true" : "false") + ",\n";
    json += "  \"subghzDefaultFreq\": " + String(settings->subghzDefaultFreq) + ",\n";
    json += "  \"subghzDefaultMod\": " + String(settings->subghzDefaultModulation) + ",\n";
    json += "  \"rfidAutoSave\": " + String(settings->rfidAutoSave ? "true" : "false") + ",\n";
    json += "  \"rfidAutoKeyTest\": " + String(settings->rfidAutoKeyTest ? "true" : "false") + ",\n";
    json += "  \"matrixTheme\": " + String(settings->matrixThemeEnabled ? "true" : "false") + ",\n";
    json += "  \"brightness\": " + String(settings->displayBrightness) + ",\n";
    json += "  \"sound\": " + String(settings->soundEnabled ? "true" : "false") + ",\n";
    json += "  \"vibration\": " + String(settings->vibrationEnabled ? "true" : "false") + ",\n";
    json += "  \"autoSleep\": " + String(settings->autoSleepMs) + "\n";
    json += "}";

    return writeFile(SETTINGS_FILE, json.c_str());
}

bool SavedConnections::loadSettings(SessionSettings* settings) {
    // Set defaults
    settings->irAutoSave = true;
    settings->irDefaultProtocol = 0;
    settings->subghzAutoSave = true;
    settings->subghzDefaultFreq = 433920000;
    settings->subghzDefaultModulation = 0;
    settings->rfidAutoSave = true;
    settings->rfidAutoKeyTest = true;
    settings->matrixThemeEnabled = true;
    settings->displayBrightness = 255;
    settings->soundEnabled = true;
    settings->vibrationEnabled = true;
    settings->autoSleepMs = 300000;

    String json = readFile(SETTINGS_FILE);
    if (json.length() == 0) return false;

    // Simple parsing (look for key:value pairs)
    if (json.indexOf("\"irAutoSave\": true") >= 0) settings->irAutoSave = true;
    if (json.indexOf("\"irAutoSave\": false") >= 0) settings->irAutoSave = false;

    if (json.indexOf("\"subghzAutoSave\": true") >= 0) settings->subghzAutoSave = true;
    if (json.indexOf("\"subghzAutoSave\": false") >= 0) settings->subghzAutoSave = false;

    if (json.indexOf("\"rfidAutoSave\": true") >= 0) settings->rfidAutoSave = true;
    if (json.indexOf("\"rfidAutoSave\": false") >= 0) settings->rfidAutoSave = false;

    if (json.indexOf("\"matrixTheme\": true") >= 0) settings->matrixThemeEnabled = true;
    if (json.indexOf("\"matrixTheme\": false") >= 0) settings->matrixThemeEnabled = false;

    return true;
}

SessionSettings* SavedConnections::getSettings() {
    return &currentSettings;
}

bool SavedConnections::addHistoryEntry(uint8_t moduleType, uint8_t actionType, const char* details, bool success) {
    SessionHistoryEntry entry;
    entry.timestamp = millis();
    entry.moduleType = moduleType;
    entry.actionType = actionType;
    strncpy(entry.details, details, 63);
    entry.success = success;

    sessionHistory->add(entry);

    // Trim if too many entries
    while (sessionHistory->size() > MAX_SESSION_HISTORY) {
        sessionHistory->remove(0);
    }

    return true;
}

LinkedList<SessionHistoryEntry>* SavedConnections::getHistory() {
    return sessionHistory;
}

bool SavedConnections::clearHistory() {
    sessionHistory->clear();
    return true;
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

String SavedConnections::irSignalToJson(SavedIRSignal* signal) {
    String json = "{\n";
    json += "  \"id\": " + String(signal->id) + ",\n";
    json += "  \"name\": \"" + String(signal->name) + "\",\n";
    json += "  \"notes\": \"" + String(signal->notes) + "\",\n";
    json += "  \"protocol\": " + String(signal->protocol) + ",\n";
    json += "  \"code\": " + String(signal->code) + ",\n";
    json += "  \"bits\": " + String(signal->bits) + ",\n";
    json += "  \"timestamp\": " + String(signal->timestamp) + ",\n";
    json += "  \"successCount\": " + String(signal->successCount) + ",\n";
    json += "  \"favorite\": " + String(signal->favorite ? "true" : "false") + "\n";
    json += "}";
    return json;
}

bool SavedConnections::jsonToIRSignal(String json, SavedIRSignal* signal) {
    memset(signal, 0, sizeof(SavedIRSignal));

    // Extract ID
    int idx = json.indexOf("\"id\":");
    if (idx >= 0) {
        signal->id = json.substring(idx + 5).toInt();
    }

    // Extract name
    idx = json.indexOf("\"name\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String name = json.substring(idx + 9, endIdx);
        strncpy(signal->name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    // Extract protocol
    idx = json.indexOf("\"protocol\":");
    if (idx >= 0) {
        signal->protocol = json.substring(idx + 11).toInt();
    }

    // Extract code
    idx = json.indexOf("\"code\":");
    if (idx >= 0) {
        signal->code = json.substring(idx + 7).toInt();
    }

    // Extract bits
    idx = json.indexOf("\"bits\":");
    if (idx >= 0) {
        signal->bits = json.substring(idx + 7).toInt();
    }

    // Extract favorite
    signal->favorite = (json.indexOf("\"favorite\": true") >= 0);

    return true;
}

String SavedConnections::subghzSignalToJson(SavedSubGHzSignal* signal) {
    String json = "{\n";
    json += "  \"id\": " + String(signal->id) + ",\n";
    json += "  \"name\": \"" + String(signal->name) + "\",\n";
    json += "  \"notes\": \"" + String(signal->notes) + "\",\n";
    json += "  \"frequency\": " + String(signal->frequency) + ",\n";
    json += "  \"modulation\": " + String(signal->modulation) + ",\n";
    json += "  \"protocol\": " + String(signal->protocol) + ",\n";
    json += "  \"code\": " + String(signal->code) + ",\n";
    json += "  \"timestamp\": " + String(signal->timestamp) + ",\n";
    json += "  \"hasRollingCode\": " + String(signal->hasRollingCode ? "true" : "false") + ",\n";
    json += "  \"successCount\": " + String(signal->successCount) + ",\n";
    json += "  \"favorite\": " + String(signal->favorite ? "true" : "false") + "\n";
    json += "}";
    return json;
}

bool SavedConnections::jsonToSubGHzSignal(String json, SavedSubGHzSignal* signal) {
    memset(signal, 0, sizeof(SavedSubGHzSignal));

    int idx = json.indexOf("\"id\":");
    if (idx >= 0) signal->id = json.substring(idx + 5).toInt();

    idx = json.indexOf("\"name\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String name = json.substring(idx + 9, endIdx);
        strncpy(signal->name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    idx = json.indexOf("\"frequency\":");
    if (idx >= 0) signal->frequency = json.substring(idx + 12).toInt();

    idx = json.indexOf("\"protocol\":");
    if (idx >= 0) signal->protocol = json.substring(idx + 11).toInt();

    idx = json.indexOf("\"code\":");
    if (idx >= 0) signal->code = json.substring(idx + 7).toInt();

    signal->hasRollingCode = (json.indexOf("\"hasRollingCode\": true") >= 0);
    signal->favorite = (json.indexOf("\"favorite\": true") >= 0);

    return true;
}

String SavedConnections::rfidCardToJson(SavedRFIDCard* card) {
    String json = "{\n";
    json += "  \"id\": " + String(card->id) + ",\n";
    json += "  \"name\": \"" + String(card->name) + "\",\n";
    json += "  \"notes\": \"" + String(card->notes) + "\",\n";
    json += "  \"uidLength\": " + String(card->uidLength) + ",\n";

    // UID as hex string
    json += "  \"uid\": \"";
    for (int i = 0; i < card->uidLength; i++) {
        if (card->uid[i] < 0x10) json += "0";
        json += String(card->uid[i], HEX);
    }
    json += "\",\n";

    json += "  \"cardType\": " + String(card->cardType) + ",\n";
    json += "  \"sak\": " + String(card->sak) + ",\n";
    json += "  \"securityLevel\": " + String(card->securityLevel) + ",\n";
    json += "  \"timestamp\": " + String(card->timestamp) + ",\n";
    json += "  \"favorite\": " + String(card->favorite ? "true" : "false") + "\n";
    json += "}";
    return json;
}

bool SavedConnections::jsonToRFIDCard(String json, SavedRFIDCard* card) {
    memset(card, 0, sizeof(SavedRFIDCard));

    int idx = json.indexOf("\"id\":");
    if (idx >= 0) card->id = json.substring(idx + 5).toInt();

    idx = json.indexOf("\"name\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String name = json.substring(idx + 9, endIdx);
        strncpy(card->name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    idx = json.indexOf("\"uidLength\":");
    if (idx >= 0) card->uidLength = json.substring(idx + 12).toInt();

    idx = json.indexOf("\"uid\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 8);
        String uidHex = json.substring(idx + 8, endIdx);
        for (int i = 0; i < card->uidLength && i * 2 < uidHex.length(); i++) {
            String byteStr = uidHex.substring(i * 2, i * 2 + 2);
            card->uid[i] = strtol(byteStr.c_str(), NULL, 16);
        }
    }

    idx = json.indexOf("\"cardType\":");
    if (idx >= 0) card->cardType = json.substring(idx + 11).toInt();

    idx = json.indexOf("\"sak\":");
    if (idx >= 0) card->sak = json.substring(idx + 6).toInt();

    card->favorite = (json.indexOf("\"favorite\": true") >= 0);

    return true;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t SavedConnections::generateUniqueId() {
    // Use timestamp + random for uniqueness
    return (millis() & 0xFFFF) << 16 | random(0, 0xFFFF);
}

String SavedConnections::generateAutoName(uint8_t moduleType, uint8_t protocol) {
    String name = "";
    uint32_t count = 0;

    switch (moduleType) {
        case MODULE_IR:
            count = getIRSignalCount() + 1;
            switch (protocol) {
                case 1: name = "NEC_"; break;
                case 2: name = "Sony_"; break;
                case 3: name = "RC5_"; break;
                case 4: name = "RC6_"; break;
                case 5: name = "Samsung_"; break;
                default: name = "IR_"; break;
            }
            break;

        case MODULE_SUBGHZ:
            count = getSubGHzSignalCount() + 1;
            switch (protocol) {
                case 1: name = "Princeton_"; break;
                case 2: name = "CAME_"; break;
                case 3: name = "Nice_"; break;
                default: name = "SubGHz_"; break;
            }
            break;

        case MODULE_RFID:
            count = getRFIDCardCount() + 1;
            switch (protocol) {
                case 1: name = "MIFARE_"; break;
                case 2: name = "NTAG_"; break;
                case 3: name = "DESFire_"; break;
                default: name = "Card_"; break;
            }
            break;

        default:
            name = "Unknown_";
            break;
    }

    name += String(count);
    return name;
}

String SavedConnections::formatTimestamp(uint32_t timestamp) {
    // Format as relative time since boot
    uint32_t seconds = timestamp / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    if (hours > 0) {
        return String(hours) + "h " + String(minutes % 60) + "m ago";
    } else if (minutes > 0) {
        return String(minutes) + "m " + String(seconds % 60) + "s ago";
    } else {
        return String(seconds) + "s ago";
    }
}

uint32_t SavedConnections::getTotalStorageUsed() {
    if (!sdCardReady) return 0;
    return SD.usedBytes();
}

uint32_t SavedConnections::getAvailableStorage() {
    if (!sdCardReady) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

// ============================================================================
// EXPORT/IMPORT
// ============================================================================

bool SavedConnections::exportIRSignal(uint32_t id, const char* filename) {
    SavedIRSignal signal;
    if (!loadIRSignal(id, &signal)) return false;

    String json = irSignalToJson(&signal);
    return writeFile(filename, json.c_str());
}

bool SavedConnections::exportSubGHzSignal(uint32_t id, const char* filename) {
    SavedSubGHzSignal signal;
    if (!loadSubGHzSignal(id, &signal)) return false;

    String json = subghzSignalToJson(&signal);
    return writeFile(filename, json.c_str());
}

bool SavedConnections::exportRFIDCard(uint32_t id, const char* filename) {
    SavedRFIDCard card;
    if (!loadRFIDCard(id, &card)) return false;

    String json = rfidCardToJson(&card);
    return writeFile(filename, json.c_str());
}

// ============================================================================
// RENAME AND UPDATE OPERATIONS
// ============================================================================

bool SavedConnections::renameIRSignal(uint32_t id, const char* newName) {
    SavedIRSignal signal;
    if (!loadIRSignal(id, &signal)) return false;

    strncpy(signal.name, newName, MAX_NAME_LENGTH - 1);
    signal.name[MAX_NAME_LENGTH - 1] = '\0';

    return saveIRSignal(&signal);
}

bool SavedConnections::setIRSignalNotes(uint32_t id, const char* notes) {
    SavedIRSignal signal;
    if (!loadIRSignal(id, &signal)) return false;

    strncpy(signal.notes, notes, MAX_NOTES_LENGTH - 1);
    signal.notes[MAX_NOTES_LENGTH - 1] = '\0';

    return saveIRSignal(&signal);
}

bool SavedConnections::toggleIRFavorite(uint32_t id) {
    SavedIRSignal signal;
    if (!loadIRSignal(id, &signal)) return false;

    signal.favorite = !signal.favorite;
    return saveIRSignal(&signal);
}

bool SavedConnections::renameSubGHzSignal(uint32_t id, const char* newName) {
    SavedSubGHzSignal signal;
    if (!loadSubGHzSignal(id, &signal)) return false;

    strncpy(signal.name, newName, MAX_NAME_LENGTH - 1);
    signal.name[MAX_NAME_LENGTH - 1] = '\0';

    return saveSubGHzSignal(&signal);
}

bool SavedConnections::setSubGHzSignalNotes(uint32_t id, const char* notes) {
    SavedSubGHzSignal signal;
    if (!loadSubGHzSignal(id, &signal)) return false;

    strncpy(signal.notes, notes, MAX_NOTES_LENGTH - 1);
    signal.notes[MAX_NOTES_LENGTH - 1] = '\0';

    return saveSubGHzSignal(&signal);
}

bool SavedConnections::toggleSubGHzFavorite(uint32_t id) {
    SavedSubGHzSignal signal;
    if (!loadSubGHzSignal(id, &signal)) return false;

    signal.favorite = !signal.favorite;
    return saveSubGHzSignal(&signal);
}

bool SavedConnections::renameRFIDCard(uint32_t id, const char* newName) {
    SavedRFIDCard card;
    if (!loadRFIDCard(id, &card)) return false;

    strncpy(card.name, newName, MAX_NAME_LENGTH - 1);
    card.name[MAX_NAME_LENGTH - 1] = '\0';

    return saveRFIDCard(&card);
}

bool SavedConnections::setRFIDCardNotes(uint32_t id, const char* notes) {
    SavedRFIDCard card;
    if (!loadRFIDCard(id, &card)) return false;

    strncpy(card.notes, notes, MAX_NOTES_LENGTH - 1);
    card.notes[MAX_NOTES_LENGTH - 1] = '\0';

    return saveRFIDCard(&card);
}

bool SavedConnections::toggleRFIDFavorite(uint32_t id) {
    SavedRFIDCard card;
    if (!loadRFIDCard(id, &card)) return false;

    card.favorite = !card.favorite;
    return saveRFIDCard(&card);
}

// ============================================================================
// WIFI NETWORK OPERATIONS
// ============================================================================

bool SavedConnections::saveWiFiNetwork(SavedWiFiNetwork* network) {
    if (!sdCardReady) return false;

    if (network->id == 0) {
        network->id = generateUniqueId();
    }
    network->timestamp = millis();

    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/wifi/wifi_%lu.dat", network->id);

    // Serialize
    String json = "{\n";
    json += "  \"id\": " + String(network->id) + ",\n";
    json += "  \"name\": \"" + String(network->name) + "\",\n";
    json += "  \"notes\": \"" + String(network->notes) + "\",\n";
    json += "  \"ssid\": \"" + String(network->ssid) + "\",\n";
    json += "  \"bssid\": \"" + String(network->bssid) + "\",\n";
    json += "  \"channel\": " + String(network->channel) + ",\n";
    json += "  \"rssi\": " + String(network->rssi) + ",\n";
    json += "  \"encType\": " + String(network->encryptionType) + ",\n";
    json += "  \"hasHandshake\": " + String(network->hasHandshake ? "true" : "false") + ",\n";
    json += "  \"handshakeFile\": \"" + String(network->handshakeFile) + "\",\n";
    json += "  \"timestamp\": " + String(network->timestamp) + ",\n";
    json += "  \"lastSeen\": " + String(network->lastSeen) + ",\n";
    json += "  \"timesFound\": " + String(network->timesFound) + ",\n";
    json += "  \"favorite\": " + String(network->favorite ? "true" : "false") + ",\n";
    json += "  \"isTarget\": " + String(network->isTarget ? "true" : "false") + "\n";
    json += "}";

    if (writeFile(filename, json.c_str())) {
        Serial.print(F("[STORAGE] Saved WiFi network: "));
        Serial.println(network->ssid);
        addHistoryEntry(MODULE_WIFI, ACTION_SAVE, network->ssid, true);
        return true;
    }
    return false;
}

bool SavedConnections::loadWiFiNetwork(uint32_t id, SavedWiFiNetwork* network) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/wifi/wifi_%lu.dat", id);

    String json = readFile(filename);
    if (json.length() == 0) return false;

    memset(network, 0, sizeof(SavedWiFiNetwork));

    int idx = json.indexOf("\"id\":");
    if (idx >= 0) network->id = json.substring(idx + 5).toInt();

    idx = json.indexOf("\"name\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String name = json.substring(idx + 9, endIdx);
        strncpy(network->name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    idx = json.indexOf("\"ssid\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String ssid = json.substring(idx + 9, endIdx);
        strncpy(network->ssid, ssid.c_str(), 32);
    }

    idx = json.indexOf("\"bssid\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 10);
        String bssid = json.substring(idx + 10, endIdx);
        strncpy(network->bssid, bssid.c_str(), 17);
    }

    idx = json.indexOf("\"channel\":");
    if (idx >= 0) network->channel = json.substring(idx + 10).toInt();

    idx = json.indexOf("\"rssi\":");
    if (idx >= 0) network->rssi = json.substring(idx + 7).toInt();

    idx = json.indexOf("\"encType\":");
    if (idx >= 0) network->encryptionType = json.substring(idx + 10).toInt();

    network->hasHandshake = (json.indexOf("\"hasHandshake\": true") >= 0);
    network->favorite = (json.indexOf("\"favorite\": true") >= 0);
    network->isTarget = (json.indexOf("\"isTarget\": true") >= 0);

    return true;
}

bool SavedConnections::deleteWiFiNetwork(uint32_t id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/wifi/wifi_%lu.dat", id);

    if (deleteFile(filename)) {
        addHistoryEntry(MODULE_WIFI, ACTION_DELETE, "Network deleted", true);
        return true;
    }
    return false;
}

bool SavedConnections::renameWiFiNetwork(uint32_t id, const char* newName) {
    SavedWiFiNetwork network;
    if (!loadWiFiNetwork(id, &network)) return false;

    strncpy(network.name, newName, MAX_NAME_LENGTH - 1);
    network.name[MAX_NAME_LENGTH - 1] = '\0';

    return saveWiFiNetwork(&network);
}

bool SavedConnections::setWiFiNetworkNotes(uint32_t id, const char* notes) {
    SavedWiFiNetwork network;
    if (!loadWiFiNetwork(id, &network)) return false;

    strncpy(network.notes, notes, MAX_NOTES_LENGTH - 1);
    network.notes[MAX_NOTES_LENGTH - 1] = '\0';

    return saveWiFiNetwork(&network);
}

bool SavedConnections::setWiFiPassword(uint32_t id, const char* password) {
    SavedWiFiNetwork network;
    if (!loadWiFiNetwork(id, &network)) return false;

    strncpy(network.password, password, 64);
    network.password[64] = '\0';

    return saveWiFiNetwork(&network);
}

bool SavedConnections::toggleWiFiFavorite(uint32_t id) {
    SavedWiFiNetwork network;
    if (!loadWiFiNetwork(id, &network)) return false;

    network.favorite = !network.favorite;
    return saveWiFiNetwork(&network);
}

bool SavedConnections::markWiFiAsTarget(uint32_t id, bool isTarget) {
    SavedWiFiNetwork network;
    if (!loadWiFiNetwork(id, &network)) return false;

    network.isTarget = isTarget;
    return saveWiFiNetwork(&network);
}

uint32_t SavedConnections::quickSaveWiFi(const char* ssid, const char* bssid, uint8_t channel, int8_t rssi, uint8_t encType) {
    SavedWiFiNetwork network;
    memset(&network, 0, sizeof(SavedWiFiNetwork));

    network.id = generateUniqueId();
    strncpy(network.ssid, ssid, 32);
    strncpy(network.bssid, bssid, 17);
    strncpy(network.name, ssid, MAX_NAME_LENGTH - 1);  // Default name is SSID
    network.channel = channel;
    network.rssi = rssi;
    network.encryptionType = encType;
    network.timestamp = millis();
    network.lastSeen = millis();
    network.timesFound = 1;

    if (saveWiFiNetwork(&network)) {
        return network.id;
    }
    return 0;
}

// ============================================================================
// BLUETOOTH DEVICE OPERATIONS
// ============================================================================

bool SavedConnections::saveBTDevice(SavedBTDevice* device) {
    if (!sdCardReady) return false;

    if (device->id == 0) {
        device->id = generateUniqueId();
    }
    device->timestamp = millis();

    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/bluetooth/bt_%lu.dat", device->id);

    String json = "{\n";
    json += "  \"id\": " + String(device->id) + ",\n";
    json += "  \"name\": \"" + String(device->name) + "\",\n";
    json += "  \"notes\": \"" + String(device->notes) + "\",\n";
    json += "  \"deviceName\": \"" + String(device->deviceName) + "\",\n";
    json += "  \"macAddress\": \"" + String(device->macAddress) + "\",\n";
    json += "  \"rssi\": " + String(device->rssi) + ",\n";
    json += "  \"isClassic\": " + String(device->isClassic ? "true" : "false") + ",\n";
    json += "  \"isSkimmer\": " + String(device->isSkimmer ? "true" : "false") + ",\n";
    json += "  \"timestamp\": " + String(device->timestamp) + ",\n";
    json += "  \"lastSeen\": " + String(device->lastSeen) + ",\n";
    json += "  \"timesFound\": " + String(device->timesFound) + ",\n";
    json += "  \"favorite\": " + String(device->favorite ? "true" : "false") + ",\n";
    json += "  \"isTarget\": " + String(device->isTarget ? "true" : "false") + "\n";
    json += "}";

    if (writeFile(filename, json.c_str())) {
        Serial.print(F("[STORAGE] Saved BT device: "));
        Serial.println(device->deviceName);
        addHistoryEntry(MODULE_BT, ACTION_SAVE, device->deviceName, true);
        return true;
    }
    return false;
}

bool SavedConnections::loadBTDevice(uint32_t id, SavedBTDevice* device) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/bluetooth/bt_%lu.dat", id);

    String json = readFile(filename);
    if (json.length() == 0) return false;

    memset(device, 0, sizeof(SavedBTDevice));

    int idx = json.indexOf("\"id\":");
    if (idx >= 0) device->id = json.substring(idx + 5).toInt();

    idx = json.indexOf("\"name\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 9);
        String name = json.substring(idx + 9, endIdx);
        strncpy(device->name, name.c_str(), MAX_NAME_LENGTH - 1);
    }

    idx = json.indexOf("\"deviceName\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 15);
        String devName = json.substring(idx + 15, endIdx);
        strncpy(device->deviceName, devName.c_str(), 32);
    }

    idx = json.indexOf("\"macAddress\": \"");
    if (idx >= 0) {
        int endIdx = json.indexOf("\"", idx + 15);
        String mac = json.substring(idx + 15, endIdx);
        strncpy(device->macAddress, mac.c_str(), 17);
    }

    idx = json.indexOf("\"rssi\":");
    if (idx >= 0) device->rssi = json.substring(idx + 7).toInt();

    device->isClassic = (json.indexOf("\"isClassic\": true") >= 0);
    device->isSkimmer = (json.indexOf("\"isSkimmer\": true") >= 0);
    device->favorite = (json.indexOf("\"favorite\": true") >= 0);
    device->isTarget = (json.indexOf("\"isTarget\": true") >= 0);

    return true;
}

bool SavedConnections::deleteBTDevice(uint32_t id) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/pentest/bluetooth/bt_%lu.dat", id);

    if (deleteFile(filename)) {
        addHistoryEntry(MODULE_BT, ACTION_DELETE, "Device deleted", true);
        return true;
    }
    return false;
}

bool SavedConnections::renameBTDevice(uint32_t id, const char* newName) {
    SavedBTDevice device;
    if (!loadBTDevice(id, &device)) return false;

    strncpy(device.name, newName, MAX_NAME_LENGTH - 1);
    device.name[MAX_NAME_LENGTH - 1] = '\0';

    return saveBTDevice(&device);
}

bool SavedConnections::setBTDeviceNotes(uint32_t id, const char* notes) {
    SavedBTDevice device;
    if (!loadBTDevice(id, &device)) return false;

    strncpy(device.notes, notes, MAX_NOTES_LENGTH - 1);
    device.notes[MAX_NOTES_LENGTH - 1] = '\0';

    return saveBTDevice(&device);
}

bool SavedConnections::toggleBTFavorite(uint32_t id) {
    SavedBTDevice device;
    if (!loadBTDevice(id, &device)) return false;

    device.favorite = !device.favorite;
    return saveBTDevice(&device);
}

bool SavedConnections::markBTAsTarget(uint32_t id, bool isTarget) {
    SavedBTDevice device;
    if (!loadBTDevice(id, &device)) return false;

    device.isTarget = isTarget;
    return saveBTDevice(&device);
}

bool SavedConnections::markBTAsSkimmer(uint32_t id, bool isSkimmer) {
    SavedBTDevice device;
    if (!loadBTDevice(id, &device)) return false;

    device.isSkimmer = isSkimmer;
    return saveBTDevice(&device);
}

uint32_t SavedConnections::quickSaveBT(const char* deviceName, const char* macAddress, int8_t rssi, bool isClassic) {
    SavedBTDevice device;
    memset(&device, 0, sizeof(SavedBTDevice));

    device.id = generateUniqueId();
    strncpy(device.deviceName, deviceName, 32);
    strncpy(device.macAddress, macAddress, 17);
    strncpy(device.name, deviceName, MAX_NAME_LENGTH - 1);  // Default name is device name
    device.rssi = rssi;
    device.isClassic = isClassic;
    device.timestamp = millis();
    device.lastSeen = millis();
    device.timesFound = 1;

    if (saveBTDevice(&device)) {
        return device.id;
    }
    return 0;
}

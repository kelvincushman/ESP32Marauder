/*
 * SubGHzInterface.cpp
 * Sub-GHz Radio Security Testing Module Implementation
 *
 * For authorized penetration testing only.
 * Jamming frequencies is illegal in most jurisdictions.
 */

#include "SubGHzInterface.h"

#ifdef HAS_SUBGHZ

SubGHzInterface subghz_obj;

// Capture parameters
const uint16_t CAPTURE_BUFFER_SIZE = 512;
const uint32_t SIGNAL_TIMEOUT_US = 50000;  // 50ms gap = end of transmission
const uint16_t MIN_PULSES_FOR_SIGNAL = 20;

// ============================================================================
// Lifecycle Methods
// ============================================================================

void SubGHzInterface::RunSetup() {
    Serial.println(F("[SubGHz] Initializing Sub-GHz security module..."));

    initialized = false;
    receiving = false;
    transmitting = false;

    // Initialize CC1101
    initCC1101();

    if (!initialized) {
        Serial.println(F("[SubGHz] ERROR: CC1101 not detected!"));
        return;
    }

    // Default configuration
    currentFrequency = 433920000;  // 433.92 MHz
    currentModulation = MOD_ASK_OOK;

    // State initialization
    currentMode = SUBGHZ_SCAN_OFF;
    lastActivityTime = 0;
    scanInterval = 10;  // 10ms

    // Data structures
    capturedSignals = new LinkedList<SubGHzSignal>();
    scanResults = new LinkedList<FrequencyScan>();
    tpmsSensors = new LinkedList<TPMSSensor>();
    memset(&lastSignal, 0, sizeof(SubGHzSignal));

    // Brute force
    bruteForceActive = false;
    bruteDelayMs = 50;

    // Statistics
    signalsCaptured = 0;
    signalsSent = 0;
    rollingCodesDetected = 0;

    Serial.println(F("[SubGHz] Module ready"));
    Serial.print(F("[SubGHz] Frequency: "));
    Serial.print(currentFrequency / 1000000.0, 3);
    Serial.println(F(" MHz"));
}

void SubGHzInterface::initCC1101() {
    // Set custom SPI pins for CC1101
    ELECHOUSE_cc1101.setSpiPin(CC1101_SCK_PIN, CC1101_MISO_PIN,
                                CC1101_MOSI_PIN, CC1101_CS_PIN);

    // Initialize
    if (ELECHOUSE_cc1101.getCC1101()) {
        initialized = true;
        Serial.println(F("[SubGHz] CC1101 detected"));

        // Configure for ASK/OOK reception
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setMHZ(433.92);
        ELECHOUSE_cc1101.SetRx();

        // Set GDO pins
        pinMode(CC1101_GDO0_PIN, INPUT);
        pinMode(CC1101_GDO2_PIN, INPUT);
    } else {
        Serial.println(F("[SubGHz] CC1101 not found"));
    }
}

void SubGHzInterface::main(uint32_t currentTime) {
    if (!initialized || currentMode == SUBGHZ_SCAN_OFF) {
        return;
    }

    switch (currentMode) {
        case SUBGHZ_SCAN_RAW:
        case SUBGHZ_SCAN_CONTINUOUS:
            if (receiving && detectCarrier()) {
                SubGHzSignal signal;
                if (captureSignal(&signal)) {
                    signalsCaptured++;
                    lastSignal = signal;

                    Serial.println(F("\n[SubGHz] Signal captured!"));
                    Serial.println(formatSignal(&signal));

                    // Try to decode protocol
                    signal.protocol = decodeProtocol(&signal);

                    // Check for rolling code
                    if (isRollingCode(&signal)) {
                        rollingCodesDetected++;
                        signal.isRollingCode = true;
                        Serial.println(F("[SubGHz] WARNING: Rolling code detected!"));
                    }

                    if (currentMode == SUBGHZ_SCAN_CONTINUOUS) {
                        capturedSignals->add(signal);
                    }
                }
            }
            break;

        case SUBGHZ_SCAN_ANALYZE:
            // Frequency spectrum analysis
            if (currentTime - lastActivityTime >= scanInterval) {
                int8_t rssi = getRSSI();
                if (rssi > -80) {  // Activity threshold
                    Serial.print(F("[SubGHz] Activity at "));
                    Serial.print(currentFrequency / 1000000.0, 3);
                    Serial.print(F(" MHz, RSSI: "));
                    Serial.print(rssi);
                    Serial.println(F(" dBm"));
                }
                lastActivityTime = currentTime;
            }
            break;

        case SUBGHZ_SCAN_RSSI:
            if (currentTime - lastActivityTime >= 100) {
                int8_t rssi = getRSSI();
                Serial.print(F("RSSI: "));
                Serial.print(rssi);
                Serial.println(F(" dBm"));
                lastActivityTime = currentTime;
            }
            break;

        case SUBGHZ_ATTACK_BRUTE:
            if (bruteForceActive) {
                if (currentTime - lastBruteTime >= bruteDelayMs) {
                    if (!sendNextBruteCode()) {
                        bruteForceActive = false;
                        Serial.println(F("[SubGHz] Brute force complete"));
                    }
                    lastBruteTime = currentTime;
                }
            }
            break;

        case SUBGHZ_SCAN_TPMS:
            // TPMS sensors typically use 433.92 or 315 MHz
            if (detectCarrier()) {
                SubGHzSignal signal;
                if (captureSignal(&signal)) {
                    TPMSSensor sensor;
                    if (decodeTPMS(&signal, &sensor)) {
                        // Update or add sensor
                        bool found = false;
                        for (int i = 0; i < tpmsSensors->size(); i++) {
                            if (tpmsSensors->get(i).sensorId == sensor.sensorId) {
                                found = true;
                                sensor.lastSeen = currentTime;
                                break;
                            }
                        }
                        if (!found) {
                            sensor.lastSeen = currentTime;
                            tpmsSensors->add(sensor);
                            Serial.print(F("[SubGHz] New TPMS sensor: "));
                            Serial.println(sensor.sensorId, HEX);
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
}

void SubGHzInterface::shutdown() {
    stopReceive();
    ELECHOUSE_cc1101.setSidle();

    delete capturedSignals;
    delete scanResults;
    delete tpmsSensors;

    initialized = false;
    Serial.println(F("[SubGHz] Module shutdown"));
}

// ============================================================================
// Configuration
// ============================================================================

void SubGHzInterface::setFrequency(uint32_t freq) {
    currentFrequency = freq;
    ELECHOUSE_cc1101.setMHZ(freq / 1000000.0);
    Serial.print(F("[SubGHz] Frequency set to "));
    Serial.print(freq / 1000000.0, 3);
    Serial.println(F(" MHz"));
}

void SubGHzInterface::setFrequencyPreset(SubGHzFrequency preset) {
    if (preset < FREQ_CUSTOM) {
        setFrequency(FREQUENCY_VALUES[preset]);
    }
}

void SubGHzInterface::setModulation(SubGHzModulation mod) {
    currentModulation = mod;
    // CC1101 modulation configuration
    switch (mod) {
        case MOD_ASK_OOK:
            ELECHOUSE_cc1101.setModulation(2);  // ASK/OOK
            break;
        case MOD_2FSK:
            ELECHOUSE_cc1101.setModulation(0);  // 2-FSK
            break;
        case MOD_GFSK:
            ELECHOUSE_cc1101.setModulation(1);  // GFSK
            break;
        case MOD_MSK:
            ELECHOUSE_cc1101.setModulation(3);  // MSK
            break;
        default:
            break;
    }
}

uint32_t SubGHzInterface::getFrequency() {
    return currentFrequency;
}

// ============================================================================
// Receive Operations
// ============================================================================

bool SubGHzInterface::startReceive() {
    if (!initialized) return false;

    ELECHOUSE_cc1101.SetRx();
    receiving = true;

    Serial.println(F("[SubGHz] Receiver enabled"));
    Serial.print(F("[SubGHz] Listening on "));
    Serial.print(currentFrequency / 1000000.0, 3);
    Serial.println(F(" MHz"));

    return true;
}

void SubGHzInterface::stopReceive() {
    ELECHOUSE_cc1101.setSidle();
    receiving = false;
    Serial.println(F("[SubGHz] Receiver disabled"));
}

bool SubGHzInterface::detectCarrier() {
    // Check GDO2 for carrier sense
    return digitalRead(CC1101_GDO2_PIN) == HIGH;
}

bool SubGHzInterface::hasSignal() {
    return detectCarrier();
}

bool SubGHzInterface::captureSignal(SubGHzSignal* signal) {
    memset(signal, 0, sizeof(SubGHzSignal));

    // Allocate buffer for raw pulses
    uint16_t* pulseBuffer = (uint16_t*)malloc(CAPTURE_BUFFER_SIZE * sizeof(uint16_t));
    if (!pulseBuffer) {
        return false;
    }

    uint16_t pulseCount = 0;
    uint32_t lastEdgeTime = micros();
    uint32_t pulseStart = lastEdgeTime;
    bool lastState = digitalRead(CC1101_GDO0_PIN);

    // Capture pulses until timeout
    while (pulseCount < CAPTURE_BUFFER_SIZE) {
        bool currentState = digitalRead(CC1101_GDO0_PIN);

        if (currentState != lastState) {
            uint32_t now = micros();
            uint16_t pulseDuration = now - lastEdgeTime;

            // Filter out noise (too short pulses)
            if (pulseDuration > 50 && pulseDuration < 50000) {
                pulseBuffer[pulseCount++] = pulseDuration;
            }

            lastEdgeTime = now;
            lastState = currentState;
        }

        // Check for timeout (end of transmission)
        if (micros() - lastEdgeTime > SIGNAL_TIMEOUT_US) {
            break;
        }

        // Check for minimum capture time
        if (micros() - pulseStart > 500000) {  // 500ms max
            break;
        }
    }

    // Need minimum number of pulses for valid signal
    if (pulseCount < MIN_PULSES_FOR_SIGNAL) {
        free(pulseBuffer);
        return false;
    }

    // Copy data to signal structure
    signal->rawData = pulseBuffer;
    signal->rawLength = pulseCount;
    signal->frequency = currentFrequency;
    signal->modulation = currentModulation;
    signal->captureTime = millis();
    signal->rssi = getRSSI();
    signal->decoded = false;

    // Analyze pulse timing to determine protocol
    processRawPulses(pulseBuffer, pulseCount, signal);

    return true;
}

void SubGHzInterface::processRawPulses(uint16_t* pulses, uint16_t count, SubGHzSignal* signal) {
    // Find typical short and long pulse durations
    uint32_t shortSum = 0, longSum = 0;
    uint16_t shortCount = 0, longCount = 0;

    // First pass: categorize pulses
    uint16_t threshold = 0;
    uint32_t total = 0;
    for (uint16_t i = 0; i < count; i++) {
        total += pulses[i];
    }
    threshold = total / count;  // Average as initial threshold

    // Refine threshold
    for (uint16_t i = 0; i < count; i++) {
        if (pulses[i] < threshold) {
            shortSum += pulses[i];
            shortCount++;
        } else {
            longSum += pulses[i];
            longCount++;
        }
    }

    if (shortCount > 0 && longCount > 0) {
        signal->pulseDuration = shortSum / shortCount;
    }
}

// ============================================================================
// Transmit Operations
// ============================================================================

bool SubGHzInterface::transmitSignal(SubGHzSignal* signal) {
    if (!initialized || !signal) return false;

    // Set frequency to match captured signal
    setFrequency(signal->frequency);

    bool success = false;
    if (signal->rawData && signal->rawLength > 0) {
        success = transmitRaw(signal->rawData, signal->rawLength);
    }

    if (success) {
        signalsSent++;
        Serial.println(F("[SubGHz] Signal transmitted"));
    }

    // Return to receive mode
    startReceive();
    return success;
}

bool SubGHzInterface::transmitRaw(uint16_t* data, uint16_t length) {
    if (!initialized) return false;

    ELECHOUSE_cc1101.SetTx();
    transmitting = true;

    // Transmit OOK pulses
    for (uint16_t i = 0; i < length; i++) {
        digitalWrite(CC1101_GDO0_PIN, (i % 2 == 0) ? HIGH : LOW);
        delayMicroseconds(data[i]);
    }

    digitalWrite(CC1101_GDO0_PIN, LOW);
    transmitting = false;

    return true;
}

bool SubGHzInterface::transmitCode(uint64_t code, uint8_t bits, uint16_t pulseWidth) {
    if (!initialized) return false;

    ELECHOUSE_cc1101.SetTx();
    transmitting = true;

    // Generic OOK transmission
    for (int8_t i = bits - 1; i >= 0; i--) {
        if ((code >> i) & 1) {
            // Logic 1: long pulse
            digitalWrite(CC1101_GDO0_PIN, HIGH);
            delayMicroseconds(pulseWidth * 3);
            digitalWrite(CC1101_GDO0_PIN, LOW);
            delayMicroseconds(pulseWidth);
        } else {
            // Logic 0: short pulse
            digitalWrite(CC1101_GDO0_PIN, HIGH);
            delayMicroseconds(pulseWidth);
            digitalWrite(CC1101_GDO0_PIN, LOW);
            delayMicroseconds(pulseWidth * 3);
        }
    }

    digitalWrite(CC1101_GDO0_PIN, LOW);
    transmitting = false;
    signalsSent++;

    return true;
}

void SubGHzInterface::setTxPower(int8_t power) {
    // CC1101 power levels: -30 to +10 dBm
    if (power < -30) power = -30;
    if (power > 10) power = 10;
    ELECHOUSE_cc1101.setPA(power);
}

// ============================================================================
// Frequency Analysis
// ============================================================================

int8_t SubGHzInterface::getRSSI() {
    return ELECHOUSE_cc1101.getRssi();
}

int8_t SubGHzInterface::getRSSIAt(uint32_t freq) {
    uint32_t originalFreq = currentFrequency;
    setFrequency(freq);
    delay(1);
    int8_t rssi = getRSSI();
    setFrequency(originalFreq);
    return rssi;
}

void SubGHzInterface::startFrequencyScan(uint32_t startFreq, uint32_t endFreq, uint32_t step) {
    scanResults->clear();

    Serial.println(F("[SubGHz] Starting frequency scan..."));

    for (uint32_t freq = startFreq; freq <= endFreq; freq += step) {
        FrequencyScan scan;
        scan.frequency = freq;
        scan.rssi = getRSSIAt(freq);
        scan.active = (scan.rssi > -70);

        scanResults->add(scan);

        if (scan.active) {
            Serial.print(F("[SubGHz] Activity at "));
            Serial.print(freq / 1000000.0, 3);
            Serial.print(F(" MHz: "));
            Serial.print(scan.rssi);
            Serial.println(F(" dBm"));
        }
    }

    Serial.println(F("[SubGHz] Frequency scan complete"));
}

// ============================================================================
// Protocol Decoding
// ============================================================================

SubGHzProtocol SubGHzInterface::decodeProtocol(SubGHzSignal* signal) {
    // Try known protocols
    if (decodePrinceton(signal)) return PROTO_PRINCETON;
    if (decodeCame(signal)) return PROTO_CAME;
    if (decodeNiceFlo(signal)) return PROTO_NICE_FLO;

    return PROTO_RAW;
}

bool SubGHzInterface::decodePrinceton(SubGHzSignal* signal) {
    // Princeton PT2262/PT2272
    // Timing: short pulse ~350us, long pulse ~1050us, sync ~10850us
    // Encoding: 0 = short-long-short-long, 1 = long-short-long-short

    if (signal->rawLength < 48) return false;  // Need at least 12 bits * 4

    // Check for sync pulse at end
    uint16_t lastPulse = signal->rawData[signal->rawLength - 1];
    if (lastPulse < 8000 || lastPulse > 14000) return false;

    // Decode bits
    uint32_t code = 0;
    for (uint16_t i = 0; i < signal->rawLength - 1; i += 4) {
        uint16_t p1 = signal->rawData[i];
        uint16_t p2 = signal->rawData[i + 1];
        uint16_t p3 = signal->rawData[i + 2];
        uint16_t p4 = signal->rawData[i + 3];

        code <<= 1;

        // short-long-short-long = 0
        // long-short-long-short = 1
        if (p1 > 700) {
            code |= 1;
        }
    }

    signal->code = code;
    signal->bits = (signal->rawLength - 1) / 4;
    signal->protocol = PROTO_PRINCETON;
    signal->protocolName = "Princeton PT2262";
    signal->decoded = true;

    return true;
}

bool SubGHzInterface::decodeCame(SubGHzSignal* signal) {
    // CAME TOP protocol
    // Timing: short ~320us, long ~640us
    // 12-bit code

    if (signal->rawLength < 24) return false;

    // Check pulse timings match CAME
    uint16_t firstPulse = signal->rawData[0];
    if (firstPulse < 200 || firstPulse > 800) return false;

    // Decode using Manchester-like encoding
    uint32_t code = 0;
    for (uint16_t i = 0; i < signal->rawLength; i += 2) {
        code <<= 1;
        if (signal->rawData[i] > signal->rawData[i + 1]) {
            code |= 1;
        }
    }

    signal->code = code;
    signal->bits = signal->rawLength / 2;
    signal->protocol = PROTO_CAME;
    signal->protocolName = "CAME TOP";
    signal->decoded = true;

    return true;
}

bool SubGHzInterface::decodeNiceFlo(SubGHzSignal* signal) {
    // Nice FLO protocol
    // Timing: short ~700us, long ~1400us

    if (signal->rawLength < 24) return false;

    uint16_t firstPulse = signal->rawData[0];
    if (firstPulse < 500 || firstPulse > 900) return false;

    uint32_t code = 0;
    for (uint16_t i = 0; i < signal->rawLength; i += 2) {
        code <<= 1;
        if (signal->rawData[i] > 1000) {
            code |= 1;
        }
    }

    signal->code = code;
    signal->bits = signal->rawLength / 2;
    signal->protocol = PROTO_NICE_FLO;
    signal->protocolName = "Nice FLO";
    signal->decoded = true;

    return true;
}

bool SubGHzInterface::decodeTPMS(SubGHzSignal* signal, TPMSSensor* sensor) {
    // Basic TPMS decoding (varies by manufacturer)
    // This is a simplified implementation

    if (signal->rawLength < 100) return false;

    // TPMS typically uses Manchester or similar encoding
    // Sensor ID is usually first 28-32 bits

    memset(sensor, 0, sizeof(TPMSSensor));
    sensor->sensorId = (uint32_t)(signal->code & 0xFFFFFFFF);

    // Pressure and temp are manufacturer-specific
    // Would need protocol-specific decoding

    return true;
}

// ============================================================================
// Rolling Code Detection
// ============================================================================

bool SubGHzInterface::isRollingCode(SubGHzSignal* signal) {
    // Heuristics for rolling code detection:
    // 1. Code length (KeeLoq uses 66+ bits)
    // 2. Manufacturer patterns
    // 3. Known rolling code protocols

    // KeeLoq typically uses 66 bits
    if (signal->bits >= 64) {
        return true;
    }

    // Nice FLOR-S uses 52 bits with rolling component
    if (signal->protocol == PROTO_NICE_FLOR_S) {
        return true;
    }

    // Check for preamble patterns common in rolling codes
    // (simplified heuristic)

    return false;
}

String SubGHzInterface::analyzeRollingCode(SubGHzSignal* signal) {
    String analysis = "";

    analysis += "╔══════════════════════════════════════╗\n";
    analysis += "║    ROLLING CODE DETECTION WARNING    ║\n";
    analysis += "╚══════════════════════════════════════╝\n\n";

    analysis += "This signal appears to use a ROLLING CODE system.\n\n";

    analysis += "What this means:\n";
    analysis += "• Each transmission uses a unique code\n";
    analysis += "• Captured signals CANNOT be replayed\n";
    analysis += "• The transmitter and receiver are synchronized\n\n";

    analysis += "Common systems using rolling codes:\n";
    analysis += "• Garage door openers (post-1995)\n";
    analysis += "• Car key fobs\n";
    analysis += "• Gate openers (modern)\n\n";

    if (signal->bits >= 64) {
        analysis += "Detected pattern: Likely KeeLoq or similar\n";
        analysis += "• 64+ bit encrypted portion\n";
        analysis += "• Synchronized counter between TX/RX\n";
    }

    analysis += "\nSECURITY NOTE:\n";
    analysis += "Rolling code systems are designed to be secure.\n";
    analysis += "Attempting to bypass requires the manufacturer's\n";
    analysis += "encryption key and is not supported by this tool.\n";

    return analysis;
}

// ============================================================================
// Brute Force (Static Codes Only)
// ============================================================================

void SubGHzInterface::startBruteForce(uint8_t bits, uint16_t pulseWidth) {
    bruteForceBits = bits;
    bruteForcePulse = pulseWidth;
    bruteForceCode = 0;
    bruteForceMax = (1ULL << bits) - 1;
    bruteForceActive = true;
    lastBruteTime = millis();

    Serial.println(F("[SubGHz] Starting brute force..."));
    Serial.print(F("[SubGHz] Bits: "));
    Serial.print(bits);
    Serial.print(F(", Codes to try: "));
    Serial.println((uint32_t)bruteForceMax);
    Serial.println(F("[SubGHz] WARNING: This only works on STATIC codes!"));
}

void SubGHzInterface::stopBruteForce() {
    bruteForceActive = false;
    Serial.println(F("[SubGHz] Brute force stopped"));
    Serial.print(F("[SubGHz] Tested "));
    Serial.print((uint32_t)bruteForceCode);
    Serial.println(F(" codes"));
}

bool SubGHzInterface::sendNextBruteCode() {
    if (bruteForceCode > bruteForceMax) {
        return false;
    }

    transmitCode(bruteForceCode, bruteForceBits, bruteForcePulse);

    // Progress indicator every 1000 codes
    if (bruteForceCode % 1000 == 0) {
        Serial.print(F("[SubGHz] Progress: "));
        Serial.print((uint32_t)bruteForceCode);
        Serial.print(F("/"));
        Serial.println((uint32_t)bruteForceMax);
    }

    bruteForceCode++;
    return true;
}

uint32_t SubGHzInterface::getBruteForceProgress() {
    return (uint32_t)bruteForceCode;
}

// ============================================================================
// Analysis & Reporting
// ============================================================================

String SubGHzInterface::analyzeSignal(SubGHzSignal* signal) {
    String analysis = "";

    analysis += "╔══════════════════════════════════════╗\n";
    analysis += "║   SUB-GHZ SIGNAL ANALYSIS REPORT     ║\n";
    analysis += "╚══════════════════════════════════════╝\n\n";

    analysis += "SIGNAL PARAMETERS\n";
    analysis += "─────────────────\n";
    analysis += "  Frequency:   " + getFrequencyString(signal->frequency) + "\n";
    analysis += "  Modulation:  " + getModulationName(signal->modulation) + "\n";
    analysis += "  RSSI:        " + String(signal->rssi) + " dBm\n";
    analysis += "  Raw pulses:  " + String(signal->rawLength) + "\n";
    analysis += "\n";

    analysis += "PROTOCOL ANALYSIS\n";
    analysis += "─────────────────\n";
    if (signal->decoded) {
        analysis += "  Status:      DECODED\n";
        analysis += "  Protocol:    " + signal->protocolName + "\n";
        analysis += "  Code:        0x" + String((uint32_t)signal->code, HEX) + "\n";
        analysis += "  Bits:        " + String(signal->bits) + "\n";
    } else {
        analysis += "  Status:      UNKNOWN PROTOCOL\n";
        analysis += "  Raw capture available for replay\n";
    }
    analysis += "\n";

    if (signal->isRollingCode) {
        analysis += "⚠️ ROLLING CODE WARNING ⚠️\n";
        analysis += "─────────────────────────\n";
        analysis += "  This signal uses a rolling code system.\n";
        analysis += "  Replay attacks will NOT work.\n";
        analysis += "\n";
    }

    analysis += "SECURITY ASSESSMENT\n";
    analysis += "───────────────────\n";
    if (signal->isRollingCode) {
        analysis += "  Risk Level:  LOW (rolling code)\n";
        analysis += "  Cloneable:   NO\n";
    } else if (signal->decoded) {
        analysis += "  Risk Level:  HIGH (static code)\n";
        analysis += "  Cloneable:   YES\n";
        analysis += "  Replay:      POSSIBLE\n";
    } else {
        analysis += "  Risk Level:  MEDIUM (unknown)\n";
        analysis += "  Cloneable:   MAYBE (raw replay)\n";
    }

    return analysis;
}

// ============================================================================
// UI Helpers
// ============================================================================

String SubGHzInterface::getFrequencyString(uint32_t freq) {
    return String(freq / 1000000.0, 3) + " MHz";
}

String SubGHzInterface::getModulationName(SubGHzModulation mod) {
    switch (mod) {
        case MOD_ASK_OOK: return "ASK/OOK";
        case MOD_2FSK:    return "2-FSK";
        case MOD_GFSK:    return "GFSK";
        case MOD_MSK:     return "MSK";
        case MOD_4FSK:    return "4-FSK";
        default:          return "Unknown";
    }
}

String SubGHzInterface::getProtocolName(SubGHzProtocol proto) {
    switch (proto) {
        case PROTO_PRINCETON:   return "Princeton PT2262";
        case PROTO_CAME:        return "CAME TOP";
        case PROTO_NICE_FLO:    return "Nice FLO";
        case PROTO_NICE_FLOR_S: return "Nice FLOR-S (Rolling)";
        case PROTO_KEELOQ:      return "KeeLoq (Rolling)";
        case PROTO_TPMS:        return "TPMS Sensor";
        case PROTO_WEATHER:     return "Weather Station";
        default:                return "Raw/Unknown";
    }
}

String SubGHzInterface::formatSignal(SubGHzSignal* signal) {
    String output = "";
    output += "Freq: " + getFrequencyString(signal->frequency) + "\n";
    output += "RSSI: " + String(signal->rssi) + " dBm\n";

    if (signal->decoded) {
        output += "Protocol: " + signal->protocolName + "\n";
        output += "Code: 0x" + String((uint32_t)signal->code, HEX) + "\n";
    } else {
        output += "Protocol: RAW\n";
        output += "Pulses: " + String(signal->rawLength) + "\n";
    }

    return output;
}

// State accessors
uint8_t SubGHzInterface::getCurrentMode() { return currentMode; }
void SubGHzInterface::setCurrentMode(uint8_t mode) {
    currentMode = mode;
    if (mode == SUBGHZ_SCAN_RAW || mode == SUBGHZ_SCAN_CONTINUOUS ||
        mode == SUBGHZ_SCAN_DECODE || mode == SUBGHZ_SCAN_TPMS) {
        startReceive();
    } else if (mode == SUBGHZ_SCAN_OFF) {
        stopReceive();
    }
}
bool SubGHzInterface::isTransmitting() { return transmitting; }
uint32_t SubGHzInterface::getSignalsCaptured() { return signalsCaptured; }

#endif // HAS_SUBGHZ

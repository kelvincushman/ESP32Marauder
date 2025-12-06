/*
 * IRInterface.cpp
 * Infrared Security Testing Module Implementation
 *
 * For authorized penetration testing only.
 */

#include "IRInterface.h"

#ifdef HAS_IR

IRInterface ir_obj;

// Receive buffer size
const uint16_t IR_CAPTURE_BUFFER_SIZE = 1024;
const uint8_t IR_TIMEOUT_MS = 50;

// ============================================================================
// Lifecycle Methods
// ============================================================================

void IRInterface::RunSetup() {
    Serial.println(F("[IR] Initializing IR security module..."));

    // Initialize receiver
    irrecv = new IRrecv(IR_RX_PIN, IR_CAPTURE_BUFFER_SIZE, IR_TIMEOUT_MS, true);

    // Initialize transmitter
    irsend = new IRsend(IR_TX_PIN);
    irsend->begin();

    // Initialize state
    currentMode = IR_SCAN_OFF;
    receiving = false;
    transmitting = false;
    lastActivityTime = 0;

    // Initialize data structures
    capturedSignals = new LinkedList<IRSignal>();
    memset(&lastSignal, 0, sizeof(IRSignal));

    // Brute force state
    bruteForceActive = false;
    bruteForceIndex = 0;
    bruteDelayMs = 100;

    // Statistics
    signalsCaptured = 0;
    signalsSent = 0;
    protocolsDecoded = 0;

    Serial.println(F("[IR] Module ready"));
    Serial.print(F("[IR] RX Pin: "));
    Serial.print(IR_RX_PIN);
    Serial.print(F(", TX Pin: "));
    Serial.println(IR_TX_PIN);
}

void IRInterface::main(uint32_t currentTime) {
    if (currentMode == IR_SCAN_OFF) {
        return;
    }

    switch (currentMode) {
        case IR_SCAN_RECEIVE:
        case IR_SCAN_CONTINUOUS:
            if (receiving && irrecv->decode(&results)) {
                IRSignal signal;
                if (captureSignal(&signal)) {
                    signalsCaptured++;
                    lastSignal = signal;

                    // Print to serial
                    Serial.println(F("\n[IR] Signal captured!"));
                    Serial.println(formatSignal(&signal));

                    if (currentMode == IR_SCAN_CONTINUOUS) {
                        capturedSignals->add(signal);
                    }
                }
                irrecv->resume();
            }
            break;

        case IR_SCAN_ANALYZE:
            if (receiving && irrecv->decode(&results)) {
                IRSignal signal;
                if (captureSignal(&signal)) {
                    signalsCaptured++;
                    lastSignal = signal;

                    Serial.println(F("\n[IR] ═══ Signal Analysis ═══"));
                    Serial.println(analyzeSignal(&signal));
                }
                irrecv->resume();
            }
            break;

        case IR_ATTACK_BRUTE:
            if (bruteForceActive) {
                if (currentTime - lastBruteTime >= bruteDelayMs) {
                    if (!sendNextBruteCode()) {
                        bruteForceActive = false;
                        Serial.println(F("[IR] Brute force complete"));
                    }
                    lastBruteTime = currentTime;
                }
            }
            break;

        case IR_ATTACK_JAMMER:
            // Send random noise at high frequency
            if (currentTime - lastActivityTime >= 10) {
                uint16_t noise[20];
                for (int i = 0; i < 20; i++) {
                    noise[i] = random(100, 1000);
                }
                transmitRaw(noise, 20, 38);
                lastActivityTime = currentTime;
            }
            break;

        default:
            break;
    }
}

void IRInterface::shutdown() {
    stopReceive();
    currentMode = IR_SCAN_OFF;

    delete irrecv;
    delete irsend;
    delete capturedSignals;

    Serial.println(F("[IR] Module shutdown"));
}

// ============================================================================
// Receive Operations
// ============================================================================

bool IRInterface::startReceive() {
    irrecv->enableIRIn();
    receiving = true;
    Serial.println(F("[IR] Receiver enabled - waiting for signals..."));
    return true;
}

void IRInterface::stopReceive() {
    irrecv->disableIRIn();
    receiving = false;
    Serial.println(F("[IR] Receiver disabled"));
}

bool IRInterface::hasSignal() {
    return irrecv->decode(&results);
}

bool IRInterface::captureSignal(IRSignal* signal) {
    memset(signal, 0, sizeof(IRSignal));

    signal->protocol = results.decode_type;
    signal->protocolName = typeToString(results.decode_type);
    signal->code = results.value;
    signal->bits = results.bits;
    signal->address = results.address;
    signal->command = results.command;
    signal->captureTime = millis();
    signal->decoded = (results.decode_type != decode_type_t::UNKNOWN);

    if (signal->decoded) {
        protocolsDecoded++;
    }

    // Copy raw data
    signal->rawLength = results.rawlen;
    if (signal->rawLength > 0) {
        signal->rawData = (uint16_t*)malloc(signal->rawLength * sizeof(uint16_t));
        if (signal->rawData) {
            for (uint16_t i = 0; i < signal->rawLength; i++) {
                signal->rawData[i] = results.rawbuf[i] * kRawTick;
            }
        }
    }

    // Estimate frequency based on protocol
    switch (signal->protocol) {
        case decode_type_t::SONY:
            signal->frequency = 40;
            break;
        case decode_type_t::RC5:
        case decode_type_t::RC6:
            signal->frequency = 36;
            break;
        default:
            signal->frequency = 38;
            break;
    }

    return true;
}

String IRInterface::decodeProtocol(IRSignal* signal) {
    if (!signal->decoded) {
        return "Unknown protocol - raw capture available";
    }

    String info = "";
    info += "Protocol: " + signal->protocolName + "\n";
    info += "Code: 0x" + String((uint32_t)(signal->code >> 32), HEX) +
            String((uint32_t)signal->code, HEX) + "\n";
    info += "Bits: " + String(signal->bits) + "\n";

    if (signal->address || signal->command) {
        info += "Address: 0x" + String(signal->address, HEX) + "\n";
        info += "Command: 0x" + String(signal->command, HEX) + "\n";
    }

    return info;
}

// ============================================================================
// Transmit Operations
// ============================================================================

bool IRInterface::transmitSignal(IRSignal* signal) {
    if (!signal) return false;

    bool success = false;

    if (signal->decoded && signal->protocol != decode_type_t::UNKNOWN) {
        // Send using protocol
        success = transmitCode(signal->protocol, signal->code, signal->bits);
    } else if (signal->rawData && signal->rawLength > 0) {
        // Send raw
        success = transmitRaw(signal->rawData, signal->rawLength, signal->frequency);
    }

    if (success) {
        signalsSent++;
        Serial.println(F("[IR] Signal transmitted"));
    }

    return success;
}

bool IRInterface::transmitRaw(uint16_t* data, uint16_t length, uint16_t freq) {
    irsend->sendRaw(data, length, freq);
    signalsSent++;
    return true;
}

bool IRInterface::transmitCode(decode_type_t protocol, uint64_t code, uint16_t bits) {
    bool success = irsend->send(protocol, code, bits);
    if (success) {
        signalsSent++;
    }
    return success;
}

bool IRInterface::transmitNEC(uint32_t address, uint32_t command) {
    irsend->sendNEC(irsend->encodeNEC(address, command));
    signalsSent++;
    return true;
}

bool IRInterface::transmitSony(uint32_t data, uint16_t bits) {
    irsend->sendSony(data, bits);
    signalsSent++;
    return true;
}

bool IRInterface::transmitRC5(uint32_t data, uint16_t bits) {
    irsend->sendRC5(data, bits);
    signalsSent++;
    return true;
}

bool IRInterface::transmitSamsung(uint64_t data, uint16_t bits) {
    irsend->sendSAMSUNG(data, bits);
    signalsSent++;
    return true;
}

// ============================================================================
// Attack Modes
// ============================================================================

void IRInterface::startBruteForce(String category) {
    bruteForceCategory = category;
    bruteForceIndex = 0;
    bruteForceActive = true;
    lastBruteTime = millis();

    Serial.print(F("[IR] Starting brute force: "));
    Serial.println(category);
    Serial.print(F("[IR] Codes to try: "));
    Serial.println(NUM_TV_POWER_CODES);
}

void IRInterface::stopBruteForce() {
    bruteForceActive = false;
    Serial.println(F("[IR] Brute force stopped"));
    Serial.print(F("[IR] Tried "));
    Serial.print(bruteForceIndex);
    Serial.println(F(" codes"));
}

bool IRInterface::sendNextBruteCode() {
    if (bruteForceIndex >= NUM_TV_POWER_CODES) {
        return false;
    }

    // Read code from PROGMEM
    IRPowerCode code;
    memcpy_P(&code, &TV_POWER_CODES[bruteForceIndex], sizeof(IRPowerCode));

    // Transmit
    Serial.print(F("[IR] Trying: "));
    Serial.print(code.brand);
    Serial.print(F(" - "));
    Serial.println(code.model);

    transmitCode(code.protocol, code.code, code.bits);

    bruteForceIndex++;
    return true;
}

void IRInterface::startJamming() {
    currentMode = IR_ATTACK_JAMMER;
    lastActivityTime = millis();
    Serial.println(F("[IR] WARNING: IR jamming active"));
    Serial.println(F("[IR] This will disrupt IR devices in range"));
}

void IRInterface::stopJamming() {
    currentMode = IR_SCAN_OFF;
    Serial.println(F("[IR] Jamming stopped"));
}

// ============================================================================
// Signal Analysis
// ============================================================================

String IRInterface::analyzeSignal(IRSignal* signal) {
    String analysis = "";

    analysis += "╔══════════════════════════════════════╗\n";
    analysis += "║      IR SIGNAL ANALYSIS REPORT       ║\n";
    analysis += "╚══════════════════════════════════════╝\n\n";

    // Basic info
    analysis += "SIGNAL IDENTIFICATION\n";
    analysis += "─────────────────────\n";

    if (signal->decoded) {
        analysis += "  Status:     DECODED\n";
        analysis += "  Protocol:   " + signal->protocolName + "\n";
        analysis += "  Code:       0x" + String((uint32_t)signal->code, HEX) + "\n";
        analysis += "  Bits:       " + String(signal->bits) + "\n";

        if (signal->address || signal->command) {
            analysis += "  Address:    0x" + String(signal->address, HEX) + "\n";
            analysis += "  Command:    0x" + String(signal->command, HEX) + "\n";
        }
    } else {
        analysis += "  Status:     UNKNOWN PROTOCOL\n";
        analysis += "  Raw Length: " + String(signal->rawLength) + " samples\n";
    }

    analysis += "  Frequency:  " + String(signal->frequency) + " kHz\n";
    analysis += "\n";

    // Device identification attempt
    analysis += "DEVICE IDENTIFICATION\n";
    analysis += "─────────────────────\n";
    analysis += "  " + identifyDevice(signal) + "\n\n";

    // Raw timing analysis
    if (signal->rawData && signal->rawLength > 0) {
        analysis += "TIMING ANALYSIS\n";
        analysis += "───────────────\n";

        // Calculate timing statistics
        uint32_t minMark = UINT32_MAX, maxMark = 0;
        uint32_t minSpace = UINT32_MAX, maxSpace = 0;
        uint32_t totalMark = 0, totalSpace = 0;
        uint16_t markCount = 0, spaceCount = 0;

        for (uint16_t i = 1; i < signal->rawLength; i++) {
            if (i % 2 == 1) {  // Mark
                if (signal->rawData[i] < minMark) minMark = signal->rawData[i];
                if (signal->rawData[i] > maxMark) maxMark = signal->rawData[i];
                totalMark += signal->rawData[i];
                markCount++;
            } else {  // Space
                if (signal->rawData[i] < minSpace) minSpace = signal->rawData[i];
                if (signal->rawData[i] > maxSpace) maxSpace = signal->rawData[i];
                totalSpace += signal->rawData[i];
                spaceCount++;
            }
        }

        analysis += "  Marks:  min=" + String(minMark) + "us, max=" + String(maxMark) +
                    "us, avg=" + String(totalMark / markCount) + "us\n";
        analysis += "  Spaces: min=" + String(minSpace) + "us, max=" + String(maxSpace) +
                    "us, avg=" + String(totalSpace / spaceCount) + "us\n";
        analysis += "  Total duration: " + String((totalMark + totalSpace) / 1000) + "ms\n";
    }

    // Security notes
    analysis += "\nSECURITY NOTES\n";
    analysis += "──────────────\n";

    if (signal->decoded) {
        analysis += "  • This signal uses a known protocol\n";
        analysis += "  • Can be easily replayed\n";
        analysis += "  • No rolling code protection detected\n";

        if (signal->protocol == decode_type_t::NEC ||
            signal->protocol == decode_type_t::SAMSUNG) {
            analysis += "  • Common consumer electronics protocol\n";
        }
    } else {
        analysis += "  • Unknown/proprietary protocol\n";
        analysis += "  • Raw replay may still work\n";
        analysis += "  • May use custom encoding\n";
    }

    return analysis;
}

String IRInterface::identifyDevice(IRSignal* signal) {
    // Try to identify device based on protocol and code patterns
    switch (signal->protocol) {
        case decode_type_t::NEC:
            // NEC address field often identifies manufacturer
            if ((signal->address & 0xFF) == 0x04) return "Samsung TV (likely)";
            if ((signal->address & 0xFF) == 0x20) return "LG TV (likely)";
            if ((signal->address & 0xFF) == 0x40) return "Philips device";
            return "NEC-compatible device";

        case decode_type_t::SAMSUNG:
            return "Samsung TV or appliance";

        case decode_type_t::SONY:
            if (signal->bits == 12) return "Sony TV (12-bit)";
            if (signal->bits == 15) return "Sony DVD/Blu-ray";
            if (signal->bits == 20) return "Sony AV Receiver";
            return "Sony device";

        case decode_type_t::RC5:
        case decode_type_t::RC6:
            return "Philips/European device (RC5/RC6)";

        case decode_type_t::PANASONIC:
            return "Panasonic device";

        case decode_type_t::LG:
            return "LG device";

        case decode_type_t::COOLIX:
        case decode_type_t::DAIKIN:
        case decode_type_t::MITSUBISHI_AC:
            return "Air Conditioner";

        default:
            return "Unknown device type";
    }
}

// ============================================================================
// Storage Operations
// ============================================================================

bool IRInterface::saveSignalToSD(IRSignal* signal, String filename) {
    #ifdef HAS_SD
    File file = SD.open("/ir/" + filename + ".ir", FILE_WRITE);
    if (!file) {
        Serial.println(F("[IR] Failed to open file for writing"));
        return false;
    }

    // Write header
    file.println(F("# ESP32 Marauder IR Signal"));
    file.println(F("# Format: Flipper-compatible"));
    file.println();

    // Write metadata
    file.print(F("Protocol: "));
    file.println(signal->protocolName);

    if (signal->decoded) {
        file.print(F("Code: 0x"));
        file.println(String((uint32_t)signal->code, HEX));
        file.print(F("Bits: "));
        file.println(signal->bits);
    }

    file.print(F("Frequency: "));
    file.println(signal->frequency);

    // Write raw data
    file.print(F("Raw_data: "));
    for (uint16_t i = 0; i < signal->rawLength; i++) {
        file.print(signal->rawData[i]);
        if (i < signal->rawLength - 1) file.print(F(" "));
    }
    file.println();

    file.close();
    Serial.print(F("[IR] Signal saved to "));
    Serial.println(filename);
    return true;
    #else
    return false;
    #endif
}

// ============================================================================
// UI Helpers
// ============================================================================

String IRInterface::formatSignal(IRSignal* signal) {
    String output = "";

    if (signal->decoded) {
        output += "Protocol: " + signal->protocolName + "\n";
        output += "Code: 0x" + String((uint32_t)signal->code, HEX);
        output += " (" + String(signal->bits) + " bits)\n";
    } else {
        output += "Protocol: RAW (unknown)\n";
        output += "Length: " + String(signal->rawLength) + " samples\n";
    }

    return output;
}

String IRInterface::formatRawData(uint16_t* data, uint16_t length) {
    String output = "";
    for (uint16_t i = 0; i < length; i++) {
        output += String(data[i]);
        if (i < length - 1) {
            output += (i % 2 == 0) ? "," : " ";
        }
        if ((i + 1) % 16 == 0) output += "\n";
    }
    return output;
}

String IRInterface::getProtocolName(decode_type_t protocol) {
    return typeToString(protocol);
}

// State accessors
uint8_t IRInterface::getCurrentMode() { return currentMode; }
void IRInterface::setCurrentMode(uint8_t mode) {
    currentMode = mode;
    if (mode == IR_SCAN_RECEIVE || mode == IR_SCAN_ANALYZE ||
        mode == IR_SCAN_CONTINUOUS) {
        startReceive();
    } else {
        stopReceive();
    }
}
uint32_t IRInterface::getSignalsCaptured() { return signalsCaptured; }
uint32_t IRInterface::getSignalsSent() { return signalsSent; }

#endif // HAS_IR

/*
 * SubGHzInterface.h
 * Sub-GHz Radio Security Testing Module for ESP32 Marauder
 *
 * Purpose: Capture, analyze, and replay sub-gigahertz radio signals
 *          for authorized penetration testing.
 *
 * LEGAL NOTICE: Only use on devices you own or have explicit permission to test.
 *               Jamming radio frequencies is illegal in most jurisdictions.
 *               Rolling code systems cannot be bypassed without the original transmitter.
 *
 * Supported Hardware:
 *   - CC1101 transceiver module (primary)
 *   - SX1276/SX1278 LoRa modules (alternative)
 *
 * Supported Frequencies:
 *   - 315 MHz (US garage doors, car remotes)
 *   - 433.92 MHz (EU/Global - most common)
 *   - 868 MHz (EU)
 *   - 915 MHz (US ISM band)
 *
 * Common Protocols:
 *   - Static codes (older garage doors, gates)
 *   - Princeton PT2262/PT2272
 *   - Came TOP, Nice FLO/SMILO
 *   - Car TPMS sensors
 *   - Weather stations
 *   - Wireless doorbells
 */

#pragma once

#ifndef SubGHzInterface_h
#define SubGHzInterface_h

#include "configs.h"

#ifdef HAS_SUBGHZ

#include <SPI.h>
#include <LinkedList.h>

// CC1101 library
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// ============================================================================
// Sub-GHz Scan Mode Constants (120-139 reserved for Sub-GHz)
// ============================================================================
#define SUBGHZ_SCAN_OFF          0
#define SUBGHZ_SCAN_RAW          120  // Raw signal capture
#define SUBGHZ_SCAN_ANALYZE      121  // Frequency analyzer
#define SUBGHZ_SCAN_HOPPING      122  // Channel hopping monitor
#define SUBGHZ_ATTACK_REPLAY     123  // Replay captured signal
#define SUBGHZ_ATTACK_BRUTE      124  // Brute force static codes
#define SUBGHZ_SCAN_CONTINUOUS   125  // Log all signals
#define SUBGHZ_SCAN_RSSI         126  // Signal strength monitor
#define SUBGHZ_SCAN_DECODE       127  // Protocol decoder
#define SUBGHZ_SCAN_TPMS         128  // TPMS sensor scanner

// ============================================================================
// Frequency Presets
// ============================================================================
enum SubGHzFrequency {
    FREQ_315_MHZ = 0,      // 315.000 MHz - US
    FREQ_433_MHZ,          // 433.920 MHz - EU/Global
    FREQ_433_42_MHZ,       // 433.420 MHz - Alternative
    FREQ_868_MHZ,          // 868.350 MHz - EU
    FREQ_915_MHZ,          // 915.000 MHz - US ISM
    FREQ_CUSTOM            // User-defined
};

// Frequency values in Hz
const uint32_t FREQUENCY_VALUES[] = {
    315000000,   // 315 MHz
    433920000,   // 433.92 MHz
    433420000,   // 433.42 MHz
    868350000,   // 868.35 MHz
    915000000    // 915 MHz
};

// ============================================================================
// Modulation Types
// ============================================================================
enum SubGHzModulation {
    MOD_ASK_OOK = 0,       // On-Off Keying (most common)
    MOD_2FSK,              // 2-level FSK
    MOD_GFSK,              // Gaussian FSK
    MOD_MSK,               // Minimum Shift Keying
    MOD_4FSK               // 4-level FSK
};

// ============================================================================
// Known Protocol Types
// ============================================================================
enum SubGHzProtocol {
    PROTO_UNKNOWN = 0,
    PROTO_RAW,
    PROTO_PRINCETON,       // PT2262/PT2272
    PROTO_CAME,            // CAME TOP
    PROTO_NICE_FLO,        // Nice FLO
    PROTO_NICE_FLOR_S,     // Nice FLOR-S (rolling)
    PROTO_GATE_TX,         // Generic gate transmitter
    PROTO_KEELOQ,          // KeeLoq rolling code (info only)
    PROTO_DOORHAN,         // Doorhan
    PROTO_HORMANN,         // Hormann HSM
    PROTO_LINEAR,          // Linear delta-3
    PROTO_CHAMBERLAIN,     // Chamberlain (rolling)
    PROTO_TPMS,            // Tire pressure sensors
    PROTO_WEATHER,         // Weather stations
    PROTO_HONEYWELL,       // Security sensors
    PROTO_OREGON           // Oregon Scientific
};

// ============================================================================
// Structures
// ============================================================================

// Captured radio signal
struct SubGHzSignal {
    // Frequency and modulation
    uint32_t frequency;
    SubGHzModulation modulation;
    uint32_t bandwidth;
    uint32_t deviation;

    // Protocol info
    SubGHzProtocol protocol;
    String protocolName;
    uint64_t code;
    uint8_t bits;
    uint32_t serialNumber;

    // Raw timing data (OOK/ASK)
    uint16_t* rawData;
    uint16_t rawLength;
    uint16_t pulseDuration;   // Typical pulse width

    // Signal quality
    int8_t rssi;
    uint8_t lqi;              // Link quality indicator
    float snr;                // Signal-to-noise ratio

    // Metadata
    uint32_t captureTime;
    bool decoded;
    bool isRollingCode;       // Warning flag
    uint8_t repeatCount;      // For brute force resistance
};

// Frequency scan result
struct FrequencyScan {
    uint32_t frequency;
    int8_t rssi;
    bool active;
};

// TPMS sensor data
struct TPMSSensor {
    uint32_t sensorId;
    float pressure;           // PSI or kPa
    float temperature;        // Celsius
    bool lowPressure;
    bool lowBattery;
    uint32_t lastSeen;
};

// ============================================================================
// Sub-GHz Interface Class
// ============================================================================
class SubGHzInterface {
public:
    // Lifecycle
    void RunSetup();
    void main(uint32_t currentTime);
    void shutdown();

    // Configuration
    void setFrequency(uint32_t freq);
    void setFrequencyPreset(SubGHzFrequency preset);
    void setModulation(SubGHzModulation mod);
    void setBandwidth(uint32_t bw);
    void setDeviation(uint32_t dev);
    uint32_t getFrequency();

    // Receive Operations
    bool startReceive();
    void stopReceive();
    bool hasSignal();
    bool captureSignal(SubGHzSignal* signal);

    // Transmit Operations
    bool transmitSignal(SubGHzSignal* signal);
    bool transmitRaw(uint16_t* data, uint16_t length);
    bool transmitCode(uint64_t code, uint8_t bits, uint16_t pulseWidth);
    void setTxPower(int8_t power);  // -30 to +10 dBm

    // Frequency Analysis
    void startFrequencyScan(uint32_t startFreq, uint32_t endFreq, uint32_t step);
    void stopFrequencyScan();
    int8_t getRSSI();
    int8_t getRSSIAt(uint32_t freq);
    LinkedList<FrequencyScan>* getFrequencyScanResults();

    // Protocol Decoding
    SubGHzProtocol decodeProtocol(SubGHzSignal* signal);
    bool decodePrinceton(SubGHzSignal* signal);
    bool decodeCame(SubGHzSignal* signal);
    bool decodeNiceFlo(SubGHzSignal* signal);
    bool decodeTPMS(SubGHzSignal* signal, TPMSSensor* sensor);

    // Attack Modes
    void startBruteForce(uint8_t bits, uint16_t pulseWidth);
    void stopBruteForce();
    bool sendNextBruteCode();
    uint32_t getBruteForceProgress();

    // Rolling Code Detection
    bool isRollingCode(SubGHzSignal* signal);
    String analyzeRollingCode(SubGHzSignal* signal);

    // Storage
    bool saveSignalToSD(SubGHzSignal* signal, String filename);
    bool loadSignalFromSD(SubGHzSignal* signal, String filename);

    // Analysis
    String analyzeSignal(SubGHzSignal* signal);
    String identifyProtocol(SubGHzSignal* signal);

    // UI Helpers
    String getFrequencyString(uint32_t freq);
    String getModulationName(SubGHzModulation mod);
    String getProtocolName(SubGHzProtocol proto);
    String formatSignal(SubGHzSignal* signal);

    // State
    uint8_t getCurrentMode();
    void setCurrentMode(uint8_t mode);
    bool isTransmitting();
    uint32_t getSignalsCaptured();

private:
    // Hardware state
    bool initialized;
    bool receiving;
    bool transmitting;
    uint32_t currentFrequency;
    SubGHzModulation currentModulation;

    // Scan state
    uint8_t currentMode;
    uint32_t lastActivityTime;
    uint32_t scanInterval;

    // Data
    LinkedList<SubGHzSignal>* capturedSignals;
    LinkedList<FrequencyScan>* scanResults;
    LinkedList<TPMSSensor>* tpmsSensors;
    SubGHzSignal lastSignal;

    // Brute force
    bool bruteForceActive;
    uint64_t bruteForceCode;
    uint64_t bruteForceMax;
    uint8_t bruteForceBits;
    uint16_t bruteForcePulse;
    uint32_t lastBruteTime;
    uint16_t bruteDelayMs;

    // Statistics
    uint32_t signalsCaptured;
    uint32_t signalsSent;
    uint32_t rollingCodesDetected;

    // Helpers
    void initCC1101();
    bool detectCarrier();
    void processRawPulses(uint16_t* pulses, uint16_t count, SubGHzSignal* signal);
    uint64_t decodeOOK(uint16_t* pulses, uint16_t count, uint16_t shortPulse, uint16_t longPulse);
    void sendOOK(uint64_t code, uint8_t bits, uint16_t shortPulse, uint16_t longPulse);
};

// ============================================================================
// Common Protocol Timings (microseconds)
// ============================================================================
namespace SubGHzTimings {
    // Princeton PT2262
    const uint16_t PRINCETON_SHORT = 350;
    const uint16_t PRINCETON_LONG = 1050;
    const uint16_t PRINCETON_SYNC = 10850;

    // CAME TOP
    const uint16_t CAME_SHORT = 320;
    const uint16_t CAME_LONG = 640;
    const uint16_t CAME_SYNC = 11520;

    // Nice FLO
    const uint16_t NICE_SHORT = 700;
    const uint16_t NICE_LONG = 1400;
    const uint16_t NICE_SYNC = 25200;

    // Generic OOK
    const uint16_t OOK_SHORT_MIN = 200;
    const uint16_t OOK_SHORT_MAX = 600;
    const uint16_t OOK_LONG_MIN = 600;
    const uint16_t OOK_LONG_MAX = 1500;
}

// ============================================================================
// Rolling Code Warning Messages
// ============================================================================
namespace RollingCodeWarnings {
    const char KEELOQ_WARNING[] PROGMEM =
        "KeeLoq rolling code detected. This signal changes with each use "
        "and cannot be replayed. The manufacturer's key would be needed "
        "to generate valid codes.";

    const char NICE_SMILO_WARNING[] PROGMEM =
        "Nice FLOR-S rolling code detected. Each button press generates "
        "a unique code. Replay attacks will not work.";

    const char GENERAL_WARNING[] PROGMEM =
        "Rolling code system detected. These are designed to prevent "
        "replay attacks. Only the original transmitter can generate valid codes.";
}

extern SubGHzInterface subghz_obj;

#endif // HAS_SUBGHZ
#endif // SubGHzInterface_h

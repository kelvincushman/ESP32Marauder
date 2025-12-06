/*
 * IRInterface.h
 * Infrared Security Testing Module for ESP32 Marauder
 *
 * Purpose: Capture, analyze, and replay infrared signals for
 *          authorized penetration testing of IR-controlled devices.
 *
 * LEGAL NOTICE: Only use on devices you own or have permission to test.
 *
 * Supported Protocols:
 *   - NEC, Sony, RC5, RC6, Samsung, LG, Panasonic, JVC
 *   - Denon, Sharp, Whynter, Coolix, Daikin, Toshiba
 *   - Raw signal capture for unknown protocols
 *
 * Hardware Requirements:
 *   - IR LED (940nm) for transmission
 *   - IR Receiver (TSOP38238, VS1838B) for capture
 */

#pragma once

#ifndef IRInterface_h
#define IRInterface_h

#include "configs.h"

#ifdef HAS_IR

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>
#include <LinkedList.h>

// ============================================================================
// IR Scan Mode Constants (100-119 reserved for IR)
// ============================================================================
#define IR_SCAN_OFF           0
#define IR_SCAN_RECEIVE       100  // Capture incoming signals
#define IR_SCAN_ANALYZE       101  // Decode and analyze protocol
#define IR_ATTACK_REPLAY      102  // Replay captured signal
#define IR_ATTACK_BRUTE       103  // Brute force (TV-B-Gone style)
#define IR_ATTACK_CUSTOM      104  // Send custom code
#define IR_SCAN_CONTINUOUS    105  // Log all signals
#define IR_ATTACK_JAMMER      106  // IR jamming (flood with noise)
#define IR_SCAN_AC_DECODE     107  // Decode AC remote protocols

// ============================================================================
// Known Protocol Types
// ============================================================================
enum IRProtocolType {
    IR_PROTO_UNKNOWN = 0,
    IR_PROTO_NEC,
    IR_PROTO_SONY,
    IR_PROTO_RC5,
    IR_PROTO_RC6,
    IR_PROTO_SAMSUNG,
    IR_PROTO_LG,
    IR_PROTO_PANASONIC,
    IR_PROTO_JVC,
    IR_PROTO_DENON,
    IR_PROTO_SHARP,
    IR_PROTO_AIWA,
    IR_PROTO_MITSUBISHI,
    IR_PROTO_PIONEER,
    IR_PROTO_RAW           // Raw capture when protocol unknown
};

// ============================================================================
// Structures for Signal Data
// ============================================================================

// Captured IR signal
struct IRSignal {
    // Protocol info
    decode_type_t protocol;
    String protocolName;
    uint64_t code;
    uint16_t bits;
    uint16_t address;
    uint16_t command;

    // Raw timing data
    uint16_t* rawData;
    uint16_t rawLength;
    uint16_t frequency;

    // Metadata
    uint32_t captureTime;
    int8_t signalStrength;  // Based on timing consistency
    bool decoded;
    String deviceType;      // TV, AC, etc. if identifiable
};

// TV-B-Gone style database entry
struct IRPowerCode {
    String brand;
    String model;
    decode_type_t protocol;
    uint64_t code;
    uint16_t bits;
    uint16_t frequency;
};

// ============================================================================
// IR Interface Class
// ============================================================================
class IRInterface {
public:
    // Lifecycle
    void RunSetup();
    void main(uint32_t currentTime);
    void shutdown();

    // Receive Operations
    bool startReceive();
    void stopReceive();
    bool hasSignal();
    bool captureSignal(IRSignal* signal);
    String decodeProtocol(IRSignal* signal);

    // Transmit Operations
    bool transmitSignal(IRSignal* signal);
    bool transmitRaw(uint16_t* data, uint16_t length, uint16_t freq);
    bool transmitCode(decode_type_t protocol, uint64_t code, uint16_t bits);
    bool transmitNEC(uint32_t address, uint32_t command);
    bool transmitSony(uint32_t data, uint16_t bits);
    bool transmitRC5(uint32_t data, uint16_t bits);
    bool transmitSamsung(uint64_t data, uint16_t bits);

    // Attack Modes
    void startBruteForce(String category);  // TV, AC, etc.
    void stopBruteForce();
    bool sendNextBruteCode();
    void startJamming();
    void stopJamming();

    // Signal Analysis
    String analyzeSignal(IRSignal* signal);
    String identifyDevice(IRSignal* signal);
    float getFrequency(IRSignal* signal);

    // Storage
    bool saveSignalToSD(IRSignal* signal, String filename);
    bool loadSignalFromSD(IRSignal* signal, String filename);
    String listSavedSignals();

    // Signal Library
    uint16_t getNumPowerCodes();
    bool getPowerCode(uint16_t index, IRPowerCode* code);

    // UI Helpers
    String getProtocolName(decode_type_t protocol);
    String formatSignal(IRSignal* signal);
    String formatRawData(uint16_t* data, uint16_t length);

    // State
    uint8_t getCurrentMode();
    void setCurrentMode(uint8_t mode);
    uint32_t getSignalsCaptured();
    uint32_t getSignalsSent();

private:
    IRrecv* irrecv;
    IRsend* irsend;
    decode_results results;

    // State
    uint8_t currentMode;
    bool receiving;
    bool transmitting;
    uint32_t lastActivityTime;

    // Data
    LinkedList<IRSignal>* capturedSignals;
    IRSignal lastSignal;

    // Brute force state
    bool bruteForceActive;
    uint16_t bruteForceIndex;
    String bruteForceCategory;
    uint32_t lastBruteTime;
    uint16_t bruteDelayMs;

    // Statistics
    uint32_t signalsCaptured;
    uint32_t signalsSent;
    uint32_t protocolsDecoded;

    // Helpers
    void copySignal(IRSignal* dest, IRSignal* src);
    void freeSignal(IRSignal* signal);
    bool allocateRawBuffer(IRSignal* signal, uint16_t length);
};

// ============================================================================
// TV-B-Gone Power Codes Database (subset for space)
// Common power codes for TVs - for testing purposes only
// ============================================================================
const IRPowerCode TV_POWER_CODES[] PROGMEM = {
    // Samsung
    {"Samsung", "TV", decode_type_t::SAMSUNG, 0xE0E040BF, 32, 38},
    {"Samsung", "TV Alt", decode_type_t::SAMSUNG, 0xE0E09966, 32, 38},

    // LG
    {"LG", "TV", decode_type_t::LG, 0x20DF10EF, 32, 38},
    {"LG", "TV Alt", decode_type_t::NEC, 0x20DF10EF, 32, 38},

    // Sony
    {"Sony", "TV", decode_type_t::SONY, 0xA90, 12, 40},
    {"Sony", "TV 15bit", decode_type_t::SONY, 0x540C, 15, 40},
    {"Sony", "TV 20bit", decode_type_t::SONY, 0x1080C, 20, 40},

    // Panasonic
    {"Panasonic", "TV", decode_type_t::PANASONIC, 0x400401003D, 48, 37},

    // Philips
    {"Philips", "TV RC5", decode_type_t::RC5, 0x100C, 13, 36},
    {"Philips", "TV RC6", decode_type_t::RC6, 0x100C, 20, 36},

    // Toshiba
    {"Toshiba", "TV", decode_type_t::NEC, 0x02FD48B7, 32, 38},

    // Sharp
    {"Sharp", "TV", decode_type_t::SHARP, 0x41A2, 15, 38},

    // Vizio
    {"Vizio", "TV", decode_type_t::NEC, 0x20DF10EF, 32, 38},

    // TCL/RCA/Hisense (often use NEC)
    {"TCL", "TV", decode_type_t::NEC, 0x807F02FD, 32, 38},
    {"Hisense", "TV", decode_type_t::NEC, 0x807F02FD, 32, 38},
};
const uint16_t NUM_TV_POWER_CODES = sizeof(TV_POWER_CODES) / sizeof(IRPowerCode);

extern IRInterface ir_obj;

#endif // HAS_IR
#endif // IRInterface_h

/*
 * LoRaInterface.h
 * LoRa Radio Security Testing Module
 *
 * Supports LoRa packet capture, spectrum analysis, and protocol decoding
 * Compatible with SX1262/SX1276/SX1278 radio modules
 *
 * Features:
 * - Packet sniffing and capture to PCAP
 * - Meshtastic protocol decoding
 * - LoRaWAN packet analysis
 * - Spectrum analysis and channel scanning
 * - Pager-style messaging
 * - Replay attacks (for authorized testing)
 *
 * FOR AUTHORIZED SECURITY TESTING ONLY
 */

#pragma once

#ifndef LoRaInterface_h
#define LoRaInterface_h

#include "configs.h"
#include <Arduino.h>
#include <SPI.h>

#ifdef HAS_LORA
  #include <LoRa.h>  // Arduino-LoRa library
#endif

// ============================================================================
// LORA SCAN MODE CONSTANTS (140-159 range)
// ============================================================================

#define LORA_SCAN_SNIFF         140   // Passive packet sniffing
#define LORA_SCAN_SPECTRUM      141   // Spectrum/channel analysis
#define LORA_SCAN_MESHTASTIC    142   // Meshtastic protocol decode
#define LORA_SCAN_LORAWAN       143   // LoRaWAN packet analysis
#define LORA_ATTACK_REPLAY      144   // Replay captured packets
#define LORA_ATTACK_JAM         145   // Jamming (USE WITH CAUTION)
#define LORA_MODE_PAGER         146   // Pager messaging mode
#define LORA_MODE_BEACON        147   // Beacon transmit mode

// ============================================================================
// FREQUENCY BANDS
// ============================================================================

// Common LoRa frequency bands (in Hz)
#define LORA_FREQ_433           433E6     // 433 MHz (EU/Asia)
#define LORA_FREQ_868           868E6     // 868 MHz (EU)
#define LORA_FREQ_915           915E6     // 915 MHz (US/AU)
#define LORA_FREQ_923           923E6     // 923 MHz (Asia)

// Meshtastic default frequencies
#define MESHTASTIC_US           906.875E6
#define MESHTASTIC_EU           869.525E6
#define MESHTASTIC_CN           470.125E6
#define MESHTASTIC_JP           920.125E6
#define MESHTASTIC_ANZ          916.875E6

// LoRaWAN frequencies (US915)
#define LORAWAN_US_CH0          902.3E6
#define LORAWAN_US_CH1          902.5E6
#define LORAWAN_US_CH2          902.7E6
#define LORAWAN_US_CH3          902.9E6
#define LORAWAN_US_CH4          903.1E6
#define LORAWAN_US_CH5          903.3E6
#define LORAWAN_US_CH6          903.5E6
#define LORAWAN_US_CH7          903.7E6

// ============================================================================
// SPREADING FACTORS AND BANDWIDTH
// ============================================================================

#define LORA_SF6                6
#define LORA_SF7                7
#define LORA_SF8                8
#define LORA_SF9                9
#define LORA_SF10               10
#define LORA_SF11               11
#define LORA_SF12               12

#define LORA_BW_7K8             7.8E3
#define LORA_BW_10K4            10.4E3
#define LORA_BW_15K6            15.6E3
#define LORA_BW_20K8            20.8E3
#define LORA_BW_31K25           31.25E3
#define LORA_BW_41K7            41.7E3
#define LORA_BW_62K5            62.5E3
#define LORA_BW_125K            125E3
#define LORA_BW_250K            250E3
#define LORA_BW_500K            500E3

// Meshtastic default settings
#define MESHTASTIC_SF           11
#define MESHTASTIC_BW           LORA_BW_250K
#define MESHTASTIC_CR           5    // Coding rate 4/5

// ============================================================================
// PROTOCOL TYPES
// ============================================================================

typedef enum {
    PROTO_UNKNOWN = 0,
    PROTO_MESHTASTIC,
    PROTO_LORAWAN,
    PROTO_RAW,
    PROTO_RADIOLIB,
    PROTO_CUSTOM
} LoRaProtocol;

// ============================================================================
// MESHTASTIC PACKET STRUCTURES
// ============================================================================

// Meshtastic packet header (simplified)
typedef struct {
    uint32_t dest;          // Destination node ID
    uint32_t sender;        // Sender node ID
    uint32_t packetId;      // Unique packet ID
    uint8_t flags;          // Packet flags
    uint8_t channelHash;    // Channel identifier
    uint8_t hopLimit;       // Remaining hops
    uint8_t hopStart;       // Initial hop count
} MeshtasticHeader;

// Meshtastic port numbers (data types)
typedef enum {
    MESH_PORT_UNKNOWN = 0,
    MESH_PORT_TEXT = 1,
    MESH_PORT_REMOTE_HW = 2,
    MESH_PORT_POSITION = 3,
    MESH_PORT_NODEINFO = 4,
    MESH_PORT_ROUTING = 5,
    MESH_PORT_ADMIN = 6,
    MESH_PORT_TELEMETRY = 67,
    MESH_PORT_RANGE_TEST = 73,
    MESH_PORT_STORE_FORWARD = 74,
    MESH_PORT_DETECTION_SENSOR = 75,
    MESH_PORT_PAXCOUNT = 76,
    MESH_PORT_TRACEROUTE = 77
} MeshtasticPort;

// ============================================================================
// LORAWAN STRUCTURES
// ============================================================================

// LoRaWAN message types
typedef enum {
    LORAWAN_JOIN_REQUEST = 0,
    LORAWAN_JOIN_ACCEPT = 1,
    LORAWAN_UNCONF_DATA_UP = 2,
    LORAWAN_UNCONF_DATA_DOWN = 3,
    LORAWAN_CONF_DATA_UP = 4,
    LORAWAN_CONF_DATA_DOWN = 5,
    LORAWAN_REJOIN_REQUEST = 6,
    LORAWAN_PROPRIETARY = 7
} LoRaWANMsgType;

// LoRaWAN MHDR (MAC header)
typedef struct {
    uint8_t mType : 3;      // Message type
    uint8_t rfu : 3;        // Reserved
    uint8_t major : 2;      // Major version
} LoRaWANMHDR;

// LoRaWAN frame header
typedef struct {
    uint32_t devAddr;       // Device address
    uint8_t fCtrl;          // Frame control
    uint16_t fCnt;          // Frame counter
    uint8_t* fOpts;         // Frame options (variable)
    uint8_t fOptsLen;
} LoRaWANFHDR;

// ============================================================================
// CAPTURED PACKET STRUCTURE
// ============================================================================

#define MAX_LORA_PACKET_SIZE    256
#define MAX_CAPTURED_PACKETS    100

typedef struct {
    uint32_t timestamp;             // Capture time (millis)
    long frequency;                 // Frequency in Hz
    int rssi;                       // Signal strength
    float snr;                      // Signal-to-noise ratio
    uint8_t spreadingFactor;
    long bandwidth;
    uint8_t codingRate;
    LoRaProtocol protocol;          // Detected protocol
    uint8_t data[MAX_LORA_PACKET_SIZE];
    uint8_t length;
    bool decoded;                   // Successfully decoded?
    char decodedSummary[64];        // Human-readable summary
} CapturedLoRaPacket;

// ============================================================================
// SPECTRUM ANALYSIS
// ============================================================================

#define SPECTRUM_CHANNELS       64
#define SPECTRUM_SAMPLES        10

typedef struct {
    long startFreq;
    long endFreq;
    long stepSize;
    int rssiValues[SPECTRUM_CHANNELS];
    uint8_t activeChannels;
    long strongestFreq;
    int strongestRSSI;
} SpectrumAnalysis;

// ============================================================================
// PAGER MESSAGE
// ============================================================================

#define MAX_PAGER_MSG_LEN       200

typedef struct {
    uint32_t id;
    uint32_t sender;
    uint32_t dest;
    char message[MAX_PAGER_MSG_LEN];
    uint32_t timestamp;
    int rssi;
    bool read;
    bool outgoing;
} PagerMessage;

// ============================================================================
// LORA INTERFACE CLASS
// ============================================================================

class LoRaInterface {
public:
    // Lifecycle
    void RunSetup();
    void shutDown();
    bool isInitialized();

    // Configuration
    void setFrequency(long freq);
    void setSpreadingFactor(uint8_t sf);
    void setBandwidth(long bw);
    void setCodingRate(uint8_t cr);
    void setTxPower(int power);
    void setSyncWord(uint8_t sw);

    // Presets
    void applyMeshtasticSettings();
    void applyLoRaWANSettings(uint8_t channel);
    void applyCustomSettings(long freq, uint8_t sf, long bw);

    // ========== PACKET CAPTURE ==========
    void startSniffing();
    void stopSniffing();
    bool isSniffing();
    void processReceivedPacket();

    CapturedLoRaPacket* getLastPacket();
    LinkedList<CapturedLoRaPacket>* getCapturedPackets();
    uint16_t getCapturedCount();
    void clearCapturedPackets();

    // Save to SD card
    bool saveToPCAP(const char* filename);
    bool saveToJSON(const char* filename);

    // ========== PROTOCOL DECODING ==========
    LoRaProtocol detectProtocol(uint8_t* data, uint8_t len);
    bool decodeMeshtastic(uint8_t* data, uint8_t len, MeshtasticHeader* header);
    bool decodeLoRaWAN(uint8_t* data, uint8_t len, LoRaWANMHDR* mhdr, LoRaWANFHDR* fhdr);
    String getProtocolName(LoRaProtocol proto);
    String getMeshtasticPortName(uint8_t port);
    String getLoRaWANMsgTypeName(uint8_t mType);

    // ========== SPECTRUM ANALYSIS ==========
    void startSpectrumScan(long startFreq, long endFreq, long step);
    void stopSpectrumScan();
    bool isScanning();
    void updateSpectrumScan();
    SpectrumAnalysis* getSpectrumResults();

    // Find active channels
    void findMeshtasticChannels();
    void findLoRaWANChannels();

    // ========== REPLAY (AUTHORIZED TESTING) ==========
    bool replayPacket(CapturedLoRaPacket* packet);
    bool replayPacketAtFreq(CapturedLoRaPacket* packet, long freq);

    // ========== PAGER MODE ==========
    void enablePagerMode();
    void disablePagerMode();
    bool sendPagerMessage(const char* message, uint32_t dest);
    LinkedList<PagerMessage>* getMessages();
    uint8_t getUnreadCount();
    void markAsRead(uint32_t msgId);

    // ========== BEACON MODE ==========
    void startBeacon(const char* message, uint32_t intervalMs);
    void stopBeacon();
    bool isBeaconing();

    // ========== STATISTICS ==========
    uint32_t getTotalPackets();
    uint32_t getMeshtasticPackets();
    uint32_t getLoRaWANPackets();
    uint32_t getUnknownPackets();
    void resetStatistics();

    // Current state
    uint8_t currentMode;
    long currentFrequency;
    uint8_t currentSF;
    long currentBW;

private:
    bool initialized;
    bool sniffing;
    bool scanning;
    bool pagerMode;
    bool beaconing;

    // Packet storage
    LinkedList<CapturedLoRaPacket>* capturedPackets;
    CapturedLoRaPacket lastPacket;

    // Pager messages
    LinkedList<PagerMessage>* pagerMessages;
    uint32_t myNodeId;

    // Spectrum
    SpectrumAnalysis spectrumData;

    // Beacon
    String beaconMessage;
    uint32_t beaconInterval;
    uint32_t lastBeaconTime;

    // Statistics
    uint32_t statTotalPackets;
    uint32_t statMeshtastic;
    uint32_t statLoRaWAN;
    uint32_t statUnknown;

    // Helpers
    void onReceive(int packetSize);
    uint32_t generateNodeId();
    void createPCAPHeader(File* file);
    void writePCAPPacket(File* file, CapturedLoRaPacket* pkt);
};

extern LoRaInterface lora_obj;

// ============================================================================
// PCAP FILE FORMAT CONSTANTS
// ============================================================================

#define PCAP_MAGIC              0xA1B2C3D4
#define PCAP_VERSION_MAJOR      2
#define PCAP_VERSION_MINOR      4
#define PCAP_LINKTYPE_USER0     147   // User-defined for LoRa

// PCAP global header
typedef struct {
    uint32_t magic;
    uint16_t versionMajor;
    uint16_t versionMinor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
} PCAPGlobalHeader;

// PCAP packet header
typedef struct {
    uint32_t tsSec;
    uint32_t tsUsec;
    uint32_t inclLen;
    uint32_t origLen;
} PCAPPacketHeader;

#endif // LoRaInterface_h

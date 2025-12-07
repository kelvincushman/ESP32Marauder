/*
 * LoRaInterface.cpp
 * LoRa Radio Security Testing Module Implementation
 *
 * Supports packet capture, Meshtastic/LoRaWAN decoding, spectrum analysis
 *
 * FOR AUTHORIZED SECURITY TESTING ONLY
 */

#include "LoRaInterface.h"

#ifdef HAS_LORA

#include "SD.h"
#include <LinkedList.h>

// Global instance
LoRaInterface lora_obj;

// ============================================================================
// LIFECYCLE
// ============================================================================

void LoRaInterface::RunSetup() {
    Serial.println(F(""));
    Serial.println(F("[LoRa] Initializing LoRa interface..."));

    initialized = false;
    sniffing = false;
    scanning = false;
    pagerMode = false;
    beaconing = false;
    currentMode = 0;

    // Initialize packet storage
    capturedPackets = new LinkedList<CapturedLoRaPacket>();
    pagerMessages = new LinkedList<PagerMessage>();

    // Reset statistics
    resetStatistics();

    // Generate unique node ID for pager mode
    myNodeId = generateNodeId();

    // Initialize LoRa radio
    #if defined(LORA_CS_PIN) && defined(LORA_RST_PIN) && defined(LORA_IRQ_PIN)
        LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_IRQ_PIN);
    #endif

    // Try default frequency (915 MHz US)
    if (!LoRa.begin(LORA_FREQ_915)) {
        Serial.println(F("[LoRa] ERROR: Radio initialization failed!"));
        Serial.println(F("[LoRa] Check wiring and antenna connection"));
        return;
    }

    // Apply default settings (Meshtastic compatible)
    applyMeshtasticSettings();

    initialized = true;

    Serial.println(F("[LoRa] Radio initialized successfully"));
    Serial.print(F("[LoRa] Node ID: 0x"));
    Serial.println(myNodeId, HEX);
    Serial.println(F(""));
}

void LoRaInterface::shutDown() {
    stopSniffing();
    stopSpectrumScan();
    stopBeacon();
    disablePagerMode();

    LoRa.end();
    initialized = false;

    Serial.println(F("[LoRa] Radio shut down"));
}

bool LoRaInterface::isInitialized() {
    return initialized;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void LoRaInterface::setFrequency(long freq) {
    if (!initialized) return;

    LoRa.setFrequency(freq);
    currentFrequency = freq;

    Serial.print(F("[LoRa] Frequency: "));
    Serial.print(freq / 1E6, 3);
    Serial.println(F(" MHz"));
}

void LoRaInterface::setSpreadingFactor(uint8_t sf) {
    if (!initialized) return;

    if (sf < 6) sf = 6;
    if (sf > 12) sf = 12;

    LoRa.setSpreadingFactor(sf);
    currentSF = sf;

    Serial.print(F("[LoRa] Spreading Factor: SF"));
    Serial.println(sf);
}

void LoRaInterface::setBandwidth(long bw) {
    if (!initialized) return;

    LoRa.setSignalBandwidth(bw);
    currentBW = bw;

    Serial.print(F("[LoRa] Bandwidth: "));
    Serial.print(bw / 1E3, 1);
    Serial.println(F(" kHz"));
}

void LoRaInterface::setCodingRate(uint8_t cr) {
    if (!initialized) return;

    if (cr < 5) cr = 5;
    if (cr > 8) cr = 8;

    LoRa.setCodingRate4(cr);

    Serial.print(F("[LoRa] Coding Rate: 4/"));
    Serial.println(cr);
}

void LoRaInterface::setTxPower(int power) {
    if (!initialized) return;

    if (power < 2) power = 2;
    if (power > 20) power = 20;

    LoRa.setTxPower(power);

    Serial.print(F("[LoRa] TX Power: "));
    Serial.print(power);
    Serial.println(F(" dBm"));
}

void LoRaInterface::setSyncWord(uint8_t sw) {
    if (!initialized) return;

    LoRa.setSyncWord(sw);

    Serial.print(F("[LoRa] Sync Word: 0x"));
    Serial.println(sw, HEX);
}

// ============================================================================
// PRESETS
// ============================================================================

void LoRaInterface::applyMeshtasticSettings() {
    Serial.println(F("[LoRa] Applying Meshtastic settings..."));

    setFrequency(MESHTASTIC_US);
    setSpreadingFactor(MESHTASTIC_SF);
    setBandwidth(MESHTASTIC_BW);
    setCodingRate(MESHTASTIC_CR);
    setSyncWord(0x2B);  // Meshtastic sync word

    Serial.println(F("[LoRa] Meshtastic mode active"));
}

void LoRaInterface::applyLoRaWANSettings(uint8_t channel) {
    Serial.println(F("[LoRa] Applying LoRaWAN settings..."));

    // US915 uplink channels
    long freqs[] = {
        LORAWAN_US_CH0, LORAWAN_US_CH1, LORAWAN_US_CH2, LORAWAN_US_CH3,
        LORAWAN_US_CH4, LORAWAN_US_CH5, LORAWAN_US_CH6, LORAWAN_US_CH7
    };

    if (channel > 7) channel = 0;

    setFrequency(freqs[channel]);
    setSpreadingFactor(LORA_SF10);  // LoRaWAN default
    setBandwidth(LORA_BW_125K);
    setCodingRate(5);
    setSyncWord(0x34);  // LoRaWAN public sync word

    Serial.print(F("[LoRa] LoRaWAN channel "));
    Serial.print(channel);
    Serial.println(F(" active"));
}

void LoRaInterface::applyCustomSettings(long freq, uint8_t sf, long bw) {
    setFrequency(freq);
    setSpreadingFactor(sf);
    setBandwidth(bw);
}

// ============================================================================
// PACKET CAPTURE
// ============================================================================

void LoRaInterface::startSniffing() {
    if (!initialized || sniffing) return;

    Serial.println(F("[LoRa] Starting packet sniffing..."));
    Serial.print(F("[LoRa] Listening on "));
    Serial.print(currentFrequency / 1E6, 3);
    Serial.println(F(" MHz"));

    // Put radio in receive mode
    LoRa.receive();
    sniffing = true;
    currentMode = LORA_SCAN_SNIFF;

    Serial.println(F("[LoRa] Sniffing active - packets will be captured"));
}

void LoRaInterface::stopSniffing() {
    if (!sniffing) return;

    LoRa.idle();
    sniffing = false;
    currentMode = 0;

    Serial.println(F("[LoRa] Sniffing stopped"));
    Serial.print(F("[LoRa] Captured "));
    Serial.print(capturedPackets->size());
    Serial.println(F(" packets"));
}

bool LoRaInterface::isSniffing() {
    return sniffing;
}

void LoRaInterface::processReceivedPacket() {
    if (!sniffing && !pagerMode) return;

    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    // Create new packet entry
    CapturedLoRaPacket pkt;
    memset(&pkt, 0, sizeof(CapturedLoRaPacket));

    pkt.timestamp = millis();
    pkt.frequency = currentFrequency;
    pkt.rssi = LoRa.packetRssi();
    pkt.snr = LoRa.packetSnr();
    pkt.spreadingFactor = currentSF;
    pkt.bandwidth = currentBW;
    pkt.length = 0;

    // Read packet data
    while (LoRa.available() && pkt.length < MAX_LORA_PACKET_SIZE) {
        pkt.data[pkt.length++] = LoRa.read();
    }

    // Detect and decode protocol
    pkt.protocol = detectProtocol(pkt.data, pkt.length);
    pkt.decoded = false;

    // Try to decode based on protocol
    switch (pkt.protocol) {
        case PROTO_MESHTASTIC: {
            MeshtasticHeader header;
            if (decodeMeshtastic(pkt.data, pkt.length, &header)) {
                pkt.decoded = true;
                snprintf(pkt.decodedSummary, 64, "MESH: %08X->%08X id:%08X",
                         header.sender, header.dest, header.packetId);
                statMeshtastic++;

                // If in pager mode, process as message
                if (pagerMode && pkt.length > sizeof(MeshtasticHeader)) {
                    // Would extract text message here
                }
            }
            break;
        }

        case PROTO_LORAWAN: {
            LoRaWANMHDR mhdr;
            LoRaWANFHDR fhdr;
            if (decodeLoRaWAN(pkt.data, pkt.length, &mhdr, &fhdr)) {
                pkt.decoded = true;
                snprintf(pkt.decodedSummary, 64, "LoRaWAN: %s devAddr:%08X",
                         getLoRaWANMsgTypeName(mhdr.mType).c_str(), fhdr.devAddr);
                statLoRaWAN++;
            }
            break;
        }

        default:
            snprintf(pkt.decodedSummary, 64, "RAW: %d bytes", pkt.length);
            statUnknown++;
            break;
    }

    // Store packet
    if (capturedPackets->size() >= MAX_CAPTURED_PACKETS) {
        capturedPackets->remove(0);  // Remove oldest
    }
    capturedPackets->add(pkt);
    memcpy(&lastPacket, &pkt, sizeof(CapturedLoRaPacket));

    statTotalPackets++;

    // Log to serial
    Serial.print(F("[LoRa] PKT: "));
    Serial.print(pkt.decodedSummary);
    Serial.print(F(" RSSI:"));
    Serial.print(pkt.rssi);
    Serial.print(F(" SNR:"));
    Serial.println(pkt.snr, 1);
}

CapturedLoRaPacket* LoRaInterface::getLastPacket() {
    return &lastPacket;
}

LinkedList<CapturedLoRaPacket>* LoRaInterface::getCapturedPackets() {
    return capturedPackets;
}

uint16_t LoRaInterface::getCapturedCount() {
    return capturedPackets->size();
}

void LoRaInterface::clearCapturedPackets() {
    capturedPackets->clear();
    Serial.println(F("[LoRa] Captured packets cleared"));
}

// ============================================================================
// PROTOCOL DETECTION AND DECODING
// ============================================================================

LoRaProtocol LoRaInterface::detectProtocol(uint8_t* data, uint8_t len) {
    if (len < 4) return PROTO_RAW;

    // Meshtastic detection
    // Meshtastic packets have specific header structure
    if (len >= 16) {
        // Check for valid Meshtastic header patterns
        // (This is simplified - real detection is more complex)
        uint8_t flags = data[12];
        if ((flags & 0x03) <= 3) {  // Valid hop limit
            return PROTO_MESHTASTIC;
        }
    }

    // LoRaWAN detection
    // MHDR byte: bits 5-7 are MType (0-7), bits 0-1 are Major (0)
    uint8_t mhdr = data[0];
    uint8_t mType = (mhdr >> 5) & 0x07;
    uint8_t major = mhdr & 0x03;

    if (major == 0 && mType <= 7) {
        // Likely LoRaWAN
        if (mType == LORAWAN_JOIN_REQUEST && len == 23) {
            return PROTO_LORAWAN;
        }
        if ((mType == LORAWAN_UNCONF_DATA_UP || mType == LORAWAN_CONF_DATA_UP) && len >= 12) {
            return PROTO_LORAWAN;
        }
    }

    return PROTO_UNKNOWN;
}

bool LoRaInterface::decodeMeshtastic(uint8_t* data, uint8_t len, MeshtasticHeader* header) {
    if (len < 16) return false;

    // Meshtastic header format (simplified)
    // Note: Real Meshtastic uses protobuf encoding
    header->dest = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    header->sender = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    header->packetId = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    header->flags = data[12];
    header->channelHash = data[13];
    header->hopLimit = data[14] & 0x07;
    header->hopStart = (data[14] >> 3) & 0x07;

    return true;
}

bool LoRaInterface::decodeLoRaWAN(uint8_t* data, uint8_t len, LoRaWANMHDR* mhdr, LoRaWANFHDR* fhdr) {
    if (len < 12) return false;

    // MHDR (1 byte)
    mhdr->mType = (data[0] >> 5) & 0x07;
    mhdr->rfu = (data[0] >> 2) & 0x07;
    mhdr->major = data[0] & 0x03;

    // FHDR starts at byte 1
    fhdr->devAddr = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
    fhdr->fCtrl = data[5];
    fhdr->fCnt = data[6] | (data[7] << 8);
    fhdr->fOptsLen = fhdr->fCtrl & 0x0F;

    return true;
}

String LoRaInterface::getProtocolName(LoRaProtocol proto) {
    switch (proto) {
        case PROTO_MESHTASTIC: return "Meshtastic";
        case PROTO_LORAWAN: return "LoRaWAN";
        case PROTO_RAW: return "Raw";
        case PROTO_RADIOLIB: return "RadioLib";
        case PROTO_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

String LoRaInterface::getMeshtasticPortName(uint8_t port) {
    switch (port) {
        case MESH_PORT_TEXT: return "Text";
        case MESH_PORT_POSITION: return "Position";
        case MESH_PORT_NODEINFO: return "NodeInfo";
        case MESH_PORT_ROUTING: return "Routing";
        case MESH_PORT_ADMIN: return "Admin";
        case MESH_PORT_TELEMETRY: return "Telemetry";
        case MESH_PORT_TRACEROUTE: return "Traceroute";
        default: return "Unknown";
    }
}

String LoRaInterface::getLoRaWANMsgTypeName(uint8_t mType) {
    switch (mType) {
        case LORAWAN_JOIN_REQUEST: return "JoinReq";
        case LORAWAN_JOIN_ACCEPT: return "JoinAcc";
        case LORAWAN_UNCONF_DATA_UP: return "UnconfUp";
        case LORAWAN_UNCONF_DATA_DOWN: return "UnconfDn";
        case LORAWAN_CONF_DATA_UP: return "ConfUp";
        case LORAWAN_CONF_DATA_DOWN: return "ConfDn";
        case LORAWAN_REJOIN_REQUEST: return "Rejoin";
        default: return "Unknown";
    }
}

// ============================================================================
// SPECTRUM ANALYSIS
// ============================================================================

void LoRaInterface::startSpectrumScan(long startFreq, long endFreq, long step) {
    if (!initialized || scanning) return;

    Serial.println(F("[LoRa] Starting spectrum scan..."));
    Serial.print(F("[LoRa] Range: "));
    Serial.print(startFreq / 1E6, 3);
    Serial.print(F(" - "));
    Serial.print(endFreq / 1E6, 3);
    Serial.println(F(" MHz"));

    spectrumData.startFreq = startFreq;
    spectrumData.endFreq = endFreq;
    spectrumData.stepSize = step;
    spectrumData.activeChannels = 0;
    spectrumData.strongestRSSI = -200;

    // Clear previous data
    memset(spectrumData.rssiValues, -200, sizeof(spectrumData.rssiValues));

    scanning = true;
    currentMode = LORA_SCAN_SPECTRUM;

    // Initial scan
    updateSpectrumScan();
}

void LoRaInterface::stopSpectrumScan() {
    if (!scanning) return;

    scanning = false;
    currentMode = 0;

    // Restore previous frequency
    LoRa.setFrequency(currentFrequency);

    Serial.println(F("[LoRa] Spectrum scan complete"));
    Serial.print(F("[LoRa] Active channels: "));
    Serial.println(spectrumData.activeChannels);
    Serial.print(F("[LoRa] Strongest: "));
    Serial.print(spectrumData.strongestFreq / 1E6, 3);
    Serial.print(F(" MHz @ "));
    Serial.print(spectrumData.strongestRSSI);
    Serial.println(F(" dBm"));
}

bool LoRaInterface::isScanning() {
    return scanning;
}

void LoRaInterface::updateSpectrumScan() {
    if (!scanning) return;

    int channel = 0;
    long freq = spectrumData.startFreq;

    while (freq <= spectrumData.endFreq && channel < SPECTRUM_CHANNELS) {
        LoRa.setFrequency(freq);
        delay(5);  // Settle time

        // Sample RSSI
        int rssiSum = 0;
        for (int i = 0; i < SPECTRUM_SAMPLES; i++) {
            // Read RSSI (requires radio to be in receive mode briefly)
            LoRa.receive();
            delay(1);
            int rssi = LoRa.packetRssi();  // Ambient RSSI
            if (rssi > -200) rssiSum += rssi;
            LoRa.idle();
        }

        int avgRSSI = rssiSum / SPECTRUM_SAMPLES;
        spectrumData.rssiValues[channel] = avgRSSI;

        // Track strongest signal
        if (avgRSSI > spectrumData.strongestRSSI) {
            spectrumData.strongestRSSI = avgRSSI;
            spectrumData.strongestFreq = freq;
        }

        // Count active channels (above noise floor)
        if (avgRSSI > -110) {
            spectrumData.activeChannels++;
        }

        freq += spectrumData.stepSize;
        channel++;
    }
}

SpectrumAnalysis* LoRaInterface::getSpectrumResults() {
    return &spectrumData;
}

void LoRaInterface::findMeshtasticChannels() {
    Serial.println(F("[LoRa] Scanning for Meshtastic channels..."));

    // Scan common Meshtastic frequencies
    long meshFreqs[] = {
        906.875E6,  // US Primary
        908.875E6,  // US Alt
        869.525E6,  // EU
        920.125E6,  // JP
        916.875E6   // ANZ
    };

    for (int i = 0; i < 5; i++) {
        LoRa.setFrequency(meshFreqs[i]);
        applyMeshtasticSettings();
        LoRa.receive();
        delay(100);

        int rssi = LoRa.packetRssi();
        Serial.print(F("[LoRa] "));
        Serial.print(meshFreqs[i] / 1E6, 3);
        Serial.print(F(" MHz: "));
        Serial.print(rssi);
        Serial.println(F(" dBm"));
    }

    // Restore settings
    setFrequency(currentFrequency);
}

void LoRaInterface::findLoRaWANChannels() {
    Serial.println(F("[LoRa] Scanning for LoRaWAN channels..."));

    for (int ch = 0; ch < 8; ch++) {
        applyLoRaWANSettings(ch);
        LoRa.receive();
        delay(100);

        int rssi = LoRa.packetRssi();
        Serial.print(F("[LoRa] US915 CH"));
        Serial.print(ch);
        Serial.print(F(": "));
        Serial.print(rssi);
        Serial.println(F(" dBm"));
    }

    // Restore settings
    setFrequency(currentFrequency);
}

// ============================================================================
// REPLAY (AUTHORIZED TESTING ONLY)
// ============================================================================

bool LoRaInterface::replayPacket(CapturedLoRaPacket* packet) {
    return replayPacketAtFreq(packet, packet->frequency);
}

bool LoRaInterface::replayPacketAtFreq(CapturedLoRaPacket* packet, long freq) {
    if (!initialized) return false;

    Serial.println(F("[LoRa] WARNING: Replay attack - authorized testing only!"));
    Serial.print(F("[LoRa] Replaying "));
    Serial.print(packet->length);
    Serial.print(F(" bytes on "));
    Serial.print(freq / 1E6, 3);
    Serial.println(F(" MHz"));

    // Set frequency and settings
    LoRa.setFrequency(freq);
    LoRa.setSpreadingFactor(packet->spreadingFactor);
    LoRa.setSignalBandwidth(packet->bandwidth);

    // Transmit packet
    LoRa.beginPacket();
    LoRa.write(packet->data, packet->length);
    LoRa.endPacket();

    // Restore receive mode if sniffing
    if (sniffing) {
        LoRa.setFrequency(currentFrequency);
        LoRa.receive();
    }

    Serial.println(F("[LoRa] Packet replayed"));
    return true;
}

// ============================================================================
// PAGER MODE
// ============================================================================

void LoRaInterface::enablePagerMode() {
    if (!initialized) return;

    Serial.println(F("[LoRa] Enabling pager mode..."));
    applyMeshtasticSettings();

    pagerMode = true;
    currentMode = LORA_MODE_PAGER;

    LoRa.receive();

    Serial.print(F("[LoRa] Pager active - Node ID: 0x"));
    Serial.println(myNodeId, HEX);
}

void LoRaInterface::disablePagerMode() {
    pagerMode = false;
    currentMode = 0;
    LoRa.idle();

    Serial.println(F("[LoRa] Pager mode disabled"));
}

bool LoRaInterface::sendPagerMessage(const char* message, uint32_t dest) {
    if (!initialized) return false;

    Serial.print(F("[LoRa] Sending message to 0x"));
    Serial.println(dest, HEX);

    // Build simple packet
    // In real implementation, would use Meshtastic protobuf format
    uint8_t packet[MAX_LORA_PACKET_SIZE];
    int len = 0;

    // Header
    packet[len++] = (dest >> 24) & 0xFF;
    packet[len++] = (dest >> 16) & 0xFF;
    packet[len++] = (dest >> 8) & 0xFF;
    packet[len++] = dest & 0xFF;

    packet[len++] = (myNodeId >> 24) & 0xFF;
    packet[len++] = (myNodeId >> 16) & 0xFF;
    packet[len++] = (myNodeId >> 8) & 0xFF;
    packet[len++] = myNodeId & 0xFF;

    // Packet ID (random)
    uint32_t pktId = random(0xFFFFFFFF);
    packet[len++] = (pktId >> 24) & 0xFF;
    packet[len++] = (pktId >> 16) & 0xFF;
    packet[len++] = (pktId >> 8) & 0xFF;
    packet[len++] = pktId & 0xFF;

    // Flags
    packet[len++] = 0x00;
    packet[len++] = 0x00;
    packet[len++] = 0x03;  // Hop limit
    packet[len++] = MESH_PORT_TEXT;

    // Message payload
    int msgLen = strlen(message);
    if (msgLen > MAX_PAGER_MSG_LEN) msgLen = MAX_PAGER_MSG_LEN;

    memcpy(&packet[len], message, msgLen);
    len += msgLen;

    // Send
    LoRa.beginPacket();
    LoRa.write(packet, len);
    LoRa.endPacket();

    // Store outgoing message
    PagerMessage msg;
    msg.id = pktId;
    msg.sender = myNodeId;
    msg.dest = dest;
    strncpy(msg.message, message, MAX_PAGER_MSG_LEN - 1);
    msg.timestamp = millis();
    msg.rssi = 0;
    msg.read = true;
    msg.outgoing = true;

    pagerMessages->add(msg);

    // Return to receive mode
    if (pagerMode) {
        LoRa.receive();
    }

    Serial.println(F("[LoRa] Message sent"));
    return true;
}

LinkedList<PagerMessage>* LoRaInterface::getMessages() {
    return pagerMessages;
}

uint8_t LoRaInterface::getUnreadCount() {
    uint8_t count = 0;
    for (int i = 0; i < pagerMessages->size(); i++) {
        if (!pagerMessages->get(i).read && !pagerMessages->get(i).outgoing) {
            count++;
        }
    }
    return count;
}

void LoRaInterface::markAsRead(uint32_t msgId) {
    for (int i = 0; i < pagerMessages->size(); i++) {
        if (pagerMessages->get(i).id == msgId) {
            PagerMessage msg = pagerMessages->get(i);
            msg.read = true;
            pagerMessages->set(i, msg);
            break;
        }
    }
}

// ============================================================================
// BEACON MODE
// ============================================================================

void LoRaInterface::startBeacon(const char* message, uint32_t intervalMs) {
    if (!initialized) return;

    beaconMessage = message;
    beaconInterval = intervalMs;
    beaconing = true;
    lastBeaconTime = 0;
    currentMode = LORA_MODE_BEACON;

    Serial.print(F("[LoRa] Beacon started: \""));
    Serial.print(message);
    Serial.print(F("\" every "));
    Serial.print(intervalMs / 1000);
    Serial.println(F(" seconds"));
}

void LoRaInterface::stopBeacon() {
    beaconing = false;
    currentMode = 0;

    Serial.println(F("[LoRa] Beacon stopped"));
}

bool LoRaInterface::isBeaconing() {
    return beaconing;
}

// ============================================================================
// FILE EXPORT
// ============================================================================

bool LoRaInterface::saveToPCAP(const char* filename) {
    if (capturedPackets->size() == 0) {
        Serial.println(F("[LoRa] No packets to save"));
        return false;
    }

    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println(F("[LoRa] Failed to create PCAP file"));
        return false;
    }

    // Write PCAP global header
    createPCAPHeader(&file);

    // Write packets
    for (int i = 0; i < capturedPackets->size(); i++) {
        CapturedLoRaPacket pkt = capturedPackets->get(i);
        writePCAPPacket(&file, &pkt);
    }

    file.close();

    Serial.print(F("[LoRa] Saved "));
    Serial.print(capturedPackets->size());
    Serial.print(F(" packets to "));
    Serial.println(filename);

    return true;
}

void LoRaInterface::createPCAPHeader(File* file) {
    PCAPGlobalHeader header;
    header.magic = PCAP_MAGIC;
    header.versionMajor = PCAP_VERSION_MAJOR;
    header.versionMinor = PCAP_VERSION_MINOR;
    header.thiszone = 0;
    header.sigfigs = 0;
    header.snaplen = MAX_LORA_PACKET_SIZE;
    header.network = PCAP_LINKTYPE_USER0;

    file->write((uint8_t*)&header, sizeof(PCAPGlobalHeader));
}

void LoRaInterface::writePCAPPacket(File* file, CapturedLoRaPacket* pkt) {
    PCAPPacketHeader header;
    header.tsSec = pkt->timestamp / 1000;
    header.tsUsec = (pkt->timestamp % 1000) * 1000;
    header.inclLen = pkt->length;
    header.origLen = pkt->length;

    file->write((uint8_t*)&header, sizeof(PCAPPacketHeader));
    file->write(pkt->data, pkt->length);
}

bool LoRaInterface::saveToJSON(const char* filename) {
    if (capturedPackets->size() == 0) return false;

    File file = SD.open(filename, FILE_WRITE);
    if (!file) return false;

    file.println("[");

    for (int i = 0; i < capturedPackets->size(); i++) {
        CapturedLoRaPacket pkt = capturedPackets->get(i);

        file.print("  {");
        file.print("\"timestamp\":");
        file.print(pkt.timestamp);
        file.print(",\"frequency\":");
        file.print(pkt.frequency);
        file.print(",\"rssi\":");
        file.print(pkt.rssi);
        file.print(",\"snr\":");
        file.print(pkt.snr);
        file.print(",\"protocol\":\"");
        file.print(getProtocolName(pkt.protocol));
        file.print("\",\"length\":");
        file.print(pkt.length);
        file.print(",\"data\":\"");

        // Hex encode data
        for (int j = 0; j < pkt.length; j++) {
            if (pkt.data[j] < 0x10) file.print("0");
            file.print(pkt.data[j], HEX);
        }

        file.print("\",\"decoded\":\"");
        file.print(pkt.decodedSummary);
        file.print("\"}");

        if (i < capturedPackets->size() - 1) file.println(",");
        else file.println();
    }

    file.println("]");
    file.close();

    return true;
}

// ============================================================================
// STATISTICS
// ============================================================================

uint32_t LoRaInterface::getTotalPackets() { return statTotalPackets; }
uint32_t LoRaInterface::getMeshtasticPackets() { return statMeshtastic; }
uint32_t LoRaInterface::getLoRaWANPackets() { return statLoRaWAN; }
uint32_t LoRaInterface::getUnknownPackets() { return statUnknown; }

void LoRaInterface::resetStatistics() {
    statTotalPackets = 0;
    statMeshtastic = 0;
    statLoRaWAN = 0;
    statUnknown = 0;
}

// ============================================================================
// HELPERS
// ============================================================================

uint32_t LoRaInterface::generateNodeId() {
    // Generate from ESP32 MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}

#endif // HAS_LORA

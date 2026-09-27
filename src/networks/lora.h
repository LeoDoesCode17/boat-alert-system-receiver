#pragma once
#include <Arduino.h>

namespace lora {

    struct TelemetryPacket {
        float roll = 0.0f;
        float pitch = 0.0f;
        float yaw = 0.0f;
        double latitude = 0.0;
        double longitude = 0.0;
        bool gpsFixValid = false;
        bool valid = false;      // true if the last parse succeeded
        int rssi = 0;            // signal strength of last received packet
        unsigned long lastReceivedMillis = 0;
    };

    // Initializes SPI + LoRa radio. Returns false if radio init fails.
    bool initialize();

    // Call every loop(). Non-blocking: checks for a packet, reads & parses it
    // if present. Returns true if a NEW valid packet was received this call.
    bool update();

    // Access to the most recently parsed packet (valid until the next one arrives).
    const TelemetryPacket &getLatestPacket();

    // True if no new packet has arrived within timeoutMs (useful for a
    // "signal lost" indicator on the display / red LED).
    bool isSignalLost(unsigned long timeoutMs = 5000);

} // namespace lora
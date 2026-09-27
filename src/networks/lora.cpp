#include "lora.h"
#include "config/pin.h"
#include <SPI.h>
#include <LoRa.h>

namespace lora {

namespace {
    constexpr long LORA_FREQUENCY = 433E6;
    TelemetryPacket latest;
    String rxBuffer; // accumulates bytes of the current packet
}

bool initialize() {
    SPI.begin(pin::LORA_SCK, pin::LORA_MISO, pin::LORA_MOSI, pin::LORA_NSS);
    LoRa.setPins(pin::LORA_NSS, pin::LORA_RST, pin::LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println(F("[LORA] LoRa init failed. Check wiring."));
        return false;
    }

    // Optional but recommended: match these to the transmitter's settings.
    // LoRa.setSpreadingFactor(7);
    // LoRa.setSignalBandwidth(125E3);
    // LoRa.setCodingRate4(5);
    // LoRa.setSyncWord(0x12);

    Serial.println(F("[LORA] LoRa initialized at 433 MHz."));
    return true;
}

namespace {
    // Parses "R:2.13,P:-1.05,Y:184.30,LAT:-5.123456,LON:120.123456,FIX:1"
    bool parsePacket(const String &raw, TelemetryPacket &out) {
        // Split on commas into key:value tokens
        int fieldsParsed = 0;
        int start = 0;

        while (start < (int)raw.length()) {
            int comma = raw.indexOf(',', start);
            String token = (comma == -1) ? raw.substring(start) : raw.substring(start, comma);
            token.trim();

            int colon = token.indexOf(':');
            if (colon != -1) {
                String key = token.substring(0, colon);
                String value = token.substring(colon + 1);
                key.trim();
                value.trim();

                if (key == "R") {
                    out.roll = value.toFloat();
                    fieldsParsed++;
                } else if (key == "P") {
                    out.pitch = value.toFloat();
                    fieldsParsed++;
                } else if (key == "Y") {
                    out.yaw = value.toFloat();
                    fieldsParsed++;
                } else if (key == "LAT") {
                    out.latitude = value.toDouble();
                    fieldsParsed++;
                } else if (key == "LON") {
                    out.longitude = value.toDouble();
                    fieldsParsed++;
                } else if (key == "FIX") {
                    out.gpsFixValid = (value.toInt() == 1);
                    fieldsParsed++;
                }
            }

            if (comma == -1) break;
            start = comma + 1;
        }

        // Require all 6 fields to consider the packet valid.
        return fieldsParsed == 6;
    }
}

bool update() {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        return false; // nothing received this call
    }

    rxBuffer = "";
    rxBuffer.reserve(packetSize);
    while (LoRa.available()) {
        rxBuffer += (char)LoRa.read();
    }

    TelemetryPacket parsed;
    bool ok = parsePacket(rxBuffer, parsed);

    if (ok) {
        parsed.valid = true;
        parsed.rssi = LoRa.packetRssi();
        parsed.lastReceivedMillis = millis();
        latest = parsed;

        Serial.printf(
            "[LORA] R:%.2f P:%.2f Y:%.2f LAT:%.6f LON:%.6f FIX:%d RSSI:%d\n",
            latest.roll, latest.pitch, latest.yaw,
            latest.latitude, latest.longitude,
            latest.gpsFixValid, latest.rssi);
    } else {
        Serial.print(F("[LORA] Malformed packet: "));
        Serial.println(rxBuffer);
    }

    return ok;
}

const TelemetryPacket &getLatestPacket() {
    return latest;
}

bool isSignalLost(unsigned long timeoutMs) {
    if (latest.lastReceivedMillis == 0) return true; // never received anything
    return (millis() - latest.lastReceivedMillis) > timeoutMs;
}

} // namespace lora
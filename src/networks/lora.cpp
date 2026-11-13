#include "lora.h"
#include "config/pin.h"
#include <SPI.h>
#include <LoRa.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "security/aes.h"

namespace lora
{

    namespace
    {
        // ---- Radio settings (must match the transmitter) -------------------
        constexpr long LORA_FREQUENCY = 433E6;
        constexpr int LORA_SPREADING_FACTOR = 7;
        constexpr long LORA_BANDWIDTH = 125E3;
        constexpr int LORA_CODING_RATE_DENOM = 5; // 4/5
        constexpr int LORA_SYNC_WORD = 0x12;

        // ---- Frame layout: AAD | NONCE | LEN | CIPHERTEXT | TAG ------------
        constexpr size_t AAD_SIZE = 2;
        constexpr size_t NONCE_SIZE = 12;
        constexpr size_t LEN_SIZE = 1;
        constexpr size_t TAG_SIZE = 16;
        constexpr size_t FRAME_OVERHEAD = AAD_SIZE + NONCE_SIZE + LEN_SIZE + TAG_SIZE; // 31
        constexpr size_t MAX_LORA_PACKET = 255;
        constexpr size_t MAX_PAYLOAD = MAX_LORA_PACKET - FRAME_OVERHEAD; // 224

        struct Frame
        {
            uint8_t aad[AAD_SIZE];
            uint8_t nonce[NONCE_SIZE];
            uint8_t length;
            uint8_t ciphertext[MAX_PAYLOAD];
            uint8_t tag[TAG_SIZE];
        };

        // ---- Payload fields: R,P,Y,LAT,LON,FIX,S ----------------------------
        enum FieldMask : uint8_t
        {
            F_ROLL = 1 << 0,
            F_PITCH = 1 << 1,
            F_YAW = 1 << 2,
            F_LAT = 1 << 3,
            F_LON = 1 << 4,
            F_FIX = 1 << 5,
            F_DANGER = 1 << 6,
            F_ALL = 0x7F
        };

        TelemetryPacket latest;
        Stats stats;

        // Reads exactly n bytes; false if the radio FIFO runs short.
        bool readExact(uint8_t *dst, size_t n)
        {
            return LoRa.readBytes(dst, n) == n;
        }

        // Reads and validates the frame structure (no crypto yet).
        bool readFrame(int packetSize, Frame &f)
        {
            if (packetSize < (int)(FRAME_OVERHEAD + 1))
                return false;

            if (!readExact(f.aad, AAD_SIZE) || !readExact(f.nonce, NONCE_SIZE))
                return false;

            const int len = LoRa.read();
            if (len <= 0 || len > (int)MAX_PAYLOAD)
                return false;

            // Declared length must match what actually arrived.
            if (packetSize != (int)(FRAME_OVERHEAD + len))
                return false;

            f.length = (uint8_t)len;
            return readExact(f.ciphertext, f.length) && readExact(f.tag, TAG_SIZE);
        }

        // Trims whitespace in place and returns the start of the trimmed string.
        char *trim(char *s)
        {
            while (*s && isspace((unsigned char)*s))
                s++;
            char *end = s + strlen(s);
            while (end > s && isspace((unsigned char)end[-1]))
                --end;
            *end = '\0';
            return s;
        }

        // Parses "R:2.13,P:-1.05,Y:184.30,LAT:-5.123456,LON:120.123456,FIX:1,S:1"
        // Modifies `text` in place (no heap allocation).
        bool parsePayload(char *text, TelemetryPacket &out)
        {
            uint8_t seen = 0;
            char *save = nullptr;

            for (char *tok = strtok_r(text, ",", &save); tok; tok = strtok_r(nullptr, ",", &save))
            {
                char *colon = strchr(tok, ':');
                if (!colon)
                    continue;

                *colon = '\0';
                const char *key = trim(tok);
                const char *val = trim(colon + 1);

                if (!strcmp(key, "R"))
                {
                    out.roll = strtof(val, nullptr);
                    seen |= F_ROLL;
                }
                else if (!strcmp(key, "P"))
                {
                    out.pitch = strtof(val, nullptr);
                    seen |= F_PITCH;
                }
                else if (!strcmp(key, "Y"))
                {
                    out.yaw = strtof(val, nullptr);
                    seen |= F_YAW;
                }
                else if (!strcmp(key, "LAT"))
                {
                    out.latitude = strtod(val, nullptr);
                    seen |= F_LAT;
                }
                else if (!strcmp(key, "LON"))
                {
                    out.longitude = strtod(val, nullptr);
                    seen |= F_LON;
                }
                else if (!strcmp(key, "FIX"))
                {
                    out.gpsFixValid = (atoi(val) == 1);
                    seen |= F_FIX;
                }
                else if (!strcmp(key, "S"))
                {
                    out.is_danger = (atoi(val) == 1);
                    seen |= F_DANGER;
                }
            }

            // Every field must appear at least once (duplicates can't fake it).
            return seen == F_ALL;
        }
    } // namespace

    bool initialize()
    {
        SPI.begin(pin::LORA_SCK, pin::LORA_MISO, pin::LORA_MOSI, pin::LORA_NSS);
        LoRa.setPins(pin::LORA_NSS, pin::LORA_RST, pin::LORA_DIO0);

        if (!LoRa.begin(LORA_FREQUENCY))
        {
            Serial.println(F("[LORA] LoRa init failed. Check wiring."));
            return false;
        }

        LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH);
        LoRa.setCodingRate4(LORA_CODING_RATE_DENOM);
        LoRa.setSyncWord(LORA_SYNC_WORD);

        Serial.println(F("[LORA] LoRa initialized at 433 MHz."));
        return true;
    }

    bool update()
    {
        const int packetSize = LoRa.parsePacket();
        if (packetSize <= 0)
            return false;

        Frame frame;
        if (!readFrame(packetSize, frame))
        {
            stats.malformedFrames++;
            Serial.printf("[LORA] Malformed frame (%d bytes)\n", packetSize);
            return false;
        }

        // TODO: check frame.aad (version / device ID) and nonce replay state here.

        char plaintext[MAX_PAYLOAD + 1];
        const bool authentic = aes::decrypt(
            frame.ciphertext,
            frame.length,
            frame.nonce,
            frame.aad,
            sizeof(frame.aad),
            frame.tag,
            reinterpret_cast<uint8_t *>(plaintext));

        if (!authentic)
        {
            stats.authFailures++;
            Serial.println(F("[LORA] Authentication failed!"));
            return false;
        }

        plaintext[frame.length] = '\0';
        Serial.printf("[LORA] Decrypted data: %s\n", plaintext);

        TelemetryPacket parsed;
        if (!parsePayload(plaintext, parsed)) // note: modifies plaintext
        {
            stats.parseFailures++;
            Serial.println(F("[LORA] Malformed payload"));
            return false;
        }

        parsed.valid = true;
        parsed.rssi = LoRa.packetRssi();
        parsed.lastReceivedMillis = millis();
        latest = parsed;
        stats.received++;

        Serial.printf(
            "[LORA] R:%.2f P:%.2f Y:%.2f LAT:%.6f LON:%.6f FIX:%d S:%d RSSI:%d\n",
            latest.roll, latest.pitch, latest.yaw,
            latest.latitude, latest.longitude,
            latest.gpsFixValid, latest.is_danger, latest.rssi);

        return true;
    }

    const TelemetryPacket &getLatestPacket()
    {
        return latest;
    }

    const Stats &getStats()
    {
        return stats;
    }

    bool isSignalLost(unsigned long timeoutMs)
    {
        if (latest.lastReceivedMillis == 0)
            return true; // never received anything
        return (millis() - latest.lastReceivedMillis) > timeoutMs;
    }

} // namespace lora
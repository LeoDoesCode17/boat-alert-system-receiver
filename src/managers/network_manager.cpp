#include "managers/network_manager.h"

namespace network_manager {

bool initialize() {
    bool ok = true;

    if (!lora::initialize()) {
        Serial.println(F("[network_manager] LoRa init failed."));
        ok = false;
    }

    // Future: wifi::initialize(), mqtt::initialize() go here.
    // Decide whether their failure should also flip `ok` to false,
    // or just be logged as non-critical, depending on your app's needs.

    return ok;
}

void update() {
    lora::update();

    // Future: wifi::update(), mqtt::update() go here.
}

const lora::TelemetryPacket &getLatestTelemetry() {
    return lora::getLatestPacket();
}

bool isLoraSignalLost(unsigned long timeoutMs) {
    return lora::isSignalLost(timeoutMs);
}

} // namespace network_manager
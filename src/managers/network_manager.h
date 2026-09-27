#pragma once
#include <Arduino.h>
#include "networks/lora.h"

namespace network_manager {

    // Initializes all network subsystems (LoRa now; WiFi/MQTT later).
    // Returns false if any critical subsystem fails to init.
    bool initialize();

    // Call every loop(). Drives all network subsystems' non-blocking updates.
    void update();

    // --- LoRa passthrough ---
    // Keeps callers decoupled from networks/lora.h internals.
    const lora::TelemetryPacket &getLatestTelemetry();
    bool isLoraSignalLost(unsigned long timeoutMs = 5000);

} // namespace network_manager
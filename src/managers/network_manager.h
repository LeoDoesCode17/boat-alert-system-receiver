#pragma once
#include <Arduino.h>
#include "networks/lora.h"
#include "core/type.h"

namespace network_manager
{

    // Initializes all network subsystems (LoRa now; WiFi/MQTT later).
    // Returns false if any critical subsystem fails to init.
    bool initialize();

    // Call every loop(). Drives all network subsystems' non-blocking updates.
    void update();

    // --- LoRa passthrough ---
    // Keeps callers decoupled from networks/lora.h internals.
    const lora::TelemetryPacket &getLatestTelemetry();
    bool isLoraSignalLost(unsigned long timeoutMs = 5000);

    // for configure network creds using web page
    bool start_ap();                   // Turn on AP + web server. Returns false if the AP failed to start
    void stop_ap();                    // Turn off web server + AP (safe to call even if not running)
    void update_ap();                  // Must be called from loop(): lets the web server serve clients
    bool isActive();                // true while configuration mode is running
    bool hasSubmittedCredentials(); // true after the user submitted the web form
    String getApSsid();             // Name of the Wi-Fi network the ESP32 creates (for the LCD guide)
    String getApPassword();         // Password of that network (for the LCD guide)
    String getApIp();               // IP address of the web page (for the LCD guide)

    WifiConfig getWifiConfig(); // Everything received for Wi-Fi (replaces getReceivedSsid)
    MqttConfig getMqttConfig(); // Everything received for the MQTT broker

} // namespace network_manager
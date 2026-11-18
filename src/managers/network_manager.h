#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "networks/lora.h"
#include "core/type.h"

namespace network_manager
{
    // ---- Lifecycle ------------------------------------------------------------
    // Core 1 = LoRa (called from setup() and mainTask).
    // Core 0 = Wi-Fi + MQTT (called from networkTask).
    // initialize_on_core_1() must run before the tasks start: it creates the mutexes.
    bool initialize_on_core_1();
    bool initialize_on_core_0();

    void update_on_core_0(); // Non-blocking-ish: drives Wi-Fi + MQTT connect/reconnect
    void update_on_core_1(); // Drives LoRa receive. Call ONLY from the core-1 task.

    // ---- LoRa passthrough -------------------------------------------------------
    const lora::TelemetryPacket &getLatestTelemetry();
    bool isLoraSignalLost(unsigned long timeoutMs = 5000);

    // ---- Connectivity status (safe to call from any task / core) -----------------
    bool isWifiConnected(); // Wi-Fi station is connected to the configured network
    bool isMqttConnected(); // MQTT session with the configured broker is up
    bool isOnline();        // Wi-Fi AND MQTT both up
    IPAddress getLocalIp(); // Station IP address, 0.0.0.0 while Wi-Fi is not connected

    // ---- Configuration mode: set credentials through a web page -------------------
    bool start_ap();                // Turn on AP + web server. Returns false if the AP failed to start.
                                    // While the AP is on, Wi-Fi/MQTT are suspended; they resume after stop_ap().
    void stop_ap();                 // Turn off web server + AP (safe to call even if not running)
    void update_ap();               // Call from the core-1 task: lets the web server serve clients
    bool isActive();                // true while configuration mode is running
    bool hasSubmittedCredentials(); // true after the user submitted the web form
    String getApSsid();             // Name of the Wi-Fi network the ESP32 creates (for the UI guide)
    String getApPassword();         // Password of that network (for the UI guide)
    String getApIp();               // IP address of the web page (for the UI guide)

    WifiConfig getWifiConfig(); // Copy of the current Wi-Fi settings (thread-safe)
    MqttConfig getMqttConfig(); // Copy of the current MQTT settings (thread-safe)

} // namespace network_manager
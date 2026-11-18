#include "managers/network_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "nvs_manager.h"

namespace
{
    // --- Access point (configuration mode) ----------------------------------
    const char *AP_SSID = "ESP32_IoT_Setup"; // Network name the user will see on their phone (later use mac address)
    const char *AP_PASSWORD = "12345678";    // WPA2 password (must be at least 8 characters)

    // --- Connectivity tuning --------------------------------------------------
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000; // give up on one attempt after this long
    constexpr uint32_t WIFI_BACKOFF_MIN_MS = 2000;      // wait before the next attempt (doubles on every failure)
    constexpr uint32_t WIFI_BACKOFF_MAX_MS = 30000;
    constexpr uint32_t MQTT_BACKOFF_MIN_MS = 2000;
    constexpr uint32_t MQTT_BACKOFF_MAX_MS = 30000;
    constexpr uint16_t MQTT_KEEPALIVE_S = 15;
    constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 5; // max time a blocking MQTT connect may take

    // --- Mutex helper (RAII) --------------------------------------------------
    class Lock
    {
    public:
        explicit Lock(SemaphoreHandle_t m) : _m(m)
        {
            if (_m)
                xSemaphoreTake(_m, portMAX_DELAY);
        }
        ~Lock()
        {
            if (_m)
                xSemaphoreGive(_m);
        }
        Lock(const Lock &) = delete;
        Lock &operator=(const Lock &) = delete;

    private:
        SemaphoreHandle_t _m;
    };

    // --- State shared between cores ----------------------------------------------
    SemaphoreHandle_t cfgMutex = nullptr;   // guards receivedWifi / receivedMqtt (Strings are not thread-safe)
    SemaphoreHandle_t radioMutex = nullptr; // serializes WiFi mode changes: AP start/stop (core 1) vs station connect (core 0)
    std::atomic<bool> active{false};        // Is configuration mode (AP) running? Core 0 pauses Wi-Fi/MQTT while true
    std::atomic<uint32_t> configVersion{0}; // Bumped whenever the stored credentials change
    std::atomic<bool> wifiUp{false};        // Published by core 0, read by anyone
    std::atomic<bool> mqttUp{false};

    WifiConfig receivedWifi; // Always access through cfgMutex
    MqttConfig receivedMqtt;

    // --- State used by core 1 only (web server runs in the main task) -----------
    WebServer server(80);            // HTTP server listening on port 80
    bool submitted = false;          // Did the user submit the form?
    bool handlersRegistered = false; // Make sure we register URL handlers only once

    // --- State used by core 0 only (network task) ------------------------------
    enum class WifiState : uint8_t
    {
        Disconnected,
        Connecting,
        Connected
    };

    WiFiClient netClient;
    PubSubClient mqttClient(netClient);

    WifiConfig wifiCfg; // Private copies used for connecting; refreshed when configVersion changes
    MqttConfig mqttCfg; // (PubSubClient keeps a pointer to mqttCfg.host, so it must stay alive)
    uint32_t appliedVersion = 0;

    WifiState wifiState = WifiState::Disconnected;
    uint32_t wifiAttemptStart = 0;
    uint32_t wifiNextAttempt = 0;
    uint32_t wifiBackoff = WIFI_BACKOFF_MIN_MS;
    uint32_t mqttNextAttempt = 0;
    uint32_t mqttBackoff = MQTT_BACKOFF_MIN_MS;
    char mqttClientId[24] = "esp32-receiver";

    // --- Web page ---------------------------------------------------------------
    const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Network Setup</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 40px auto; max-width: 400px; padding: 0 20px; background: #f4f4f9; }
    input { width: 100%; padding: 10px; margin: 8px 0 16px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
    button { width: 100%; background-color: #007BFF; color: white; padding: 12px; border: none; border-radius: 4px; font-size: 16px; }
    small { color: #777; }
  </style>
</head>
<body>
  <h2>Network Setup</h2>
  <form action="/save" method="POST">
    <h3>Wi-Fi</h3>
    <label for="ssid">Wi-Fi SSID:</label>
    <input type="text" id="ssid" name="ssid" required>
    <label for="pass">Wi-Fi Password:</label>
    <input type="password" id="pass" name="pass">

    <h3>MQTT Broker</h3>
    <label for="mqtt_host">Broker Host:</label>
    <input type="text" id="mqtt_host" name="mqtt_host" placeholder="broker.example.com" required>
    <label for="mqtt_port">Broker Port:</label>
    <input type="number" id="mqtt_port" name="mqtt_port" value="1883" min="1" max="65535" required>
    <label for="mqtt_user">Username <small>(optional)</small>:</label>
    <input type="text" id="mqtt_user" name="mqtt_user">
    <label for="mqtt_pass">Password <small>(optional)</small>:</label>
    <input type="password" id="mqtt_pass" name="mqtt_pass">

    <button type="submit">Save</button>
  </form>
</body>
</html>
)rawliteral";

    // Called when a browser requests "GET /"  -> send the form page
    void handleRoot()
    {
        server.send_P(200, "text/html", INDEX_HTML); // 200 = OK; send_P sends data stored in PROGMEM
    }

    // Called when the form is submitted: "POST /save"
    void handleSave()
    {
        // --- 1. Read every field into local variables first -------------------
        // server.arg("name") returns an empty String if the field is missing.
        const String ssid = server.arg("ssid");
        const String wifiPass = server.arg("pass");

        String host = server.arg("mqtt_host");
        String portText = server.arg("mqtt_port");
        String mqttUser = server.arg("mqtt_user");
        const String mqttPass = server.arg("mqtt_pass");

        host.trim(); // Remove accidental leading/trailing spaces (trim() edits in place)
        portText.trim();
        mqttUser.trim();

        // --- 2. Validate (the browser also checks, but never trust the client) --
        if (ssid.length() == 0)
        {
            server.send(400, "text/plain", "Missing Wi-Fi SSID"); // 400 = Bad Request
            return;
        }
        if (host.length() == 0)
        {
            server.send(400, "text/plain", "Missing MQTT broker host");
            return;
        }

        const long port = portText.toInt(); // toInt() returns 0 if the text is not a number
        if (port < 1 || port > 65535)       // Valid TCP port range
        {
            server.send(400, "text/plain", "Invalid MQTT port (1-65535)");
            return;
        }

        // --- 3. Everything is valid: build the new config, then publish it at once ---
        // (Storing only after all checks pass avoids half-saved configurations.)
        WifiConfig newWifi;
        newWifi.ssid = ssid;
        newWifi.password = wifiPass; // May be empty for open networks

        MqttConfig newMqtt;
        newMqtt.host = host;
        newMqtt.port = static_cast<uint16_t>(port); // Safe: range was checked above
        newMqtt.username = mqttUser;                // Empty = no authentication
        newMqtt.password = mqttPass;

        // why need a new branch? : to make the lock goes out of scope to trigger destructor
        {
            Lock lock(cfgMutex); // The network task (core 0) reads these too
            receivedWifi = newWifi;
            receivedMqtt = newMqtt;
        }
        configVersion++; // Tells the network task to reconnect with the new credentials
                         // (it stays paused until the AP is stopped, i.e. you leave config mode)

        // For now we only print the values to Serial
        Serial.println("[WifiConfig] Configuration received:");
        Serial.print("  Wi-Fi SSID    : ");
        Serial.println(newWifi.ssid);
        Serial.print("  Wi-Fi Password: ");
        Serial.println(newWifi.password); // Debug only: remove once you store these securely
        Serial.print("  MQTT Host     : ");
        Serial.println(newMqtt.host);
        Serial.print("  MQTT Port     : ");
        Serial.println(newMqtt.port);
        Serial.print("  MQTT Username : ");
        Serial.println(newMqtt.username.length() > 0 ? newMqtt.username : String("(none)"));
        Serial.print("  MQTT Password : ");
        Serial.println(newMqtt.password.length() > 0 ? newMqtt.password : String("(none)")); // Debug only

        submitted = true; // Tell the UI that the form was submitted

        // Reply to the browser so the user knows it worked
        server.send(200, "text/html", "<h2>Configuration received!</h2><p>You can close this page.</p>");

        // store the new network (wifi and mqtt) configuration to nvs
        nvs_manager::store_network_config(newWifi, newMqtt);
    }

    // ===========================================================================
    //  Wi-Fi / MQTT connectivity (runs on core 0, in the network task)
    // ===========================================================================

    // Wrap-safe "has this deadline passed?" for millis() timestamps.
    bool reached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0;
    }

    uint32_t doubled(uint32_t value, uint32_t maxValue)
    {
        return (value * 2 > maxValue) ? maxValue : value * 2;
    }

    void publishStatus(bool wifi, bool mqtt)
    {
        wifiUp = wifi;
        mqttUp = mqtt;
    }

    // Closes the MQTT session and the TCP socket (does not touch the Wi-Fi radio).
    void dropMqtt()
    {
        mqttClient.disconnect();
        netClient.stop();
        mqttUp = false;
    }

    // Copies the latest credentials into the core-0 private copies.
    void loadConfig()
    {
        // why need a new branch? to make the lock goes out of scope to trigger destructor
        {
            Lock lock(cfgMutex);
            wifiCfg = receivedWifi;
            mqttCfg = receivedMqtt;
        }
        // PubSubClient only stores the pointer, so it must point at our long-lived copy.
        mqttClient.setServer(mqttCfg.host.c_str(), mqttCfg.port);
    }

    // Config mode owns the radio: drop everything and wait. Cheap when already idle.
    // this will be called from task pinned on core 0? 
    void suspendConnectivity()
    {
        if (wifiState != WifiState::Disconnected || wifiUp || mqttUp)
        {
            dropMqtt(); // the sockets die anyway once the station is switched off
            wifiState = WifiState::Disconnected;
            publishStatus(false, false);
            Serial.println(F("[NETWORK-MANAGER] Config mode active - Wi-Fi/MQTT suspended"));
        }
        // Reconnect immediately after config mode ends.
        wifiNextAttempt = millis();
        wifiBackoff = WIFI_BACKOFF_MIN_MS;
        mqttNextAttempt = millis();
        mqttBackoff = MQTT_BACKOFF_MIN_MS;
    }

    void wifiStep(uint32_t now)
    {
        switch (wifiState)
        {
        case WifiState::Disconnected:
        {
            if (!reached(now, wifiNextAttempt))
                return;

                // why need new branch? to make the lock goes out of scope to trigger destructor
            {
                Lock lock(radioMutex);
                if (active) // AP got started in the meantime: leave the radio alone
                    return;
                WiFi.mode(WIFI_STA);
                WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.password.length() > 0 ? wifiCfg.password.c_str() : nullptr);
            }
            wifiAttemptStart = now;
            wifiState = WifiState::Connecting;
            Serial.printf("[NETWORK-MANAGER] Wi-Fi connecting to \"%s\"...\n", wifiCfg.ssid.c_str());
            break;
        }

        case WifiState::Connecting:
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                wifiState = WifiState::Connected;
                wifiBackoff = WIFI_BACKOFF_MIN_MS;
                wifiUp = true;
                Serial.printf("[NETWORK-MANAGER] Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
            }
            else if (now - wifiAttemptStart > WIFI_CONNECT_TIMEOUT_MS)
            {
                // why need a new branch? to make the lock goes out of scope to trigger destructor
                {
                    Lock lock(radioMutex);
                    if (!active)
                        WiFi.disconnect();
                }
                wifiState = WifiState::Disconnected;
                wifiNextAttempt = now + wifiBackoff;
                Serial.printf("[NETWORK-MANAGER] Wi-Fi connect failed, retry in %lu ms\n", (unsigned long)wifiBackoff);
                wifiBackoff = doubled(wifiBackoff, WIFI_BACKOFF_MAX_MS);
            }
            break;
        }

        case WifiState::Connected:
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                dropMqtt();
                publishStatus(false, false);
                wifiState = WifiState::Disconnected;
                wifiNextAttempt = now + WIFI_BACKOFF_MIN_MS;
                Serial.println(F("[NETWORK-MANAGER] Wi-Fi lost, reconnecting..."));
            }
            break;
        }
        }
    }

    void mqttStep(uint32_t now)
    {
        if (mqttCfg.host.length() == 0) // no broker configured
        {
            mqttUp = false;
            return;
        }

        if (mqttClient.connected())
        {
            mqttClient.loop(); // keep-alive + incoming messages (later)
            mqttUp = true;
            return;
        }

        if (mqttUp) // it was up a moment ago: the session just dropped
        {
            mqttUp = false;
            mqttNextAttempt = now + MQTT_BACKOFF_MIN_MS;
            Serial.println(F("[NETWORK-MANAGER] MQTT disconnected, reconnecting..."));
        }

        if (!reached(now, mqttNextAttempt))
            return;

        Serial.printf("[NETWORK-MANAGER] MQTT connecting to %s:%u...\n", mqttCfg.host.c_str(), mqttCfg.port);

        // This call blocks for at most MQTT_SOCKET_TIMEOUT_S (+ DNS). That's why it lives on core 0.
        const bool ok = (mqttCfg.username.length() > 0)
                            ? mqttClient.connect(mqttClientId, mqttCfg.username.c_str(), mqttCfg.password.c_str())
                            : mqttClient.connect(mqttClientId);

        if (ok)
        {
            mqttUp = true;
            mqttBackoff = MQTT_BACKOFF_MIN_MS;
            Serial.println(F("[NETWORK-MANAGER] MQTT connected"));
        }
        else
        {
            // state: -4 timeout, -2 connect failed, 4 bad credentials, 5 not authorized
            Serial.printf("[NETWORK-MANAGER] MQTT connect failed (state %d), retry in %lu ms\n",
                          mqttClient.state(), (unsigned long)mqttBackoff);
            mqttNextAttempt = millis() + mqttBackoff;
            mqttBackoff = doubled(mqttBackoff, MQTT_BACKOFF_MAX_MS);
        }
    }

    void createMutexes()
    {
        // Called from both initializers. initialize_on_core_1() runs in setup() before the
        // tasks exist, so this is never executed concurrently.
        if (cfgMutex == nullptr)
            cfgMutex = xSemaphoreCreateMutex();
        if (radioMutex == nullptr)
            radioMutex = xSemaphoreCreateMutex();
    }
}

namespace network_manager
{
    bool initialize_on_core_1()
    {
        createMutexes();

        bool ok = true;

        if (!lora::initialize())
        {
            Serial.println(F("[NETWORK-MANAGER] LoRa init failed."));
            ok = false;
        }

        if (!ok)
        {
            Serial.println("[NETWORK-MANAGER] Initialize network on core 1 failed");
        }

        return ok;
    }

    bool initialize_on_core_0()
    {
        createMutexes();

        WiFi.persistent(false);       // don't rewrite Wi-Fi settings to flash on every begin()
        WiFi.setAutoReconnect(false); // reconnects are driven by update_on_core_0()
        mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
        mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

        // Unique client id (two clients with the same id kick each other off the broker)
        const uint64_t mac = ESP.getEfuseMac();
        snprintf(mqttClientId, sizeof(mqttClientId), "esp32-rx-%06X",
                 static_cast<unsigned>((mac >> 24) & 0xFFFFFF));

        WifiConfig wifi;
        MqttConfig mqtt;
        nvs_manager::read_network_config(wifi, mqtt);

        // why need a new branch? to make the lock goes out of scope to trigger destructor
        {
            Lock lock(cfgMutex);
            receivedWifi = wifi;
            receivedMqtt = mqtt;
        }

        configVersion++; // makes the first update_on_core_0() load the config and start connecting

        return true;
    }

    void update_on_core_0()
    {
        // 1. Configuration mode owns the radio: stay out of the way.
        if (active)
        {
            suspendConnectivity();
            return;
        }

        const uint32_t now = millis();

        // 2. New or changed credentials: restart the connection from scratch.
        const uint32_t version = configVersion;
        if (version != appliedVersion)
        {
            appliedVersion = version;
            loadConfig();
            dropMqtt();

            // why need a new branch? to make the lock goes out of scope to trigger destructor
            {
                Lock lock(radioMutex);
                if (!active)
                    WiFi.disconnect();
            }

            wifiState = WifiState::Disconnected;
            publishStatus(false, false);
            wifiBackoff = WIFI_BACKOFF_MIN_MS;
            mqttBackoff = MQTT_BACKOFF_MIN_MS;
            wifiNextAttempt = now;
            mqttNextAttempt = now;
        }

        // 3. Nothing configured yet.
        if (wifiCfg.ssid.length() == 0)
            return;

        // 4. Keep Wi-Fi up, and MQTT on top of it.
        wifiStep(now);
        if (wifiState == WifiState::Connected)
            mqttStep(now);
    }

    void update_on_core_1()
    {
        lora::update();
    }

    const lora::TelemetryPacket &getLatestTelemetry()
    {
        return lora::getLatestPacket();
    }

    bool isLoraSignalLost(unsigned long timeoutMs)
    {
        return lora::isSignalLost(timeoutMs);
    }

    bool isWifiConnected() { return wifiUp; }
    bool isMqttConnected() { return mqttUp; }
    bool isOnline() { return wifiUp && mqttUp; }

    bool start_ap()
    {
        if (active) // Already running? Nothing to do
        {
            return true;
        }

        submitted = false; // Reset the state from any previous session

        // why need a new branch
        {
            Lock lock(radioMutex); // The network task must not touch the radio while we switch modes

            WiFi.mode(WIFI_AP); // Access Point mode: the ESP32 creates its own network

            if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) // Start the AP; returns false on failure
            {
                WiFi.mode(WIFI_OFF); // Clean up the radio
                return false;        // Let the caller (UI) show an error
            }

            active = true; // From now on the network task suspends Wi-Fi/MQTT
        }

        if (!handlersRegistered)                       // Register URL routes only on the first start,
        {                                              // otherwise they would be duplicated every time
            server.on("/", HTTP_GET, handleRoot);      // GET  /      -> show form
            server.on("/save", HTTP_POST, handleSave); // POST /save  -> receive form data
            handlersRegistered = true;
        }

        server.begin(); // Start listening for HTTP clients

        Serial.print("[WifiConfig] AP started: ");
        Serial.print(AP_SSID);
        Serial.print("  IP: ");
        Serial.println(WiFi.softAPIP()); // Default AP IP is 192.168.4.1
        return true;
    }

    void stop_ap()
    {
        if (!active) // Not running? Nothing to stop
        {
            return;
        }

        server.stop(); // Stop listening for HTTP clients

        // why need a new branch? to make the lock goes out of scope to trigger destructor
        {
            Lock lock(radioMutex);
            WiFi.softAPdisconnect(true); // Shut down the access point
            WiFi.mode(WIFI_OFF);         // Turn the Wi-Fi radio off (the network task re-enables the station)
            active = false;              // Mark configuration mode as stopped
        }
        Serial.println("[WifiConfig] Stopped");
    }

    void update_ap()
    {
        if (active) // Only serve clients while config mode is on
        {
            server.handleClient(); // Process any pending HTTP request (non-blocking)
        }
    }

    bool isActive() { return active; }
    bool hasSubmittedCredentials() { return submitted; }
    String getApSsid() { return String(AP_SSID); }
    String getApPassword() { return String(AP_PASSWORD); }
    String getApIp() { return WiFi.softAPIP().toString(); }

    WifiConfig getWifiConfig()
    {
        Lock lock(cfgMutex);
        return receivedWifi; // Copy is made before the lock is released
    }

    MqttConfig getMqttConfig()
    {
        Lock lock(cfgMutex);
        return receivedMqtt;
    }

} // namespace network_manager
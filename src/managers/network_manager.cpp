#include "managers/network_manager.h"
#include <WiFi.h>
#include <WebServer.h>

namespace
{
    // --- Configuration constants -------------------------------------------
    const char *AP_SSID = "ESP32_IoT_Setup"; // Network name the user will see on their phone (later use mac address)
    const char *AP_PASSWORD = "12345678";    // WPA2 password (must be at least 8 characters)

    // --- State ---------------------------------------------------------------
    WebServer server(80);            // HTTP server listening on port 80
    bool active = false;             // Is configuration mode running?
    bool submitted = false;          // Did the user submit the form?
    bool handlersRegistered = false; // Make sure we register URL handlers only once

    WifiConfig receivedWifi;
    MqttConfig receivedMqtt;

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

        // --- 3. Everything is valid: store it all at once ---------------------
        // (Storing only after all checks pass avoids half-saved configurations.)
        receivedWifi.ssid = ssid;
        receivedWifi.password = wifiPass; // May be empty for open networks

        receivedMqtt.host = host;
        receivedMqtt.port = static_cast<uint16_t>(port); // Safe: range was checked above
        receivedMqtt.username = mqttUser;                // Empty = no authentication
        receivedMqtt.password = mqttPass;

        // For now we only print the values to Serial (no WiFi.begin / MQTT connect yet)
        Serial.println("[WifiConfig] Configuration received:");
        Serial.print("  Wi-Fi SSID    : ");
        Serial.println(receivedWifi.ssid);
        Serial.print("  Wi-Fi Password: ");
        Serial.println(receivedWifi.password); // Debug only: remove once you store these securely
        Serial.print("  MQTT Host     : ");
        Serial.println(receivedMqtt.host);
        Serial.print("  MQTT Port     : ");
        Serial.println(receivedMqtt.port);
        Serial.print("  MQTT Username : ");
        Serial.println(receivedMqtt.username.length() > 0 ? receivedMqtt.username : String("(none)"));
        Serial.print("  MQTT Password : ");
        Serial.println(receivedMqtt.password.length() > 0 ? receivedMqtt.password : String("(none)")); // Debug only

        submitted = true; // Tell the UI that the form was submitted

        // Reply to the browser so the user knows it worked
        server.send(200, "text/html",
                    "<h2>Configuration received!</h2><p>You can close this page.</p>");
    }

}

namespace network_manager
{

    bool initialize()
    {
        bool ok = true;

        if (!lora::initialize())
        {
            Serial.println(F("[network_manager] LoRa init failed."));
            ok = false;
        }

        // Future: wifi::initialize(), mqtt::initialize() go here.
        // Decide whether their failure should also flip `ok` to false,
        // or just be logged as non-critical, depending on your app's needs.

        return ok;
    }

    void update()
    {
        lora::update();

        // Future: wifi::update(), mqtt::update() go here.
    }

    const lora::TelemetryPacket &getLatestTelemetry()
    {
        return lora::getLatestPacket();
    }

    bool isLoraSignalLost(unsigned long timeoutMs)
    {
        return lora::isSignalLost(timeoutMs);
    }

    bool start_ap()
    {
        if (active) // Already running? Nothing to do
        {
            return true;
        }

        submitted = false; // Reset the state from any previous session

        WiFi.mode(WIFI_AP); // Access Point mode: the ESP32 creates its own network

        if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) // Start the AP; returns false on failure
        {
            WiFi.mode(WIFI_OFF); // Clean up the radio
            return false;        // Let the caller (UI) show an error
        }

        if (!handlersRegistered)                       // Register URL routes only on the first start,
        {                                              // otherwise they would be duplicated every time
            server.on("/", HTTP_GET, handleRoot);      // GET  /      -> show form
            server.on("/save", HTTP_POST, handleSave); // POST /save  -> receive form data
            handlersRegistered = true;
        }

        server.begin(); // Start listening for HTTP clients
        active = true;  // Mark configuration mode as running

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

        server.stop();               // Stop listening for HTTP clients
        WiFi.softAPdisconnect(true); // Shut down the access point
        WiFi.mode(WIFI_OFF);         // Turn the Wi-Fi radio off
        active = false;              // Mark configuration mode as stopped
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

    WifiConfig getWifiConfig() { return receivedWifi; } // Returns a copy of the received Wi-Fi settings
    MqttConfig getMqttConfig() { return receivedMqtt; } // Returns a copy of the received MQTT settings

} // namespace network_manager
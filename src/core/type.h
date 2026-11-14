#pragma once
#include <Arduino.h>

// Wi-Fi credentials entered in the web form
struct WifiConfig
{
    String ssid;
    String password; // Empty for open networks
};

// MQTT broker settings entered in the web form
struct MqttConfig
{
    String host;
    uint16_t port = 1883; // Stored as a number (validated 1-65535); 1883 is the MQTT default
    String username;      // Optional: empty means "connect without authentication"
    String password;      // Optional
};
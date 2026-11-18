#include "nvs_manager.h"
#include <Preferences.h>

namespace
{
    static Preferences preferences;
    const char *NVS_NETWORK_NAMESPACE = "network-creds";
}

namespace nvs_manager
{
    void store_network_config(const WifiConfig &_wifi_config, const MqttConfig &_mqtt_config)
    {
        const bool is_read_only = false;
        if (!preferences.begin(NVS_NETWORK_NAMESPACE, is_read_only))
        {
            Serial.println("[NVS-MANAGER] Failed to open NVS for writing");
            return;
        }

        Serial.println("[NVS-MANAGER] Storing network config into nvs");

        preferences.putString("wifi_ssid", _wifi_config.ssid);
        preferences.putString("wifi_pass", _wifi_config.password);
        preferences.putString("mqtt_host", _mqtt_config.host);
        preferences.putUShort("mqtt_port", _mqtt_config.port); // Use putUShort for uint16_t
        preferences.putString("mqtt_user", _mqtt_config.username);
        preferences.putString("mqtt_pass", _mqtt_config.password);

        preferences.end();
        Serial.println("[NVS-MANAGER] Stored network config and closed connection to NVS");
    }

    void read_network_config(WifiConfig &_wifi_config, MqttConfig &_mqtt_config)
    {
        const bool is_read_only = true;

        // On the very first boot before any config is saved, opening in read-only mode fails
        if (!preferences.begin(NVS_NETWORK_NAMESPACE, is_read_only))
        {
            Serial.println("[NVS-MANAGER] Namespace not found (first boot?), using defaults");
            return;
        }

        Serial.println("[NVS-MANAGER] Reading network config from NVS");

        _wifi_config.ssid = preferences.getString("wifi_ssid", "");
        _wifi_config.password = preferences.getString("wifi_pass", "");

        _mqtt_config.host = preferences.getString("mqtt_host", "");
        _mqtt_config.port = preferences.getUShort("mqtt_port", 1883); // Defaults to 1883 if missing
        _mqtt_config.username = preferences.getString("mqtt_user", "");
        _mqtt_config.password = preferences.getString("mqtt_pass", "");

        preferences.end();
        Serial.println("[NVS-MANAGER] Loaded network config and closed connection to NVS");
    }
}

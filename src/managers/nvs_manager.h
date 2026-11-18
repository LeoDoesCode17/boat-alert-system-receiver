#pragma once
#include "core/type.h"

namespace nvs_manager
{
    void store_network_config(const WifiConfig &_wifi_config, const MqttConfig &_mqtt_config);
    void read_network_config(WifiConfig &_wifi_config, MqttConfig &_mqtt_config);
}
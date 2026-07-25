#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

struct DeviceConfig {
    String wifi_ssid;
    String wifi_pass;
    String server_url;
    String display_token;
    uint32_t wifi_timeout_ms;
    uint32_t default_sleep_sec;
    uint8_t refresh_mode;
    bool audio_battery_alert;
    bool developer_mode;
    uint32_t maintenance_timeout_sec;
    uint32_t config_version;
};


class ConfigManager {
public:
    DeviceConfig config;

    ConfigManager();
    bool begin();
    bool load();
    bool save();
    String toJsonString();
    bool updateFromJson(const String& jsonStr);
    bool updateKey(const String& key, const String& val);
    void resetToDefaults();
};

extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H

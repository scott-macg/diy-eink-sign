#include "config_manager.h"

ConfigManager configManager;

ConfigManager::ConfigManager() {
    resetToDefaults();
}

void ConfigManager::resetToDefaults() {
    config.wifi_ssid = WIFI_SSID;
    config.wifi_pass = WIFI_PASS;
    config.server_url = SERVER_URL;
    config.display_token = DISPLAY_TOKEN;
    config.default_sleep_sec = DEFAULT_SLEEP_SEC;
    config.audio_battery_alert = AUDIO_BATTERY_ALERT_DEFAULT; // false by default
    config.developer_mode = DEVELOPER_MODE_DEFAULT; // false by default
    config.maintenance_timeout_sec = MAINTENANCE_TIMEOUT_SEC;
    config.config_version = 1;
}

bool ConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[CONFIG] LittleFS Mount Failed!");
        return false;
    }

    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
        Serial.println("[CONFIG] Configuration file not found. Creating default config.json...");
        resetToDefaults();
        return save();
    }

    return load();
}

bool ConfigManager::load() {
    File configFile = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!configFile) {
        Serial.println("[CONFIG] Failed to open config file for reading");
        resetToDefaults();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.printf("[CONFIG] Failed to parse config JSON: %s\n", error.c_str());
        resetToDefaults();
        return false;
    }

    config.wifi_ssid = doc["wifi_ssid"] | WIFI_SSID;
    config.wifi_pass = doc["wifi_pass"] | WIFI_PASS;
    config.server_url = doc["server_url"] | SERVER_URL;
    config.display_token = doc["display_token"] | DISPLAY_TOKEN;
    config.default_sleep_sec = doc["default_sleep_sec"] | DEFAULT_SLEEP_SEC;
    config.audio_battery_alert = doc["audio_battery_alert"] | AUDIO_BATTERY_ALERT_DEFAULT;
    config.developer_mode = doc["developer_mode"] | DEVELOPER_MODE_DEFAULT;
    config.maintenance_timeout_sec = doc["maintenance_timeout_sec"] | MAINTENANCE_TIMEOUT_SEC;
    config.config_version = doc["config_version"] | 1;

    Serial.println("[CONFIG] Configuration loaded successfully from LittleFS.");
    return true;
}

bool ConfigManager::save() {
    JsonDocument doc;
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_pass"] = config.wifi_pass;
    doc["server_url"] = config.server_url;
    doc["display_token"] = config.display_token;
    doc["default_sleep_sec"] = config.default_sleep_sec;
    doc["audio_battery_alert"] = config.audio_battery_alert;
    doc["developer_mode"] = config.developer_mode;
    doc["maintenance_timeout_sec"] = config.maintenance_timeout_sec;
    doc["config_version"] = config.config_version;

    File configFile = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!configFile) {
        Serial.println("[CONFIG] Failed to open config file for writing");
        return false;
    }

    if (serializeJson(doc, configFile) == 0) {
        Serial.println("[CONFIG] Failed to write to config file");
        configFile.close();
        return false;
    }

    configFile.close();
    Serial.println("[CONFIG] Configuration saved to LittleFS.");
    return true;
}

String ConfigManager::toJsonString() {
    JsonDocument doc;
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_pass"] = "********"; // Mask password in public export
    doc["server_url"] = config.server_url;
    doc["display_token"] = config.display_token;
    doc["default_sleep_sec"] = config.default_sleep_sec;
    doc["audio_battery_alert"] = config.audio_battery_alert;
    doc["developer_mode"] = config.developer_mode;
    doc["maintenance_timeout_sec"] = config.maintenance_timeout_sec;
    doc["config_version"] = config.config_version;

    String output;
    serializeJsonPretty(doc, output);
    return output;
}

bool ConfigManager::updateFromJson(const String& jsonStr) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.printf("[CONFIG] Update JSON parse error: %s\n", error.c_str());
        return false;
    }

    if (doc["wifi_ssid"]) config.wifi_ssid = doc["wifi_ssid"].as<String>();
    if (doc["wifi_pass"] && doc["wifi_pass"].as<String>() != "********") {
        config.wifi_pass = doc["wifi_pass"].as<String>();
    }
    if (doc["server_url"]) config.server_url = doc["server_url"].as<String>();
    if (doc["display_token"]) config.display_token = doc["display_token"].as<String>();
    if (doc["default_sleep_sec"]) config.default_sleep_sec = doc["default_sleep_sec"].as<uint32_t>();
    if (doc["audio_battery_alert"]) config.audio_battery_alert = doc["audio_battery_alert"].as<bool>();
    if (doc["developer_mode"]) config.developer_mode = doc["developer_mode"].as<bool>();
    if (doc["maintenance_timeout_sec"]) config.maintenance_timeout_sec = doc["maintenance_timeout_sec"].as<uint32_t>();

    config.config_version++;
    return save();
}

bool ConfigManager::updateKey(const String& key, const String& val) {
    if (key == "wifi_ssid") config.wifi_ssid = val;
    else if (key == "wifi_pass") config.wifi_pass = val;
    else if (key == "server_url") config.server_url = val;
    else if (key == "display_token") config.display_token = val;
    else if (key == "default_sleep_sec") config.default_sleep_sec = val.toInt();
    else if (key == "audio_battery_alert") config.audio_battery_alert = (val == "true" || val == "1");
    else if (key == "developer_mode") config.developer_mode = (val == "true" || val == "1");
    else if (key == "maintenance_timeout_sec") config.maintenance_timeout_sec = val.toInt();
    else {
        Serial.printf("[CONFIG] Unknown key: %s\n", key.c_str());
        return false;
    }

    config.config_version++;
    return save();
}


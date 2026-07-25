#include "manifest_manager.h"

static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

static size_t base64_decode(const char* in, size_t in_len, uint8_t* out, size_t max_out_len) {
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    size_t out_idx = 0;

    while (in_len-- && (in[in_] != '=') && is_base64(in[in_])) {
        char_array_4[i++] = in[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = strchr(base64_chars, char_array_4[i]) - base64_chars;

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3) && (out_idx < max_out_len); i++)
                out[out_idx++] = char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = strchr(base64_chars, char_array_4[j]) - base64_chars;

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1) && (out_idx < max_out_len); j++)
            out[out_idx++] = char_array_3[j];
    }

    return out_idx;
}

bool initManifestFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] Mount Failed");
        return false;
    }
    Serial.println("[LittleFS] Mounted successfully");
    return true;
}

bool loadManifest(ManifestData &manifest) {
    if (!LittleFS.exists("/manifest.json")) {
        Serial.println("[Manifest] /manifest.json not found");
        manifest.has_cached_slots = false;
        return false;
    }

    File file = LittleFS.open("/manifest.json", "r");
    if (!file) {
        Serial.println("[Manifest] Failed to open /manifest.json");
        manifest.has_cached_slots = false;
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[Manifest] Deserialization failed: %s\n", error.c_str());
        manifest.has_cached_slots = false;
        return false;
    }

    manifest.etag = doc["etag"] | "";
    manifest.developer_mode = doc["developer_mode"] | false;
    manifest.sleep_interval_sec = doc["sleep_interval_sec"] | 3600;
    manifest.sync_timestamp = doc["sync_timestamp"] | 0;
    manifest.has_cached_slots = LittleFS.exists("/bw_slot0.raw") && LittleFS.exists("/red_slot0.raw");

    Serial.printf("[Manifest] Loaded ETag: %s | DevMode: %d | Sleep: %us\n",
                  manifest.etag.c_str(), manifest.developer_mode, manifest.sleep_interval_sec);
    return true;
}

bool saveManifest(const String &jsonStr, ManifestData &outManifest) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.printf("[Manifest] Save parse error: %s\n", error.c_str());
        return false;
    }

    outManifest.etag = doc["etag"] | "";
    outManifest.developer_mode = doc["developer_mode"] | false;
    outManifest.sleep_interval_sec = doc["sleep_interval_sec"] | 3600;
    outManifest.sync_timestamp = doc["sync_timestamp"] | 0;

    // Decode and save base64 slot 0 bitmaps if present
    JsonArray slots = doc["slots"];
    if (slots.size() > 0) {
        const char* bw_b64 = slots[0]["bw_b64"];
        const char* red_b64 = slots[0]["red_b64"];
        if (bw_b64 && red_b64) {
            saveBitmapSlotBase64(0, bw_b64, red_b64);
        }
    }

    // Save manifest JSON file
    File file = LittleFS.open("/manifest.json", "w");
    if (!file) {
        Serial.println("[Manifest] Failed to write /manifest.json");
        return false;
    }
    file.print(jsonStr);
    file.close();

    outManifest.has_cached_slots = LittleFS.exists("/bw_slot0.raw") && LittleFS.exists("/red_slot0.raw");
    Serial.println("[Manifest] Successfully saved /manifest.json & bitmap slots");
    return true;
}

bool saveBitmapSlotBase64(int slot_id, const char* bw_b64, const char* red_b64) {
    static uint8_t decode_buf[BITMAP_BUFFER_SIZE];

    // Decode BW
    size_t bw_decoded = base64_decode(bw_b64, strlen(bw_b64), decode_buf, BITMAP_BUFFER_SIZE);
    if (bw_decoded == BITMAP_BUFFER_SIZE) {
        char path[32];
        snprintf(path, sizeof(path), "/bw_slot%d.raw", slot_id);
        File bw_file = LittleFS.open(path, "w");
        if (bw_file) {
            bw_file.write(decode_buf, BITMAP_BUFFER_SIZE);
            bw_file.close();
        }
    }

    // Decode RED
    size_t red_decoded = base64_decode(red_b64, strlen(red_b64), decode_buf, BITMAP_BUFFER_SIZE);
    if (red_decoded == BITMAP_BUFFER_SIZE) {
        char path[32];
        snprintf(path, sizeof(path), "/red_slot%d.raw", slot_id);
        File red_file = LittleFS.open(path, "w");
        if (red_file) {
            red_file.write(decode_buf, BITMAP_BUFFER_SIZE);
            red_file.close();
        }
    }

    return true;
}

bool loadBitmapSlot(int slot_id, uint8_t* bw_buf, uint8_t* red_buf, size_t buf_len) {
    char bw_path[32], red_path[32];
    snprintf(bw_path, sizeof(bw_path), "/bw_slot%d.raw", slot_id);
    snprintf(red_path, sizeof(red_path), "/red_slot%d.raw", slot_id);

    File bw_file = LittleFS.open(bw_path, "r");
    File red_file = LittleFS.open(red_path, "r");

    if (!bw_file || !red_file) {
        Serial.println("[Manifest] Failed to open bitmap slot files");
        if (bw_file) bw_file.close();
        if (red_file) red_file.close();
        return false;
    }

    bw_file.read(bw_buf, buf_len);
    red_file.read(red_buf, buf_len);

    bw_file.close();
    red_file.close();
    return true;
}

#include "manifest_manager.h"
#include "web_server_manager.h"

static size_t base64_decode(const char* in, size_t in_len, uint8_t* out, size_t max_out_len) {
    if (!in || !out) return 0;
    size_t out_idx = 0;
    uint32_t val = 0;
    int valb = -8;
    for (size_t k = 0; k < in_len; k++) {
        unsigned char c = (unsigned char)in[k];
        if (c == '=') break;
        int d = -1;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else continue;

        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            if (out_idx < max_out_len) {
                out[out_idx++] = (uint8_t)((val >> valb) & 0xFF);
            }
            valb -= 8;
        }
    }
    return out_idx;
}

bool initManifestFS() {
    if (!LittleFS.begin(true)) {
        sys_log("[LittleFS] Mount Failed!");
        return false;
    }
    sys_log("[LittleFS] Mounted successfully.");
    return true;
}

bool loadManifest(ManifestData &manifest) {
    if (!LittleFS.exists("/manifest.json")) {
        sys_log("[Manifest] /manifest.json not found");
        manifest.has_cached_slots = false;
        return false;
    }

    File file = LittleFS.open("/manifest.json", "r");
    if (!file) {
        sys_log("[Manifest] Failed to open /manifest.json");
        manifest.has_cached_slots = false;
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        sys_log("[Manifest] Deserialization failed: %s", error.c_str());
        manifest.has_cached_slots = false;
        return false;
    }

    manifest.etag = doc["etag"] | "";
    manifest.developer_mode = doc["developer_mode"] | false;
    manifest.sleep_interval_sec = doc["sleep_interval_sec"] | 3600;
    manifest.sync_timestamp = doc["sync_timestamp"] | 0;
    manifest.has_cached_slots = LittleFS.exists("/bw_slot0.raw") && LittleFS.exists("/red_slot0.raw");

    sys_log("[Manifest] Loaded ETag: %s | DevMode: %d | Sleep: %us | CachedSlots: %d",
               manifest.etag.c_str(), manifest.developer_mode, manifest.sleep_interval_sec, manifest.has_cached_slots);
    return true;
}

bool saveManifest(const String &jsonStr, ManifestData &outManifest) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        sys_log("[Manifest] Save parse error: %s", error.c_str());
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
        } else {
            sys_log("[Manifest] Warning: Slot 0 missing base64 data");
        }
    } else {
        sys_log("[Manifest] Warning: Manifest JSON contains no slots array");
    }

    // Save manifest JSON file
    File file = LittleFS.open("/manifest.json", "w");
    if (!file) {
        sys_log("[Manifest] Failed to write /manifest.json");
        return false;
    }
    file.print(jsonStr);
    file.close();

    outManifest.has_cached_slots = LittleFS.exists("/bw_slot0.raw") && LittleFS.exists("/red_slot0.raw");
    sys_log("[Manifest] Successfully saved /manifest.json (CachedSlots: %d)", outManifest.has_cached_slots);
    return true;
}

bool saveBitmapSlotBase64(int slot_id, const char* bw_b64, const char* red_b64) {
    static uint8_t decode_buf[BITMAP_BUFFER_SIZE];

    // Decode BW
    size_t bw_len = strlen(bw_b64);
    size_t bw_decoded = base64_decode(bw_b64, bw_len, decode_buf, BITMAP_BUFFER_SIZE);
    if (bw_decoded == BITMAP_BUFFER_SIZE) {
        char path[32];
        snprintf(path, sizeof(path), "/bw_slot%d.raw", slot_id);
        File bw_file = LittleFS.open(path, "w");
        if (bw_file) {
            bw_file.write(decode_buf, BITMAP_BUFFER_SIZE);
            bw_file.close();
            sys_log("[Manifest] Wrote %s (%u bytes)", path, BITMAP_BUFFER_SIZE);
        }
    } else {
        sys_log("[Manifest] BW Decode mismatch: read %u base64 chars, expected %u bytes, got %u bytes",
                   (unsigned)bw_len, BITMAP_BUFFER_SIZE, (unsigned)bw_decoded);
    }

    // Decode RED
    size_t red_len = strlen(red_b64);
    size_t red_decoded = base64_decode(red_b64, red_len, decode_buf, BITMAP_BUFFER_SIZE);
    if (red_decoded == BITMAP_BUFFER_SIZE) {
        char path[32];
        snprintf(path, sizeof(path), "/red_slot%d.raw", slot_id);
        File red_file = LittleFS.open(path, "w");
        if (red_file) {
            red_file.write(decode_buf, BITMAP_BUFFER_SIZE);
            red_file.close();
            sys_log("[Manifest] Wrote %s (%u bytes)", path, BITMAP_BUFFER_SIZE);
        }
    } else {
        sys_log("[Manifest] RED Decode mismatch: read %u base64 chars, expected %u bytes, got %u bytes",
                   (unsigned)red_len, BITMAP_BUFFER_SIZE, (unsigned)red_decoded);
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
        sys_log("[Manifest] Failed to open bitmap slot files (%s, %s)", bw_path, red_path);
        if (bw_file) bw_file.close();
        if (red_file) red_file.close();
        return false;
    }

    size_t bw_read = bw_file.read(bw_buf, buf_len);
    size_t red_read = red_file.read(red_buf, buf_len);

    bw_file.close();
    red_file.close();

    if (bw_read != buf_len || red_read != buf_len) {
        sys_log("[Manifest] Read size mismatch: BW=%u, RED=%u (expected %u)", (unsigned)bw_read, (unsigned)red_read, (unsigned)buf_len);
        return false;
    }

    return true;
}

#ifndef MANIFEST_MANAGER_H
#define MANIFEST_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define BITMAP_BUFFER_SIZE 4736 // 296x128 1-bit raw packed buffer size

struct ManifestData {
    String etag;
    bool developer_mode;
    uint32_t sleep_interval_sec;
    uint32_t sync_timestamp;
    bool has_cached_slots;
};

bool initManifestFS();
bool loadManifest(ManifestData &manifest);
bool saveManifest(const String &jsonStr, ManifestData &outManifest);
bool saveBitmapSlotBase64(int slot_id, const char* bw_b64, const char* red_b64);
bool loadBitmapSlot(int slot_id, uint8_t* bw_buf, uint8_t* red_buf, size_t buf_len);

#endif // MANIFEST_MANAGER_H

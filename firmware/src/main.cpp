#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <GxEPD2_3C.h>
#include <esp_sleep.h>

#include "config.h"
#include "manifest_manager.h"

// Waveshare / WeAct 2.9" 3-Color E-Paper Display (296x128)
// Class: GxEPD2_290_C90c
// Pins: CS=1, DC=2, RST=21, BUSY=22, SCK=19, MOSI=18
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
    GxEPD2_290_C90c(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY)
);

// Global RTC memory boot counter preserved across deep sleep resets
RTC_DATA_ATTR static uint32_t boot_counter = 0;

static uint8_t bw_buffer[BITMAP_BUFFER_SIZE];
static uint8_t red_buffer[BITMAP_BUFFER_SIZE];

// Play notification PWM audio chime on GPIO 16
void play_chime() {
    pinMode(BUZZER_PIN, OUTPUT);
    // 2-tone notification melody (1000Hz -> 1500Hz)
    tone(BUZZER_PIN, 1000, 100);
    delay(120);
    tone(BUZZER_PIN, 1500, 150);
    delay(180);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
}

// Battery ADC reader
uint16_t read_battery_adc() {
    analogReadResolution(12);
    return analogRead(BAT_SENSE_PIN);
}

uint8_t calculate_battery_pct(uint16_t adc_val) {
    // Piecewise approximation for 3.7V LiPo voltage divider
    if (adc_val >= 2600) return 100;
    if (adc_val <= 2000) return 0;
    return (uint8_t)((adc_val - 2000) * 100 / 600);
}

// Wi-Fi Connection helper
bool connect_wifi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < WIFI_TIMEOUT_MS) {
        delay(100);
    }
    return (WiFi.status() == WL_CONNECTED);
}

// Mid-Day Delta Checkpoint (Sub-1.5s HEAD check)
// Returns 304 if unchanged, 200 if modified, or -1 on network failure
int check_delta_checkpoint(const String &cached_etag) {
    if (!connect_wifi()) return -1;

    HTTPClient http;
    String endpoint = String(SERVER_URL) + "checkpoint";
    if (endpoint.startsWith("https://your-app-name")) {
        // Fallback for default template
        endpoint = "http://localhost:8000/api/checkpoint";
    }

    http.begin(endpoint);
    if (cached_etag.length() > 0) {
        http.addHeader("If-None-Match", cached_etag);
    }

    uint32_t req_start = millis();
    int httpCode = http.sendRequest("HEAD");
    uint32_t req_dur = millis() - req_start;

    Serial.printf("[Checkpoint] Code: %d | Time: %ums\n", httpCode, req_dur);
    http.end();
    return httpCode;
}

// Full Morning Sync (Bilateral Heavy Exchange)
bool perform_full_sync(ManifestData &manifest) {
    if (!connect_wifi()) return false;

    HTTPClient http;
    uint16_t adc = read_battery_adc();
    uint8_t pct = calculate_battery_pct(adc);

    String sync_url = String(SERVER_URL) + "sync?batt_adc=" + String(adc) + 
                      "&batt_pct=" + String(pct) + 
                      "&reboot_count=" + String(boot_counter);
    if (sync_url.startsWith("https://your-app-name")) {
        sync_url = "http://localhost:8000/api/sync?batt_adc=" + String(adc) + 
                   "&batt_pct=" + String(pct) + 
                   "&reboot_count=" + String(boot_counter);
    }

    http.begin(sync_url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();
        WiFi.disconnect(true);
        return saveManifest(payload, manifest);
    }

    Serial.printf("[Sync] Failed with HTTP code: %d\n", httpCode);
    http.end();
    WiFi.disconnect(true);
    return false;
}

// Render raw 1-bit LittleFS bitmap to Waveshare display
void render_bitmap_from_flash() {
    if (!loadBitmapSlot(0, bw_buffer, red_buffer, BITMAP_BUFFER_SIZE)) {
        Serial.println("[Display] Failed to load bitmap slot from LittleFS");
        return;
    }

    SPI.begin(EINK_SCK, -1, EINK_MOSI, EINK_CS);
    display.init(115200, true, 50, false);
    display.setRotation(1);

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bw_buffer, EINK_WIDTH, EINK_HEIGHT, GxEPD_BLACK);
        display.drawBitmap(0, 0, red_buffer, EINK_WIDTH, EINK_HEIGHT, GxEPD_RED);
    } while (display.nextPage());

    display.hibernate();
    Serial.println("[Display] Render complete, panel hibernating.");
}

void setup() {
    boot_counter++;
    Serial.begin(115200);
    delay(100);

    Serial.printf("\n=== DIY E-Ink Sign v0.2.0 (Boot #%u) ===\n", boot_counter);

    initManifestFS();

    ManifestData manifest;
    bool has_manifest = loadManifest(manifest);

    bool need_full_sync = false;

    if (!has_manifest || !manifest.has_cached_slots) {
        Serial.println("[Strategy] No valid cache found -> Full Morning Sync required.");
        need_full_sync = true;
    } else {
        // Run Sub-1.5s Mid-Day Delta Checkpoint
        Serial.println("[Strategy] Running Mid-Day Delta Checkpoint...");
        int check_code = check_delta_checkpoint(manifest.etag);

        if (check_code == 304) {
            Serial.println("[Strategy] HTTP 304 Not Modified -> Rapid disconnect, rendering local LittleFS cache.");
            WiFi.disconnect(true);
        } else if (check_code == 200 || check_code == -1) {
            Serial.println("[Strategy] ETag modified or initial sync -> Fetching new manifest.");
            need_full_sync = true;
        }
    }

    if (need_full_sync) {
        if (perform_full_sync(manifest)) {
            Serial.println("[Sync] Full sync successful, manifest updated.");
        } else {
            Serial.println("[Sync] Sync failed, using existing cache if available.");
        }
    }

    // Render screen and play notification audio chime
    render_bitmap_from_flash();
    play_chime();

    // Determine deep sleep interval
    uint32_t sleep_sec = manifest.developer_mode ? 120 : (manifest.sleep_interval_sec > 0 ? manifest.sleep_interval_sec : 3600);

    Serial.printf("[Power] Entering deep sleep for %u seconds (DevMode: %d)...\n\n", 
                  sleep_sec, manifest.developer_mode);

    esp_sleep_enable_timer_wakeup(sleep_sec * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Deep sleep prevents reaching loop()
}

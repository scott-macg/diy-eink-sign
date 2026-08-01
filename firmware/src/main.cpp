#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <GxEPD2_3C.h>
#include <esp_sleep.h>

#include "config.h"
#include "config_manager.h"
#include "manifest_manager.h"
#include "web_server_manager.h"

// Waveshare / WeAct 2.9" 3-Color E-Paper Display (296x128)
// Class: GxEPD2_290_C90c
// Pins: CS=1, DC=2, RST=21, BUSY=22, SCK=19, MOSI=18
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
    GxEPD2_290_C90c(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY)
);

// Global RTC memory boot counter preserved across deep sleep resets
RTC_DATA_ATTR static uint32_t boot_counter = 0;
static bool stay_awake = false;

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

float read_battery_volts() {
    uint16_t adc = read_battery_adc();
    return (adc / 4095.0f) * 3.3f * BAT_DIVIDER_RATIO;
}

// Wi-Fi Connection helper
bool connect_wifi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    String ssid = configManager.config.wifi_ssid;
    String pass = configManager.config.wifi_pass;
    uint32_t timeout_ms = configManager.config.wifi_timeout_ms > 0 ? configManager.config.wifi_timeout_ms : WIFI_TIMEOUT_MS;

    if (ssid.length() == 0 || ssid == "YOUR_WIFI_SSID") {
        ssid = WIFI_SSID;
        pass = WIFI_PASS;
    }

    sys_log("[WiFi] Connecting to SSID: '%s'...", ssid.c_str());
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < timeout_ms) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        sys_log("[WiFi] Connected! IP: %s (RSSI: %d dBm)", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    } else {
        sys_log("[WiFi] Connection timed out.");
        return false;
    }
}

// Mid-Day Delta Checkpoint (Sub-1.5s HEAD check)
// Returns 304 if unchanged, 200 if modified, or -1 on network failure
int check_delta_checkpoint(const String &cached_etag) {
    if (!connect_wifi()) return -1;

    HTTPClient http;
    String base_url = configManager.config.server_url;
    if (base_url.length() == 0 || base_url.startsWith("https://your-app-name")) {
        base_url = "http://localhost:8000/api/";
    }
    if (!base_url.endsWith("/")) base_url += "/";

    String endpoint = base_url + "checkpoint";

    http.begin(endpoint);
    if (cached_etag.length() > 0) {
        http.addHeader("If-None-Match", cached_etag);
    }

    uint32_t req_start = millis();
    int httpCode = http.sendRequest("HEAD");
    uint32_t req_dur = millis() - req_start;

    sys_log("[Checkpoint] Endpoint: %s | Code: %d | Time: %ums", endpoint.c_str(), httpCode, req_dur);
    http.end();
    return httpCode;
}

// Full Morning Sync (Bilateral Heavy Exchange)
bool perform_full_sync(ManifestData &manifest) {
    if (!connect_wifi()) return false;

    HTTPClient http;
    uint16_t adc = read_battery_adc();
    uint8_t pct = calculate_battery_pct(adc);

    String base_url = configManager.config.server_url;
    if (base_url.length() == 0 || base_url.startsWith("https://your-app-name")) {
        base_url = "http://localhost:8000/api/";
    }
    if (!base_url.endsWith("/")) base_url += "/";

    String sync_url = base_url + "sync?batt_adc=" + String(adc) + 
                      "&batt_pct=" + String(pct) + 
                      "&reboot_count=" + String(boot_counter);

    sys_log("[Sync] Requesting GET %s", sync_url.c_str());
    http.begin(sync_url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        sys_log("[Sync] HTTP 200 OK received (%u bytes)", payload.length());
        http.end();
        return saveManifest(payload, manifest);
    }

    sys_log("[Sync] Failed with HTTP code: %d", httpCode);
    http.end();
    return false;
}

void render_fallback_card() {
    sys_log("[Display] Rendering fallback status card...");
    display.init(115200, true, 50, false);
    SPI.end();
    SPI.begin(EINK_SCK, -1, EINK_MOSI, EINK_CS);
    display.setRotation(3);

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawRect(2, 2, EINK_WIDTH - 4, EINK_HEIGHT - 4, GxEPD_BLACK);
        display.drawRect(4, 4, EINK_WIDTH - 8, EINK_HEIGHT - 8, GxEPD_RED);
        display.setCursor(15, 25);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(2);
        display.print("DIY E-Ink Sign");
        display.setCursor(15, 55);
        display.setTextSize(1);
        display.setTextColor(GxEPD_RED);
        display.print("Status: Hardware Active");
        display.setCursor(15, 75);
        display.setTextColor(GxEPD_BLACK);
        display.printf("IP: %s", WiFi.localIP().toString().c_str());
        display.setCursor(15, 95);
        display.printf("Bat: %.2fV (%d%%)", read_battery_volts(), calculate_battery_pct(read_battery_adc()));
    } while (display.nextPage());

    display.hibernate();
    sys_log("[Display] Fallback card render complete.");
}

// Render raw 1-bit LittleFS bitmap to Waveshare display
void render_bitmap_from_flash() {
    if (!loadBitmapSlot(0, bw_buffer, red_buffer, BITMAP_BUFFER_SIZE)) {
        sys_log("[Display] Slot 0 missing from LittleFS. Rendering fallback card...");
        render_fallback_card();
        return;
    }

    sys_log("[Display] Initializing E-Paper display SPI...");
    display.init(115200, true, 50, false);
    SPI.end();
    SPI.begin(EINK_SCK, -1, EINK_MOSI, EINK_CS);
    display.setRotation(3); // 180-degree inverted landscape for physical enclosure mounting

    sys_log("[Display] Flushing bitmap buffers to Waveshare 2.9\" panel...");
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bw_buffer, EINK_WIDTH, EINK_HEIGHT, GxEPD_BLACK);
        display.drawBitmap(0, 0, red_buffer, EINK_WIDTH, EINK_HEIGHT, GxEPD_RED);
    } while (display.nextPage());

    display.hibernate();
    sys_log("[Display] Render complete! Panel hibernating.");
}

void setup() {
    boot_counter++;
    Serial.begin(115200);
    delay(100);

    // Initialize hardware GPIO pin modes explicitly for ESP32 Arduino v3 compatibility
    pinMode(EINK_CS, OUTPUT);
    pinMode(EINK_DC, OUTPUT);
    pinMode(EINK_RST, OUTPUT);
    pinMode(EINK_BUSY, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(EINK_CS, HIGH);
    digitalWrite(EINK_DC, HIGH);
    digitalWrite(EINK_RST, HIGH);

    sys_log("\n=== DIY E-Ink Sign v0.2.0 (Boot #%u) ===", boot_counter);

    initManifestFS();
    configManager.begin();

    ManifestData manifest;
    bool has_manifest = loadManifest(manifest);

    bool need_full_sync = false;

    if (!has_manifest || !manifest.has_cached_slots) {
        sys_log("[Strategy] No valid cache found -> Full Morning Sync required.");
        need_full_sync = true;
    } else {
        // Run Sub-1.5s Mid-Day Delta Checkpoint
        sys_log("[Strategy] Running Mid-Day Delta Checkpoint...");
        int check_code = check_delta_checkpoint(manifest.etag);

        if (check_code == 304) {
            sys_log("[Strategy] HTTP 304 Not Modified -> Rapid disconnect, rendering local LittleFS cache.");
        } else if (check_code == 200 || check_code == -1) {
            sys_log("[Strategy] ETag modified or initial sync -> Fetching new manifest.");
            need_full_sync = true;
        }
    }

    if (need_full_sync) {
        if (perform_full_sync(manifest)) {
            sys_log("[Sync] Full sync successful, manifest updated.");
        } else {
            sys_log("[Sync] Sync failed, using existing cache if available.");
        }
    }

    // Render screen and play notification audio chime
    render_bitmap_from_flash();
    play_chime();

    stay_awake = true;
    connect_wifi();
    webServerManager.begin(
        render_bitmap_from_flash,
        read_battery_volts,
        []() -> int { return (int)calculate_battery_pct(read_battery_adc()); },
        [](uint16_t freq, uint16_t dur) {
            pinMode(BUZZER_PIN, OUTPUT);
            tone(BUZZER_PIN, freq, dur);
            delay(dur + 20);
            noTone(BUZZER_PIN);
        }
    );
    sys_log("[Power] Deep sleep DISABLED (dev build) -> Staying awake with Web Console active.");
}

void loop() {
    if (stay_awake) {
        webServerManager.handleClient();
        delay(1);
    }
}

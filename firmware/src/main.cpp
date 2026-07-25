#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SPI.h>
#include <GxEPD2_3C.h>
#include <PNGdec.h>
#include "config.h"
#include "config_manager.h"
#include "web_server_manager.h"
#include "battery_curve.h"

// Initialize display driver for WeAct 2.9" 3-color (296x128)
GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(
    GxEPD2_290c(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY)
);

PNG png;
Preferences preferences;

// Audio Feedback Helper
void playBuzzerTone(uint16_t freq, uint16_t durationMs) {
    pinMode(BUZZER_PIN, OUTPUT);
    tone(BUZZER_PIN, freq, durationMs);
    delay(durationMs + 20);
    noTone(BUZZER_PIN);
}

void playSoundBootWake() {
    playBuzzerTone(1000, 80);
    delay(40);
    playBuzzerTone(1500, 80);
}

void playSoundSuccess() {
    playBuzzerTone(1200, 100);
    delay(50);
    playBuzzerTone(1800, 150);
}

void playSoundError() {
    playBuzzerTone(400, 300);
}

void playSoundLowBattery() {
    playBuzzerTone(400, 150);
    delay(100);
    playBuzzerTone(300, 250);
}

// Battery Sensing Helpers (16-sample averaged reading for voltage stability)
float readBatteryVoltage() {
    uint32_t totalMv = 0;
    const int numSamples = 16;
    for (int i = 0; i < numSamples; i++) {
        totalMv += analogReadMilliVolts(BAT_SENSE_PIN);
        delayMicroseconds(100);
    }
    float avgMv = static_cast<float>(totalMv) / numSamples;
    return (avgMv * BAT_DIVIDER_RATIO) / 1000.0f;
}

int readBatteryPercent() {
    float vbat = readBatteryVoltage();
    return batteryCurveManager.getPercent(vbat);
}

// Callback for PNGdec line renderer
int pngDrawCallback(PNGDRAW *pDraw) {
    uint16_t usPixels[300];
    png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0x0000);

    for (int x = 0; x < pDraw->iWidth; x++) {
        uint16_t rgb = usPixels[x];
        uint8_t r = (rgb >> 11) & 0x1F;
        uint8_t g = (rgb >> 5) & 0x3F;
        uint8_t b = rgb & 0x1F;

        // Map RGB565 to 3-Color E-Paper Palette (White, Black, Red)
        uint16_t color = GxEPD_WHITE;
        if (r > 15 && g < 15 && b < 15) {
            color = GxEPD_RED;
        } else if (r < 10 && g < 10 && b < 10) {
            color = GxEPD_BLACK;
        }

        display.drawPixel(x, pDraw->y, color);
    }
    return 1;
}

// Fast B/W PNGdec line renderer (ignores red channel for 1.5s expedited cycling)
int pngDrawCallbackFastBW(PNGDRAW *pDraw) {
    uint16_t usPixels[300];
    png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0x0000);

    for (int x = 0; x < pDraw->iWidth; x++) {
        uint16_t rgb = usPixels[x];
        uint8_t r = (rgb >> 11) & 0x1F;
        uint8_t g = (rgb >> 5) & 0x3F;
        uint8_t b = rgb & 0x1F;

        // Map to Black vs White only (Fast B/W)
        uint16_t color = GxEPD_WHITE;
        if (r < 15 && g < 15 && b < 15) {
            color = GxEPD_BLACK;
        }

        display.drawPixel(x, pDraw->y, color);
    }
    return 1;
}

// Extract max-age integer from Cache-Control header string
uint32_t parseCacheControlMaxAge(const String& header) {
    int idx = header.indexOf("max-age=");
    if (idx != -1) {
        String valStr = header.substring(idx + 8);
        int endIdx = valStr.indexOf(',');
        if (endIdx != -1) valStr = valStr.substring(0, endIdx);
        uint32_t val = valStr.toInt();
        if (val >= MIN_SLEEP_SEC) return val;
    }
    return configManager.config.default_sleep_sec;
}

// Helper for manual trigger from REPL or Server command
void triggerDisplayRefresh() {
    Serial.println("[DISPLAY] Manual refresh triggered from Web REPL.");
    playSoundBootWake();
}

bool isMaintenanceMode = false;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== DIY E-Ink Smart Sign starting up ===");

    // Initialize LittleFS & Load Configuration
    if (!configManager.begin()) {
        Serial.println("[MAIN] ConfigManager initialization failed!");
    }
    batteryCurveManager.begin();

    // Configure Boot / Force-Refresh Switch pin and Analog Battery Sense
    pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
    pinMode(BAT_SENSE_PIN, INPUT);
    analogReadResolution(12);

    // Read Battery Level
    float vbat = readBatteryVoltage();
    int battPct = readBatteryPercent();
    Serial.printf("[BATT] Voltage: %.2fV (%d%%)\n", vbat, battPct);

    if (battPct <= CRITICAL_BATTERY_PERCENT_THRESHOLD) {
        Serial.println("[BATT] WARNING: Battery critically low!");
        if (configManager.config.audio_battery_alert) {
            playSoundLowBattery();
        } else {
            Serial.println("[BATT] Audio battery alert is disabled in configuration.");
        }
    }

    // Check top microswitch (GPIO 9) press type & wakeup reason
    bool isUserCycleAction = false;
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (configManager.config.developer_mode) {
        Serial.println("[MODE] Persistent Developer Mode enabled in config.json! Deep sleep disabled.");
        isMaintenanceMode = true;
    } else if (digitalRead(BOOT_BTN_PIN) == LOW || wakeup_reason == ESP_SLEEP_WAKEUP_GPIO || wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        // Debounce / Hold duration check (400ms)
        delay(400);
        if (digitalRead(BOOT_BTN_PIN) == LOW) {
            Serial.println("[MODE] Maintenance Mode requested via HELD top BOOT switch!");
            isMaintenanceMode = true;
            playSoundBootWake();
        } else {
            Serial.println("[MODE] User pressed top BOOT switch to cycle screen!");
            isUserCycleAction = true;
            playSoundBootWake();
        }
    } else {
        Serial.println("[MODE] Operating in Normal Low-Power Mode.");
    }


    // Connect to Wi-Fi using dynamic config
    Serial.print("[WIFI] Connecting to ");
    Serial.println(configManager.config.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(configManager.config.wifi_ssid.c_str(), configManager.config.wifi_pass.c_str());

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < configManager.config.wifi_timeout_ms) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Failed to connect to Wi-Fi!");
        playSoundError();
        return;
    }

    Serial.print("[WIFI] Connected! IP: ");
    Serial.println(WiFi.localIP());

    if (isMaintenanceMode) {
        // Start Web Server & WebSockets REPL Console
        webServerManager.begin(triggerDisplayRefresh, readBatteryVoltage, readBatteryPercent, playBuzzerTone);
        Serial.println("[MAINTENANCE] Maintenance Web Console active. Open http://" + WiFi.localIP().toString() + " in browser.");
        return; // Stay awake in loop()
    }

    // =========================================================================
    // Normal Low-Power Mode Execution Path
    // =========================================================================
    preferences.begin("epaper", false);
    String storedETag = preferences.getString("etag", "");

    HTTPClient http;
    WiFiClientSecure secureClient;
    String url = configManager.config.server_url;

    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, url);
    } else {
        http.begin(url);
    }
    const char * headers[] = {"Cache-Control", "ETag"};
    http.collectHeaders(headers, 2);

    http.addHeader("X-Display-ID", configManager.config.display_token);
    http.addHeader("X-Battery-Voltage", String(vbat, 2));
    http.addHeader("X-Battery-Percent", String(battPct));

    if (isUserCycleAction) {
        http.addHeader("X-Display-Action", "cycle");
        Serial.println("[HTTP] Manual top button press detected. Sending X-Display-Action: cycle (bypassing 304 cache).");
    } else if (storedETag.length() > 0) {
        http.addHeader("ETag", storedETag);
        http.addHeader("If-None-Match", storedETag);
        Serial.print("[HTTP] Sending stored ETag: ");
        Serial.println(storedETag);
    }

    int httpCode = http.GET();
    Serial.printf("[HTTP] GET status code: %d\n", httpCode);

    uint32_t sleepDurationSec = configManager.config.default_sleep_sec;
    if (http.hasHeader("Cache-Control")) {
        sleepDurationSec = parseCacheControlMaxAge(http.header("Cache-Control"));
    }

    if (httpCode == 200) {
        Serial.println("[HTTP] New image data received! Updating display...");
        String newETag = http.header("ETag");
        if (newETag.length() > 0) {
            preferences.putString("etag", newETag);
            Serial.print("[HTTP] Updated stored ETag: ");
            Serial.println(newETag);
        }

        int len = http.getSize();
        if (len > 0) {
            uint8_t *buffer = (uint8_t *)malloc(len);
            if (buffer != NULL) {
                WiFiClient *stream = http.getStreamPtr();
                int bytesRead = 0;
                while (http.connected() && (bytesRead < len)) {
                    size_t sizeAvail = stream->available();
                    if (sizeAvail) {
                        int c = stream->readBytes(buffer + bytesRead, sizeAvail);
                        bytesRead += c;
                    }
                    delay(1);
                }

                // Initialize SPI and Display
                SPI.begin(EINK_SCK, -1, EINK_MOSI, EINK_CS);
                display.setRotation(1); // Landscape mode

                uint8_t mode = configManager.config.refresh_mode;

                // Determine active refresh scheme
                bool useFastBW = (isUserCycleAction && (mode == REFRESH_MODE_TWO_PASS || mode == REFRESH_MODE_FAST_BW)) || (mode == REFRESH_MODE_FAST_BW);
                bool usePartialWindow = (mode == REFRESH_MODE_PARTIAL);

                if (useFastBW) {
                    Serial.println("[DISPLAY] Scheme: Fast B/W Partial Refresh (~1.5s)...");
                    display.init(115200, true, 20, false);
                    display.setPartialWindow(0, 0, EINK_WIDTH, EINK_HEIGHT);
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        int rc = png.openRAM(buffer, len, pngDrawCallbackFastBW);
                        if (rc == PNG_SUCCESS) {
                            png.decode(NULL, 0);
                            png.close();
                        }
                    } while (display.nextPage());
                } else if (usePartialWindow) {
                    Serial.println("[DISPLAY] Scheme: Partial Window Quote Update (~5s)...");
                    display.init(115200, true, 50, false);
                    display.setPartialWindow(0, 40, EINK_WIDTH, 88);
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        int rc = png.openRAM(buffer, len, pngDrawCallback);
                        if (rc == PNG_SUCCESS) {
                            png.decode(NULL, 0);
                            png.close();
                        }
                    } while (display.nextPage());
                } else {
                    Serial.println("[DISPLAY] Scheme: Full 3-Color Clean Refresh (~14s)...");
                    display.init(115200, true, 50, false);
                    display.setFullWindow();
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        int rc = png.openRAM(buffer, len, pngDrawCallback);
                        if (rc == PNG_SUCCESS) {
                            png.decode(NULL, 0);
                            png.close();
                        }
                    } while (display.nextPage());
                }

                display.powerOff();
                playSoundSuccess();

                // Two-Pass Mode: If fast preview rendered on button tap, run delayed Pass 2 (Full 3-Color Clean Finish)
                if (isUserCycleAction && mode == REFRESH_MODE_TWO_PASS) {
                    Serial.println("[DISPLAY] Two-Pass Mode: Fast preview active. Pausing 10s before full 3-color clean pass...");
                    delay(10000);
                    Serial.println("[DISPLAY] Executing Two-Pass Pass 2: Full 3-Color Clean Finish...");
                    display.init(115200, true, 50, false);
                    display.setFullWindow();
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        int rc = png.openRAM(buffer, len, pngDrawCallback);
                        if (rc == PNG_SUCCESS) {
                            png.decode(NULL, 0);
                            png.close();
                        }
                    } while (display.nextPage());
                    display.powerOff();
                    playSoundSuccess(); // Pass 2 completion chime
                }

                free(buffer);
            } else {
                Serial.println("[ERROR] Failed to allocate memory for PNG buffer!");
                playSoundError();
            }
        }
    } else if (httpCode == 304) {
        Serial.println("[HTTP] 304 Not Modified. Canvas untouched, preserving battery.");
    } else {
        Serial.printf("[HTTP] Unexpected response code: %d\n", httpCode);
        playSoundError();
    }

    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    preferences.end();

    if (configManager.config.developer_mode) {
        Serial.println("[POWER] Persistent Developer Mode enabled in config.json - deep sleep bypassed.");
    } else {
        Serial.printf("[POWER] Entering deep sleep for %u seconds...\n", sleepDurationSec);
        esp_deep_sleep_enable_gpio_wakeup(1ULL << BOOT_BTN_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        esp_sleep_enable_timer_wakeup((uint64_t)sleepDurationSec * 1000000ULL);
        esp_deep_sleep_start();
    }
}

void loop() {
    if (isMaintenanceMode) {
        webServerManager.handleClient();
    }
    delay(10);
}

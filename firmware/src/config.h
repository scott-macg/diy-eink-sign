#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// Hardware Pinout Definitions for Seeed Studio XIAO ESP32-C6
// ============================================================================

// SPI Pins for WeAct 2.9" E-Paper Display
#define EINK_MOSI        18  // Xiao D10
#define EINK_SCK         19  // Xiao D8
#define EINK_CS           1  // Xiao D1
#define EINK_DC           2  // Xiao D2
#define EINK_RST         21  // Xiao D3
#define EINK_BUSY        22  // Xiao D4

// Peripherals
#define BOOT_BTN_PIN      9  // Xiao D9 / BOOT pin (External Boot & Force-Refresh Switch)
#define BUZZER_PIN       16  // Xiao D6 (Piezo Buzzer Audio Alert)

// Note: External Reset Switch is wired directly between RST (CHIP_PU) and GND.

// ============================================================================
// Display Driver Specifications
// ============================================================================
#define EINK_WIDTH      296
#define EINK_HEIGHT     128

// ============================================================================
// Network & Server Configuration
// ============================================================================
#define WIFI_SSID       "curlmacg"
#define WIFI_PASS       "1qaz2wsx3edc4rfv"

// Python backend endpoint and token
#define SERVER_URL      "https://esp32-epaper-display.onrender.com/get/"
#define DISPLAY_TOKEN   "sign_token_123"
#define WIFI_TIMEOUT_MS 15000

// ============================================================================
// Power Management Parameters
// ============================================================================
#define DEFAULT_SLEEP_SEC 3600  // Default 1 hour sleep if no Cache-Control header
#define MIN_SLEEP_SEC     60    // Minimum sleep duration (1 minute)

#endif // CONFIG_H

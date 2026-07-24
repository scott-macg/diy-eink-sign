#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

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
#define BAT_SENSE_PIN     0  // Xiao D0 / GPIO 0 (Analog Battery Sense via 2x 100k resistor divider)

// Note: External Reset Switch is wired directly between RST (CHIP_PU) and GND.

// ============================================================================
// Display Driver Specifications
// ============================================================================
#define EINK_WIDTH      296
#define EINK_HEIGHT     128

// ============================================================================
// Network & Server Configuration
// Copy this file to config.h and fill in your actual Wi-Fi and server settings.
// ============================================================================
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASS       "YOUR_WIFI_PASSWORD"

// Python backend endpoint and token
#define SERVER_URL      "https://your-app-name.onrender.com/get/"
#define DISPLAY_TOKEN   "YOUR_SECRET_TOKEN"
#define WIFI_TIMEOUT_MS 15000

// ============================================================================
// Power Management & Battery Sensing Parameters
// ============================================================================
#define DEFAULT_SLEEP_SEC 3600  // Default 1 hour sleep if no Cache-Control header
#define MIN_SLEEP_SEC     60    // Minimum sleep duration (1 minute)

#define BAT_DIVIDER_RATIO                 2.0f // 2x 100k ohm resistor divider (V_bat = V_adc * 2)
#define LOW_BATTERY_PERCENT_THRESHOLD     20   // Low battery indicator threshold (%)
#define CRITICAL_BATTERY_PERCENT_THRESHOLD 10   // Critical battery warning beep threshold (%)

#endif // CONFIG_EXAMPLE_H

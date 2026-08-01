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

// Peripherals & Microswitches
#define SW_LEFT_PIN      17  // Xiao D7 / GPIO 17 (Left Switch: Prev Slot / Long Press Status Card)
#define SW_RIGHT_PIN     20  // Xiao D9 / GPIO 20 (Right Switch: Next Slot / Long Press Force Sync)
#define BUZZER_PIN       16  // Xiao D6 (Piezo Buzzer Audio Alert)
#define BAT_SENSE_PIN     0  // Xiao D0 / GPIO 0 (Analog Battery Sense via 2x 100k resistor divider)

// Note for future hardware revisions:
// #define HARDWARE_RESET_BTN_PIN  CHIP_PU // Optional External Hard Reset Switch between CHIP_PU (EN) and GND


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

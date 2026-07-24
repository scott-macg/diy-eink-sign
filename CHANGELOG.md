# Changelog & Update History (`CHANGELOG.md`)

All notable changes to the DIY 2.9" E-Ink Smart Sign project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased] - 2026-07-24

### Added
- **Hardware & Sensing:**
  - Integrated 902540 3.7V 800mAh 2.96Wh 25C LiPo battery specification into system design.
  - Added 2x 100kΩ voltage divider network on pin `D0` (`GPIO 0` / `ADC1_CH0`) to scale 4.2V max battery voltage down to 2.1V for safe 12-bit ADC reading.
  - Documented step-by-step LiPo battery direct-soldering safety guidelines (wire stripping sequence, polarity check, strain relief) in `breadboard_wiring.md`.
  - Added NPN transistor (2N2222/2N3904) dynamic speaker driver circuit on `D6` (`GPIO 16`) for 8kHz 8-bit PCM audio notifications.

- **Firmware (`/firmware`):**
  - Added ADC battery voltage sensing (`readBatteryVoltage()`, `readBatteryPercent()`) using ESP32 `analogReadMilliVolts()`.
  - Added HTTP GET telemetry headers (`X-Battery-Voltage`, `X-Battery-Percent`) to send battery stats to the server during updates.
  - Added `playSoundLowBattery()` audio alert chirp when battery drops to $\le 10\%$.
  - Integrated `GxEPD2_3C` display driver for WeAct Studio 2.9" 3-Color E-Paper module (`GxEPD2_290c`).
  - Added GPIO 9 (`D9`) boot switch interrupt for manual force-refresh / maintenance mode wake-up.
  - Added `ETag` and `Cache-Control: max-age` sleep duration parsing to optimize deep sleep cycles.

- **Backend Server (`/server/epaper-server`):**
  - Created `epaperengine/battery.py` module to dynamically overlay a crisp 3-color (Black frame, White margin padding, Red fill) battery icon in the lower right-hand corner (`x=268, y=112`) when `X-Battery-Percent` $\le 20\%$.
  - Implemented `epaperengine/widgets/quotes.py` custom widget with local fallback pool, word-wrap engine, and 3-color palette styling.
  - Added `credentials.json.example` and `config/README.md` for Google Calendar OAuth integration.
  - Optimized deployment for Render.com PaaS by stubbing unconfigured external APIs (Google Maps, Calendar, Weather) to prevent startup crashes.

### Changed
- Updated `plan.md` and `breadboard_wiring.md` to reflect hardware prototyping validations, battery pinout mapping, and component lists.

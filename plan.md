# DIY 2.9" 3-Color E-Ink Smart Sign (`plan.md`)

## 1. Executive Summary
A local-first, low-power smart desk plaque using a Seeed Studio XIAO ESP32-C6 and a WeAct Studio 2.9" 3-Color (Black/White/Red or Yellow) e-paper display. The microcontroller wakes up on a schedule, queries a lightweight Python server for a pre-rendered 296x128 bitmap, flushes the image via SPI using `GxEPD2_3C`, and enters deep sleep.

---

## 2. Hardware Architecture & Pinout Map

> [!NOTE]
> For step-by-step breadboard assembly diagrams and perf-board layout planning (30x70mm grid), see [breadboard_wiring.md](file:///home/smacd/diy-eink-sign/breadboard_wiring.md).
> For a chronological log of all hardware, firmware, and server updates, see [CHANGELOG.md](file:///home/smacd/diy-eink-sign/CHANGELOG.md).

### Core Components
- **MCU:** Seeed Studio XIAO ESP32-C6 (3.3V logic, integrated LiPo charging, Wi-Fi 6).
- **Display:** WeAct Studio 2.9" 3-Color E-Paper Module (296x128 pixels, SPI interface).
- **Battery:** 902540 3.7V 800mAh 2.96Wh 25C High-Discharge LiPo cell (integrated PCM, direct solder to `BAT+`/`BAT-` pads).
- **Peripherals:** 2x Micro switches (External Reset & External Boot / Force-Refresh), 20mm 8Ω Dynamic Speaker with NPN Transistor Driver (5V rail, 1kΩ base resistor, flyback diode), 2x 100kΩ Voltage Divider on Analog Battery Sense (`D0`).
- **Enclosure:** 3D-printed housing + laser-cut/custom plexiglass protective window.

### Pin Wiring Table
| Peripherals / Function | WeAct Pin | XIAO ESP32-C6 Silk Pin | GPIO Number | Signal Type |
| :--- | :--- | :--- | :--- | :--- |
| **SPI MOSI** | DIN | `D10` | GPIO 18 | Output (SPI Data) |
| **SPI SCK** | CLK | `D8` | GPIO 19 | Output (SPI Clock) |
| **Chip Select** | CS | `D1` | GPIO 1 | Output |
| **Data / Command** | DC | `D2` | GPIO 2 | Output |
| **Reset** | RST | `D3` | GPIO 21 | Output |
| **Busy Signal** | BUSY | `D4` | GPIO 22 | Input |
| **External Reset Switch**| - | `RST` | CHIP_PU / RST | Input (Pull to GND for hardware reset) |
| **Boot / Force-Refresh Switch**| - | `D9` | GPIO 9 | Input (`INPUT_PULLUP` / Boot mode) |
| **Speaker (NPN Driver)** | - | `D6` | GPIO 16 | Output (PWM / 8-bit 8kHz PCM Audio) |
| **Battery Sense Divider** | - | `D0` | GPIO 0 | Input (Analog ADC1_CH0, 2x 100kΩ divider) |
| **Power (3.3V)** | VCC | `3V3` | - | 3.3V Power Rail |
| **Ground** | GND | `GND` | - | Common Ground |

---

## 3. Software & Backend Architecture

### Server Layer (`/server`)
- **Base Repo:** Forked from `ugomeda/esp32-epaper-display`.
- **Stack:** Python 3, AIOHTTP, Pillow (PIL) image canvas generation.
- **Widgets:**
  1. `googlecalendar`: OAuth / Service account integration pulling daily events.
  2. `inspirational_quotes`: Custom Python module pulling quotes from API or local JSON array.
  3. `custom_push`: Emergency or prioritized messages passed via HTTP API or web form.
- **Caching, Battery & Telemetry Strategy:**
  - Emits `ETag` hashes matching canvas state.
  - Returns `304 Not Modified` if data has not changed (preventing display refresh wear).
  - Supplies `Cache-Control: max-age=...` header to instruct the ESP32-C6 exactly how long to sleep.
  - Receives `X-Battery-Voltage` and `X-Battery-Percent` telemetry headers from ESP32-C6.
  - Overlays a 3-color (Black/White/Red) low battery icon (`epaperengine/battery.py`) in the bottom-right corner when `X-Battery-Percent` $\le 20\%$.

### Firmware Layer (`/firmware`)
- **Framework:** PlatformIO / Arduino Core for ESP32-C6.
- **Display Driver:** `GxEPD2` / `GxEPD2_3C` tailored to the 2.9" WeAct driver IC (`GxEPD2_290c`).
- **Power & Battery Lifecycle:**
  1. Boot up -> Measure battery voltage on ADC pin `D0` (`readBatteryVoltage()`, `readBatteryPercent()`).
  2. If battery $\le 10\%$, play low-pitch audio warning chirp (`playSoundLowBattery()`).
  3. Check wake-up source (Timer vs GPIO 9 Boot Switch).
  4. Connect to Wi-Fi.
  5. Send HTTP GET request with stored `ETag` and telemetry headers (`X-Battery-Voltage`, `X-Battery-Percent`).
  6. If `HTTP 200`: Read stream, push bitmap via `GxEPD2_3C`, save new `ETag`.
  7. If `HTTP 304`: Skip display refresh, keep current screen.
  8. Disconnect Wi-Fi and enter `esp_deep_sleep()`.

---

## 4. Implementation Roadmap for Antigravity IDE

### Phase 1: Server Customization
- [x] Install dependencies in `/server/epaper-server/requirements.txt` into Python venv (`/server/epaper-server/venv`).
- [x] Configure `config.json` for 2.9" display (`296x128` canvas resolution) and 3-color palette mapping (Black `#000000`, White `#FFFFFF`, Red `#FF0000`).
- [x] Fixed Pillow 10+ compatibility (`textbbox`) in `helper.py`/`date.py` and added graceful missing-credential handling in `googlecalendar.py`. Verified 296x128 image rendering (`test_output.png`).
- [x] Created `config/credentials.json.example` and `config/README.md` for Google Calendar OAuth setup.
- [x] Implemented `quotes` widget module (`epaperengine/widgets/quotes.py`) with dynamic quote fetching, local fallback pool, auto text-wrapping, and 3-color styling.
- [x] **Render.com Deployment Optimization:** Commented out external API network calls (`googlemaps.py`, `googlecalendar.py`, `weather.py`) and updated `config.example.json` to prevent headless startup crashes during deployment.
- [x] Implemented server-side 3-color low battery icon overlay (`epaperengine/battery.py`) triggered by `X-Battery-Percent` $\le 20\%$.
- [ ] **Future Circle-Back (Optional API Re-enabling):**
  - [ ] **Google Maps:** Supply `client_key` in `config.json` and uncomment `googlemaps.Client` & `_fetch_map` in `googlemaps.py`.
  - [ ] **Google Calendar:** Mount `credentials.json`/`token.pickle` and uncomment OAuth/calendar list fetching in `googlecalendar.py`.
  - [ ] **Weather:** Supply OpenWeatherMap `api_key` & `city_id` in `config.json` and uncomment `requests.get` in `weather.py`.


### Phase 2: ESP32-C6 Firmware Development
- [x] Create `platformio.ini` targeting `seeed_xiao_esp32c6`.
- [x] Configure `GxEPD2_3C` constructor with GPIO 18 (MOSI), GPIO 19 (SCK), GPIO 1 (CS), GPIO 2 (DC), GPIO 21 (RST), and GPIO 22 (BUSY).
- [x] Implement HTTP client handling `ETag` headers and deep sleep duration parameters.
- [x] Add GPIO 9 Boot switch interrupt wake-up logic for manual force-refresh button.
- [x] Implement ADC battery voltage sensing on `GPIO 0` (`D0`) with 2x 100kΩ divider, HTTP telemetry headers (`X-Battery-Voltage`, `X-Battery-Percent`), and audio low-battery alert.
- [ ] **Future Feature (On-Device Rendering & Offline Screen Caching):**
  - [ ] Store fallback bitmap screens/templates in LittleFS flash memory for offline display when Wi-Fi connection fails or server is unreachable.
  - [ ] Render basic local canvas graphics (clock, offline status badge, local alerts) directly on-device using `Adafruit_GFX` / `GxEPD2` primitives without server roundtrips.

### Phase 3: Hardware Assembly & Enclosure
- [x] Prototype hardware connections on breadboard (MCU, display SPI, switches, 20mm speaker NPN driver, battery sense divider) powered via USB.
- [ ] Transition to perf-board & solder harness once component configuration is validated.
- [ ] Direct-solder 902540 800mAh 3.7V LiPo battery to `BAT+`/`BAT-` pads on XIAO ESP32-C6 (cutting/soldering one wire at a time for safety).
- [ ] Print 3D bezel/case and assemble plexiglass protective front window.
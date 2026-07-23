# DIY 2.9" 3-Color E-Ink Smart Sign (`plan.md`)

## 1. Executive Summary
A local-first, low-power smart desk plaque using a Seeed Studio XIAO ESP32-C6 and a WeAct Studio 2.9" 3-Color (Black/White/Red or Yellow) e-paper display. The microcontroller wakes up on a schedule, queries a lightweight Python server for a pre-rendered 296x128 bitmap, flushes the image via SPI using `GxEPD2_3C`, and enters deep sleep.

---

## 2. Hardware Architecture & Pinout Map

> [!NOTE]
> For step-by-step breadboard assembly diagrams and perf-board layout planning (30x70mm grid), see [breadboard_wiring.md](file:///home/smacd/diy-eink-sign/breadboard_wiring.md).

### Core Components
- **MCU:** Seeed Studio XIAO ESP32-C6 (3.3V logic, integrated LiPo charging, Wi-Fi 6).
- **Display:** WeAct Studio 2.9" 3-Color E-Paper Module (296x128 pixels, SPI interface).
- **Peripherals:** 2x Micro switches (External Reset & External Boot / Force-Refresh), Piezo buzzer (audio alert), Li-ion battery.
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
| **Piezo Audio Alert** | - | `D6` | GPIO 16 | Output (PWM / Tone) |
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
- **Caching & Battery Strategy:**
  - Emits `ETag` hashes matching canvas state.
  - Returns `304 Not Modified` if data has not changed (preventing display refresh wear).
  - Supplies `Cache-Control: max-age=...` header to instruct the ESP32-C6 exactly how long to sleep.

### Firmware Layer (`/firmware`)
- **Framework:** PlatformIO / Arduino Core for ESP32-C6.
- **Display Driver:** `GxEPD2` / `GxEPD2_3C` tailored to the 2.9" WeAct driver IC (`GxEPD2_290c`).
- **Power Lifecycle:**
  1. Boot up -> Check wake-up source (Timer vs GPIO 9 Boot Switch).
  2. Connect to Wi-Fi.
  3. Send HTTP GET request with stored `ETag`.
  4. If `HTTP 200`: Read stream, push bitmap via `GxEPD2_3C`, save new `ETag`.
  5. If `HTTP 304`: Skip display refresh, keep current screen.
  6. Disconnect Wi-Fi and enter `esp_deep_sleep()`.

---

## 4. Implementation Roadmap for Antigravity IDE

### Phase 1: Server Customization
- [x] Install dependencies in `/server/epaper-server/requirements.txt` into Python venv (`/server/epaper-server/venv`).
- [x] Configure `config.json` for 2.9" display (`296x128` canvas resolution) and 3-color palette mapping (Black `#000000`, White `#FFFFFF`, Red `#FF0000`).
- [x] Fixed Pillow 10+ compatibility (`textbbox`) in `helper.py`/`date.py` and added graceful missing-credential handling in `googlecalendar.py`. Verified 296x128 image rendering (`test_output.png`).
- [x] Created `config/credentials.json.example` and `config/README.md` for Google Calendar OAuth setup.
- [x] Implemented `quotes` widget module (`epaperengine/widgets/quotes.py`) with dynamic quote fetching, local fallback pool, auto text-wrapping, and 3-color styling.

### Phase 2: ESP32-C6 Firmware Development
- [x] Create `platformio.ini` targeting `seeed_xiao_esp32c6`.
- [x] Configure `GxEPD2_3C` constructor with GPIO 18 (MOSI), GPIO 19 (SCK), GPIO 1 (CS), GPIO 2 (DC), GPIO 21 (RST), and GPIO 22 (BUSY).
- [x] Implement HTTP client handling `ETag` headers and deep sleep duration parameters.
- [x] Add GPIO 9 Boot switch interrupt wake-up logic for manual force-refresh button.

### Phase 3: Hardware Assembly & Enclosure
- [ ] Prototype hardware connections on breadboard (MCU, display SPI, switches, buzzer) powered via USB.
- [ ] Transition to perf-board & solder harness once component configuration is validated.
- [ ] Integrate and test Li-ion battery & charging circuit on perf-board.
- [ ] Print 3D bezel/case and assemble plexiglass protective front window.
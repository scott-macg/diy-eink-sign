# Changelog & Update History (`CHANGELOG.md`)

All notable changes to the DIY 2.9" E-Ink Smart Sign project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased] - 2026-07-29

### Added & Improved
- **Modular Hardware Vector SVG Schematics (`/schematics`):**
  - Created standalone vector SVG schematics for all 4 functional hardware sub-systems in `schematics/`: [`speaker_schematic.svg`](file:///home/smacd/diy-eink-sign/schematics/speaker_schematic.svg), [`battery_sensing_schematic.svg`](file:///home/smacd/diy-eink-sign/schematics/battery_sensing_schematic.svg), [`display_mcu_schematic.svg`](file:///home/smacd/diy-eink-sign/schematics/display_mcu_schematic.svg), and [`microswitch_schematic.svg`](file:///home/smacd/diy-eink-sign/schematics/microswitch_schematic.svg).
  - Created master schematics guide [`schematics/README.md`](file:///home/smacd/diy-eink-sign/schematics/README.md) and updated [`breadboard_wiring.md`](file:///home/smacd/diy-eink-sign/breadboard_wiring.md) to render SVG schematics directly.
- **Empirical Battery Profiling & C++ Calibration (`/utils` & `/firmware`):**
  - Processed 6.8 hours of untethered profiling data (815 samples) from [`utils/battery_curve.csv`](file:///home/smacd/diy-eink-sign/utils/battery_curve.csv).
  - Added standalone analysis script [`utils/generate_curve_table.py`](file:///home/smacd/diy-eink-sign/utils/generate_curve_table.py) for noise reduction and C++ curve array generation.
  - Updated C++ firmware files ([`firmware/src/battery_curve.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/battery_curve.cpp), [`firmware/data/battery_curve.csv`](file:///home/smacd/diy-eink-sign/firmware/data/battery_curve.csv), and [`firmware/data/battery_curve.example.csv`](file:///home/smacd/diy-eink-sign/firmware/data/battery_curve.example.csv)) with calibrated hybrid battery curve values ($4.15\text{V} \rightarrow 100\%$ down to $3.30\text{V} \rightarrow 0\%$).
- **MicroPython Battery Discharge Profiler Upgrade (`/utils`):**
  - Upgraded [`utils/battery_profiler.py`](file:///home/smacd/diy-eink-sign/utils/battery_profiler.py) with top-level `KeyboardInterrupt` handling (`Ctrl-C`) for graceful execution halt, automatic data buffer flushing to LittleFS, and exit fanfare.
  - Added live animated console heartbeat spinner and onboard User LED (`GPIO 15`) status patterns: 2-second double-pulse (*lub-dub*) heartbeat for untethered battery tracking and 3-burst flash when recording samples to LittleFS.
  - Adjusted `FULL_CHARGE_VOLTAGE` threshold to `4.14V` to align with physical LiPo charger termination voltage.
- **Custom Agent Skills Refactoring (`.agents/skills`):**
  - Refactored project rules in [`AGENTS.md`](file:///home/smacd/diy-eink-sign/AGENTS.md) into a standalone project skill [`wrapup-routine`](file:///.agents/skills/wrapup-routine/SKILL.md), eliminating per-prompt instruction bloat.

### Fixed
- **Battery Profiler Generator Bug (`/utils`):**
  - Fixed cumulative minimum latching bug in `generate_cpp_code()` in [`utils/battery_web_profiler.py`](file:///home/smacd/diy-eink-sign/utils/battery_web_profiler.py) that caused identical voltage values across all percentages. Added startup transient guard ($t \ge 180\text{s}$) and 9-sample moving median filtering.
- **MicroPython Compatibility (`/utils`):**
  - Handled `AttributeError` on `sys.stdout.flush()` in [`utils/battery_profiler.py`](file:///home/smacd/diy-eink-sign/utils/battery_profiler.py) for MicroPython stream compatibility.
- **Hardware Debugging & Perf-Board Divider Rework:**
  - Diagnosed ungrounded bottom 100kΩ resistor on the 30x70mm perf-board causing ADC pin saturation (`7.208V` / `4.08V` at `D0`). Guided 3-node rewiring to restore proper 1:2 division ($4.17\text{V} \rightarrow 2.08\text{V}$ at `D0`).

## [v0.2.0] - 2026-07-25

### Added & Refactored (Architecture Overhaul)
- **Vercel Serverless FastAPI Backend (`/server`):**
  - Deprecated legacy containerized backend and AIOHTTP/Flask server (`run.py`, `render.yaml`, `googlemaps.py`).
  - Created FastAPI entry point `server/api/index.py` with 296x128 3-color Pillow layout engine (`server/api/composer.py`).
  - Built `GET /api/sync` (bilateral telemetry & manifest/bitmap payload exchange) and `HEAD /api/checkpoint` (sub-1.5s delta checks via `If-None-Match` ETag validation).
  - Integrated Google Calendar appointment syncing (`server/api/gcal.py`) and instant message overrides.
  - Added Developer Mode compositor overlay: visual `🛠 DEV` badge rendered server-side into 296x128 bitmaps when `developer_mode=True`.

- **Mobile PWA Dashboard (`/web`):**
  - Hosted static Progressive Web App with dark mode glassmorphism UI (`index.html`, `styles.css`, `app.js`, `manifest.json`, `sw.js`).
  - Live display preview rendering (`/api/render.png`), telemetry stats (battery %, ADC, reboots, ETag), custom push message override form, and remote Developer Mode toggle.

- **Offline-First Smart Edge Node Firmware (`/firmware`):**
  - Refactored ESP32-C6 firmware into an offline-first schedule executor with LittleFS manifest and binary bitmap caching (`manifest_manager.h/.cpp`).
  - Implemented sub-1.5s mid-day `HEAD /api/checkpoint` requests: disconnects Wi-Fi within < 1.5s on `304 Not Modified`.
  - Added GxEPD2 3-color panel bitmap rendering directly from LittleFS raw flash buffers, PWM audio notification chime, and RTC deep sleep execution.

## [v0.1.0-alpha] - 2026-07-25

### Pre-Release / Alpha Highlights
- Initial alpha pre-release in preparation for physical perf board prototype assembly.
- Added `HISTORY.md` to [`.gitignore`](file:///home/smacd/diy-eink-sign/.gitignore) in favor of [`CHAT_DIARY.md`](file:///home/smacd/diy-eink-sign/CHAT_DIARY.md).
- Cleaned up stray OS metadata files (`*:Zone.Identifier`) and committed local offline quotes dataset (`~1,950` items) in `server/epaper-server/assets/quotes-v6.json`.

### Added
- **End-of-Session Housekeeping Protocol (`AGENTS.md`):**
  - Created [`AGENTS.md`](file:///home/smacd/diy-eink-sign/AGENTS.md) defining an end-of-session routine covering security reviews, documentation updates (`CHANGELOG.md`, `README.md`), chat diary entries, and user approval for Git commits and pushes.
  - Added `.diary_format.json`, `.diary_prompt.md`, and `CHAT_DIARY.md` to [`.gitignore`](file:///home/smacd/diy-eink-sign/.gitignore) to maintain privacy while the repository is public.

## [Unreleased] - 2026-07-24


### Added
- **Dynamic Battery Discharge Curve Calibration Engine (`/firmware`):**
  - Added `BatteryCurveManager` ([`battery_curve.h`](file:///home/smacd/diy-eink-sign/firmware/src/battery_curve.h) / [`battery_curve.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/battery_curve.cpp)) to load and parse LittleFS `/battery_curve.csv` calibration data.
  - Implemented piecewise linear interpolation for mapping non-linear LiPo battery discharge voltages to exact percentage readings.
  - Updated `readBatteryPercent()` in [`main.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/main.cpp) to query `batteryCurveManager`, with automatic fallback to linear math ($3.3\text{V} - 4.2\text{V}$) if `/battery_curve.csv` is missing or invalid.
  - Created baseline placeholder CSV files [`firmware/data/battery_curve.csv`](file:///home/smacd/diy-eink-sign/firmware/data/battery_curve.csv) and [`firmware/data/battery_curve.example.csv`](file:///home/smacd/diy-eink-sign/firmware/data/battery_curve.example.csv).

- **Multi-Scheme E-Paper Refresh Engine (`/firmware`):**
  - Implemented 4 configurable display refresh modes (`REFRESH_MODE_FULL`, `REFRESH_MODE_PARTIAL`, `REFRESH_MODE_FAST_BW`, `REFRESH_MODE_TWO_PASS`) in [`config.h`](file:///home/smacd/diy-eink-sign/firmware/src/config.h) and [`main.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/main.cpp).
  - Added `pngDrawCallbackFastBW()` for expedited 1.5-second 1-bit B/W partial rendering.
  - Added **Two-Pass Mode** (Default): Renders an instant 1.5s B/W preview when cycling screens, followed by a delayed 3-color clean pass after a 10s pause to clear ghosting.
  - Added **E-Paper Refresh Mode** dropdown selector in the Web Console UI.

- **Hardware UI Controls & Top/Back Microswitches (`/firmware` & `breadboard_wiring.md`):**
  - Updated top microswitch (`GPIO 9` / `BOOT_BTN_PIN`) with press-duration detection in [`main.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/main.cpp):
    - **Short Press:** Wakes device, sends `X-Display-Action: cycle` header (bypassing 304 ETag cache) to force-render the next screen/quote of the day.
    - **Long Press (>400ms Hold):** Enters Maintenance Mode (starts Web Console and WebSockets REPL).
  - Documented physical placement & interaction rules in [`breadboard_wiring.md`](file:///home/smacd/diy-eink-sign/breadboard_wiring.md) for top raised microswitch and back recessed pin-hole reset switch (`CHIP_PU`).

- **Local Quotes Asset Integration (`/server/epaper-server`):**
  - Integrated offline quotes dataset [`assets/quotes-v6.json`](file:///home/smacd/diy-eink-sign/server/epaper-server/assets/quotes-v6.json) containing ~1,950 quotes and science facts.
  - Updated [`QuotesWidget`](file:///home/smacd/diy-eink-sign/server/epaper-server/epaperengine/widgets/quotes.py) to load quotes locally from server assets instead of calling third-party APIs.

- **Platform-Agnostic Server Configuration (`/firmware`):**
  - Added `wifi_timeout_ms` to `DeviceConfig` struct in [`config_manager.h`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.h) and [`config_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.cpp).
  - Added `WiFi Timeout (ms)` field to the Maintenance Mode Web Console UI ([`web_server_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/web_server_manager.cpp)).
  - Created [`firmware/data/config.json`](file:///home/smacd/diy-eink-sign/firmware/data/config.json) and [`firmware/data/config.example.json`](file:///home/smacd/diy-eink-sign/firmware/data/config.example.json) templates for LittleFS filesystem uploads.
  - Enabled dynamic switching between local LAN servers (`http://`) and cloud SSL endpoints (`https://`) without recompiling.

- **Standalone Battery Profiler Utility (`/utils`):**
  - Created standalone MicroPython battery profiling utility [`utils/battery_profiler.py`](file:///home/smacd/diy-eink-sign/utils/battery_profiler.py) for mapping real non-linear LiPo discharge curves.
  - Integrated Legend of Zelda audio chimes (Item Discovery Fanfare on full charge; Lost Life Ditty on cutoff).
  - Added RAM buffering to flush CSV data to LittleFS periodically, protecting flash filesystem integrity.

- **Seeed Studio Battery Sensing Compliance (`/firmware`):**
  - Added 16-sample ADC averaging in `readBatteryVoltage()` ([`main.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/main.cpp)) to eliminate ADC reading noise.
  - Added explicit `pinMode(BAT_SENSE_PIN, INPUT)` initialization in `setup()`.
  - Updated [`breadboard_wiring.md`](file:///home/smacd/diy-eink-sign/breadboard_wiring.md) speaker positive rail to `3V3`/`BAT+` (since 5V pin carries 0V on untethered battery power).

- **Remote Device Management & LittleFS Configuration (`/firmware`):**
  - Added `ConfigManager` ([`config_manager.h`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.h) / [`config_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.cpp)) to manage runtime configuration parameters in `/config.json` via LittleFS.
  - Supported dynamic settings: `wifi_ssid`, `wifi_pass`, `server_url`, `display_token`, `wifi_timeout_ms`, `default_sleep_sec`, `refresh_mode`, `audio_battery_alert`, `developer_mode`, `maintenance_timeout_sec`.
  - Enforced `audio_battery_alert = false` by default.

- **Web REPL Console & REST Asset Management (`/firmware`):**
  - Added `WebServerManager` ([`web_server_manager.h`](file:///home/smacd/diy-eink-sign/firmware/src/web_server_manager.h) / [`web_server_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/web_server_manager.cpp)) embedding a responsive glassmorphism Dashboard UI in firmware `PROGMEM`.
  - Implemented an interactive Web REPL terminal over WebSockets (`/ws` port 81) supporting runtime commands (`info`, `status`, `ls`, `cat`, `rm`, `config`, `set`, `play`, `refresh`, `reboot`).
  - Added LittleFS REST File Manager endpoints (`/api/files`, `/api/upload`, `/api/delete`) for managing audio clips (`.wav`) and images (`.png`).

- **Persistent Developer Mode:**
  - Integrated `"developer_mode"` toggle in `/config.json`.
  - When enabled (`"developer_mode": true`), the ESP32-C6 automatically boots into Maintenance Mode, bypasses deep sleep, and maintains active Wi-Fi, Web Console, and REPL connectivity.

- **Hardware & Sensing:**
  - Integrated 902540 3.7V 800mAh 2.96Wh 25C LiPo battery specification into system design.
  - Added 2x 100kΩ voltage divider network on pin `D0` (`GPIO 0` / `ADC1_CH0`) to scale 4.2V max battery voltage down to 2.1V for safe 12-bit ADC reading.
  - Documented step-by-step LiPo battery direct-soldering safety guidelines in `breadboard_wiring.md`.
  - Added NPN transistor dynamic speaker driver circuit on `D6` (`GPIO 16`) for 8kHz 8-bit PCM audio notifications.

- **Backend Server (`/server/epaper-server`):**
  - Created `epaperengine/battery.py` module to dynamically overlay a 3-color battery icon in the lower right-hand corner (`x=268, y=112`) when `X-Battery-Percent` $\le 20\%$.
  - Implemented `epaperengine/widgets/quotes.py` custom widget with local fallback pool, word-wrap engine, and 3-color palette styling.
  - Added `credentials.json.example` and `config/README.md` for Google Calendar OAuth integration.
  - Optimized deployment for Render.com PaaS by stubbing unconfigured external APIs to prevent startup crashes.

### Changed
- **Flash Partitioning:** Updated `platformio.ini` with `board_build.filesystem = littlefs` and `board_build.partitions = huge_app.csv`, expanding application flash limit to 3MB to accommodate Web Server, WebSockets, and LittleFS features.
- Updated `plan.md` and `breadboard_wiring.md` to reflect hardware prototyping validations, battery pinout mapping, remote management architecture, and component lists.

### Fixed
- **Bug — Duplicate `playSoundSuccess()` call ([`main.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/main.cpp)):** The success chime was firing twice on every successful display update: once after the primary render pass and once unconditionally after `free(buffer)`. Removed the spurious post-free call. In Two-Pass Mode, the chime now correctly fires once after Pass 1 and once after Pass 2.
- **Security — Stored XSS in File Manager ([`web_server_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/web_server_manager.cpp)):** File names returned by `/api/files` were injected into `innerHTML` via template literals, allowing a maliciously named file to execute arbitrary JavaScript in the dashboard. Replaced with DOM element construction using `textContent` and `addEventListener`.
- **Security — Path Traversal in File Upload/Delete ([`web_server_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/web_server_manager.cpp)):** The `handleFileUpload()` and `handleDeleteFile()` handlers did not reject filenames containing `..` sequences. Added explicit rejection with HTTP 400 before any filesystem operation.
- **Security — Unvalidated `wifi_timeout_ms` Config Field ([`config_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.cpp)):** Accepting an unclamped timeout value could freeze the device (e.g. 2^32 ms) or prevent all Wi-Fi connections (0 ms). Now clamped to 2,000ms–120,000ms in both `updateFromJson()` and `updateKey()`.
- **Security — Unvalidated `refresh_mode` Config Field ([`config_manager.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/config_manager.cpp)):** Values outside the valid range 0–3 were silently accepted. Added explicit bounds check; out-of-range values are now silently ignored, preserving the current mode.
- **Bug — CSV Header-Skip Drops First Data Row ([`battery_curve.cpp`](file:///home/smacd/diy-eink-sign/firmware/src/battery_curve.cpp)):** The `isHeader` flag was cleared before the `startsWith()` header check, causing the first data row to be silently discarded when the CSV contained no header line. Rearranged logic to only `continue` after a confirmed header match.
- **Portability — `hashlib` Import in Battery Profiler ([`utils/battery_profiler.py`](file:///home/smacd/diy-eink-sign/utils/battery_profiler.py)):** Removed `import hashlib` and its usage in `run_synthetic_load()`. The `hashlib` module is not available on all ESP32 MicroPython builds; `math.sin`/`cos` is sufficient and universally supported for CPU load simulation.
- **Hygiene — Build Artifacts Untracked:** Ran `git rm -r --cached firmware/.pio/build/` to stop tracking compiled binaries, `.elf`, `.map`, and object files. The `firmware/.gitignore` already excludes `.pio/`, but previously-committed artifacts remained tracked.

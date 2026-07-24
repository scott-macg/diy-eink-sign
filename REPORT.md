# DIY E-Ink Sign — Code Review Report

> **Generated:** 2026-07-24  
> **Scope:** Full repository review covering structure, purpose, conventions, code quality, and potential issues.

---

## 1. Repository Structure

```
diy-eink-sign/
├── firmware/                    # PlatformIO / Arduino project (ESP32-C6)
│   ├── src/
│   │   ├── main.cpp             # Device entry point & main loop
│   │   ├── config.h             # Hardware pin defs & compile-time defaults ← tracked with secrets
│   │   ├── config.example.h     # Template for config.h (safe to commit)
│   │   ├── config_manager.cpp/h # Runtime config r/w via LittleFS (JSON)
│   │   └── web_server_manager.cpp/h  # Maintenance web UI + WebSocket REPL
│   ├── platformio.ini           # Build system & library pinning
│   └── assets/ lib/ venv/ .pio/
│
├── server/
│   ├── epaper-server/           # Python aiohttp image-generation backend
│   │   ├── run.py               # CLI entry point (click) + HTTP routes
│   │   ├── config.json          # Active server config ← tracked with secrets
│   │   ├── config.example.json  # Safe template
│   │   ├── Dockerfile           # Container for Render.com deployment
│   │   ├── requirements.txt     # Python deps (unpinned)
│   │   └── epaperengine/
│   │       ├── display.py       # Display class: widget orchestration + palette
│   │       ├── asynchronous.py  # Async loop: periodic image regeneration
│   │       ├── battery.py       # Overlay low-battery icon on PIL image
│   │       ├── helper.py        # DrawHelper, FontProvider, ImageProvider
│   │       ├── utils.py         # Small parsing helpers
│   │       └── widgets/
│   │           ├── base.py      # BaseWidget ABC
│   │           ├── date.py      # Current date display
│   │           ├── quotes.py    # Random quote from API w/ fallback
│   │           ├── weather.py   # OpenWeatherMap (currently disabled)
│   │           ├── googlecalendar.py  # Google Calendar (currently disabled)
│   │           └── googlemaps.py      # Google Maps commute time (currently disabled)
│   │
│   └── epaper-esp32/            # Abandoned/alternative IDF-based firmware
│       └── src/
│           ├── main.c           # Earlier C implementation (ESP-IDF style)
│           └── epd.c            # Low-level e-paper driver in C
│
├── vendor/WeActStudio/          # Vendor reference materials
├── render.yaml                  # Render.com deployment manifest
├── convert_and_push.py          # Dev tool: convert image to raw display format
├── play_image.py / play_chime.py # Dev/test scripts
├── breadboard_wiring.md         # Hardware wiring documentation
├── mermaid_diagram.md           # System architecture diagram
├── plan.md                      # Development notes / planning
└── CHANGELOG.md                 # Release history
```

**Summary:** The repository is a **monorepo** hosting two distinct sub-projects (firmware and server) that are tightly coupled by an HTTP API contract.

---

## 2. Overall Purpose & Functionality

This project implements a **DIY battery-powered e-ink smart sign** built around the following stack:

| Layer | Technology |
|---|---|
| Microcontroller | Seeed Studio XIAO ESP32-C6 |
| Display | WeAct 2.9" 3-color e-paper (296×128, B/W/R) |
| Firmware | C++ / Arduino (PlatformIO) |
| Backend | Python / aiohttp, deployed on Render.com |
| Infrastructure | Docker + Render free tier |

### Data Flow

```
[Python Server] → generates PNG image (296×128, 3-color palette)
      ↓ HTTP GET /get/ (with ETag caching)
[ESP32-C6] → decodes PNG → renders to e-paper display → deep sleeps
```

### Operational Modes

1. **Normal Low-Power Mode** — Wake from deep sleep → connect WiFi → GET image → refresh display → sleep (interval driven by server `Cache-Control: max-age`).
2. **Maintenance Mode** — Triggered by holding BOOT button or `developer_mode=true` in config → stays awake, serves a web dashboard with: WebSocket REPL console, config editor, LittleFS file manager.

### Widget System (Server-side)

The server uses a composable **widget** model. Each widget (`date`, `quotes`, `weather`, `googlecalendar`, `googlemaps`) independently fetches data and draws into a PIL sub-image. The `Display` class tiles these into the final image with 3-color palette quantization. Currently active widgets are `date` and `quotes`; the others are stubbed out.

### Battery Management

- Voltage is read via an ADC on GPIO 0 through a 2× 100kΩ resistor divider.
- Battery % is sent to the server as `X-Battery-Percent` header.
- The server overlays a red battery icon when ≤ 20%.
- A buzzer alert plays on the device at ≤ 10%.

---

## 3. Coding Conventions

### Python (Server)

| Convention | Observation |
|---|---|
| Style | Mostly PEP 8 compliant; good use of `logging` throughout |
| Naming | `snake_case` for variables/functions; `PascalCase` for classes — consistent |
| Module structure | Good separation: `display.py`, `asynchronous.py`, `battery.py`, `helper.py`, `widgets/` |
| Docstrings | Sparse — only `BaseWidget` and `DrawHelper.text()` have docstrings |
| Type hints | Absent throughout the Python codebase |
| Error handling | Mixed — some `try/except` with logging, others bare `except:` |
| Configuration | Flat JSON config passed as dicts; no validation/schema layer |

### C++ (Firmware)

| Convention | Observation |
|---|---|
| Style | Consistent `[CATEGORY]` prefix logging via `Serial.printf` |
| Naming | `camelCase` for functions/variables; `PascalCase` for classes — consistent |
| Organization | Good use of separate `.cpp/.h` files for `ConfigManager` and `WebServerManager` |
| Comments | Well-commented, especially in `main.cpp` |
| Constants | Hardware pins and thresholds centralized in `config.h` |
| Global state | `configManager` and `webServerManager` are global singletons (acceptable for embedded) |

### Both

- Example/template config files exist (`config.example.h`, `config.example.json`) — good practice.
- `CHANGELOG.md` is maintained.

---

## 4. Code Quality & Code Smells

### 🔴 Critical

#### C-1: Real Credentials Committed to `config.h`

`firmware/src/config.h` contains a real Wi-Fi SSID, Wi-Fi password, server URL, and display token committed to the repository. This is a significant security leak. `config.h` is **not** in `.gitignore`.

```c
// firmware/src/config.h (L34–39) — REAL SECRETS
#define WIFI_SSID   "curlmacg"
#define WIFI_PASS   "1qaz2wsx3edc4rfv"
#define SERVER_URL  "https://esp32-epaper-display.onrender.com/get/"
#define DISPLAY_TOKEN "sign_token_123"
```

`config.example.h` (the correct template) exists but is not being used to keep secrets out.

#### C-2: Real Token Committed to `config.json` (Server)

`server/epaper-server/config.json` contains the live token `sign_token_123`. This file is also not excluded from git.

---

### 🟠 High

#### H-1: Deep Sleep Is Disabled (Critical Feature Regression)

In `firmware/src/main.cpp` (L281–283), the three deep sleep lines are **commented out**:

```cpp
// esp_deep_sleep_enable_gpio_wakeup(...)
// esp_sleep_enable_timer_wakeup(...)
// esp_deep_sleep_start();
```

This means the device **never enters deep sleep** in Normal Mode — it just exits `setup()` and spins in `loop()` doing nothing. For a battery-powered device this is catastrophic; it will drain the battery in hours instead of weeks.

#### H-2: `isdigit()` Battery Parsing Is Too Narrow

In `run.py` (L66):

```python
batt_pct = int(batt_pct_str) if batt_pct_str and batt_pct_str.isdigit() else None
```

`isdigit()` rejects `-` so negative readings from a faulty sensor won't be caught; it also accepts values like `999`. There is no bounds-check before comparing `batt_pct <= 20`.

#### H-3: `images_equal` Logic Is Inverted

In `asynchronous.py` (L14–19):

```python
def images_equal(image_1, image_2):
    if image_1 is None:
        return True   # Returns "equal" when there is no prior image — wrong
    return ImageChops.difference(image_1, image_2).getbbox() is not None
    # getbbox() is not None → differences exist → this is NOT "equal"
```

The function name says "equal" but returns `True` when images differ. The name and semantics are contradictory — a latent bug waiting to confuse a future developer.

#### H-4: Bare `except:` Clause

In `asynchronous.py` (L56):

```python
except:
    logger.exception(...)
```

Catching all exceptions including `SystemExit` and `GeneratorExit` is bad practice. Should be `except Exception:`.

#### H-5: TLS Certificate Verification Disabled

In `main.cpp` (L192):

```cpp
secureClient.setInsecure();
```

All HTTPS connections skip certificate validation, making the device vulnerable to man-in-the-middle attacks. Since the server is on Render.com with a valid Let's Encrypt cert, proper validation should be achievable.

#### H-6: No Authentication on Maintenance Web Server

The web dashboard served in Maintenance Mode has **no password protection**. Anyone on the same Wi-Fi network can access the REPL, change config (including Wi-Fi credentials and server URL), upload/delete LittleFS files, and reboot the device.

---

### 🟡 Medium

#### M-1: `import os` Mid-File in `run.py`

`import os` appears on line 119, after other imports and after a function definition that already uses `os`. By PEP 8, all imports should be at the top of the file.

#### M-2: `get_status()` Token Lookup Bug

In `run.py` (L29–31):

```python
def get_status(self, token):
    display_id = self.tokens.get(token)
    if token is None:      # ← should check display_id, not token
        return None
```

The guard checks `token` (the input) but the relevant variable to check is `display_id` (what the token maps to). Functionally it works due to Python returning `None` for a dict miss, but the intent is wrong.

#### M-3: `argparse` Is Imported But Never Used

`run.py` imports `argparse` on line 6 but uses `click` exclusively. Dead import.

#### M-4: No Version Pinning in `requirements.txt`

`requirements.txt` has no version constraints. This makes builds non-reproducible — a breaking change in any dependency (e.g., `Pillow`, `aiohttp`) would silently break the server on the next deploy.

#### M-5: `ETag` Sent as a Request Header (Non-Standard)

In `main.cpp` (L205–206):

```cpp
http.addHeader("ETag", storedETag);
http.addHeader("If-None-Match", storedETag);
```

`ETag` is a response header. Sending it in a request is non-standard. Only `If-None-Match` is needed.

#### M-6: HTTP Stream Read Loop Has No Timeout

In `main.cpp` (L234–241), the stream read loop has no timeout. If the TCP stream stalls mid-transfer, the device will hang indefinitely (never entering deep sleep) until the WiFi drops.

#### M-7: Disabled Widgets Produce Silent Black Boxes

The `update()` methods in `weather.py`, `googlecalendar.py`, and `googlemaps.py` set their data to `None` unconditionally. If a user configures one of these widgets, they see a black box with a "(Disabled)" label but no log-level explanation. This is invisible and confusing.

#### M-8: Font/Image Caches Are Not Shared Across Displays

`FontProvider` and `ImageProvider` are instantiated per `Display` object. In a multi-display setup, the same font files would be loaded once per display instead of being shared globally.

#### M-9: Pixel-by-Pixel Loop in `helper.py` Is Slow

In `helper.py` (L111–118), a nested Python loop iterates over every pixel to replace colors. For a 296×128 canvas this is ~38,000 iterations per text draw call. PIL's `ImageOps` or numpy would be significantly faster.

#### M-10: `date.replace(tzinfo=...)` Result Is Discarded

In `weather.py` (L138):

```python
date = parse(weather_data["dt_txt"])
date.replace(tzinfo=pytz.UTC)   # ← datetime is immutable; this is a no-op
```

`datetime.replace()` returns a new object — the result is never used, so the timezone is never applied. This is a latent bug that would cause incorrect time display when the widget is re-enabled.

#### M-11: Typo in `ls` REPL Command Output

In `web_server_manager.cpp` (L475):

```cpp
reply = "--- Filesystem List (/ ---\n";  // Missing closing ')'
```

---

### 🟢 Low / Style

#### L-1: `epaper-esp32/` Is Dead Code

`server/epaper-esp32/` contains an older ESP-IDF (C-language) firmware. It appears fully superseded by the PlatformIO Arduino project in `firmware/` and adds confusion about which firmware is authoritative.

#### L-2: Commented-Out Code Throughout

- `run.py` L142: `# loop.run_until_complete(image_generator.stop())`
- `display.py` L59: alternative quantize call
- `weather.py` L63–81: entire API implementation
- `main.cpp` L281–283: deep sleep calls (see H-1)

Commented-out code should be removed; git history preserves it.

#### L-3: Hardcoded Magic Numbers

- `battery.py`: pixel offsets `28`, `16`, `22`, `12` with no named constants
- `display.py`: palette `[0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0] * 64` is opaque
- `main.cpp`: battery voltage limits `3.3f` and `4.2f` inline in calculation

#### L-4: `FIXME` Comments Left Unresolved

In `utils.py`:

```python
def parse_dimensions(dimensions):
    # FIXME Error handling
    return list(map(int, dimensions.split("x")))
```

A malformed `size` in `config.json` would crash the server at startup with no informative error.

#### L-5: `asyncio.get_event_loop()` Is Deprecated

In `run.py` (L128), `asyncio.get_event_loop()` is deprecated since Python 3.10. The preferred approach is `asyncio.run()`.

#### L-6: Dev Artifacts Committed to Repository

`plan.md` and `gemini-code-1784817089042.txt:Zone.Identifier` are tracked in version control. These should be added to `.gitignore`.

#### L-7: Widget Module Has No Explicit Public API

`display.py` resolves widget classes via `getattr(widgets, widget_name + "Widget")`. A typo in `config.json` gives an unhelpful `AttributeError`. An explicit registry dict would give a better error message.

---

## 5. Potential Issues Summary

| # | Severity | Area | Issue |
|---|---|---|---|
| C-1 | 🔴 Critical | Security | Wi-Fi password & tokens committed to git |
| C-2 | 🔴 Critical | Security | Live server token in committed `config.json` |
| H-1 | 🟠 High | Firmware | Deep sleep disabled — device will drain battery |
| H-2 | 🟠 High | Server | Battery % parsing accepts out-of-range values |
| H-3 | 🟠 High | Server | `images_equal()` has inverted name/logic |
| H-4 | 🟠 High | Server | Bare `except:` swallows `SystemExit` etc. |
| H-5 | 🟠 High | Security | TLS cert verification disabled (`setInsecure()`) |
| H-6 | 🟠 High | Security | Maintenance web server has no authentication |
| M-1 | 🟡 Medium | Server | `import os` mid-file; used before import point |
| M-2 | 🟡 Medium | Server | `get_status()` checks wrong variable for `None` |
| M-3 | 🟡 Medium | Server | `import argparse` is dead code |
| M-4 | 🟡 Medium | Deps | No version pinning in `requirements.txt` |
| M-5 | 🟡 Medium | Firmware | `ETag` sent as request header (non-standard) |
| M-6 | 🟡 Medium | Firmware | HTTP stream read has no timeout |
| M-7 | 🟡 Medium | Server | Disabled widgets produce silent black boxes |
| M-8 | 🟡 Medium | Server | Font/image caches not shared across displays |
| M-9 | 🟡 Medium | Server | Pixel-by-pixel Python loop is slow |
| M-10 | 🟡 Medium | Server | `date.replace(tzinfo=...)` result discarded |
| M-11 | 🟡 Medium | Firmware | Typo in `ls` REPL command output |
| L-1 | 🟢 Low | Structure | `epaper-esp32/` is dead code |
| L-2 | 🟢 Low | Style | Commented-out code throughout |
| L-3 | 🟢 Low | Style | Hardcoded magic numbers |
| L-4 | 🟢 Low | Style | Unresolved `FIXME` with no error handling |
| L-5 | 🟢 Low | Server | `asyncio.get_event_loop()` deprecated |
| L-6 | 🟢 Low | Structure | Dev artifacts committed to repo |
| L-7 | 🟢 Low | Server | Widget module has no explicit public API |

---

## 6. Recommendations (Prioritized)

1. **Immediately rotate** the Wi-Fi password and display token — both are in git history. Add `config.h` and `config.json` to `.gitignore` and use only the `.example` variants in version control.
2. **Re-enable deep sleep** in `main.cpp` (uncomment the 3 lines at L281–283) — this is the single most impactful change for battery life.
3. **Add a PIN/password** to the Maintenance Mode web server, or at minimum add an IP/network check.
4. **Pin all Python dependencies** in `requirements.txt` (e.g., `aiohttp==3.9.5`).
5. **Rename `images_equal`** to `images_differ` and fix the `image_1 is None` return value.
6. **Move `import os`** to the top of `run.py` and remove `import argparse`.
7. **Fix the `date.replace()` bug** in `weather.py` (`date = date.replace(tzinfo=pytz.UTC)`).
8. **Remove or archive `server/epaper-esp32/`** to reduce confusion.
9. Consider adding **type hints** and **input validation** (e.g., Pydantic) for the server config loading path.
10. Replace the **pixel-by-pixel loop** in `helper.py` with a vectorized PIL or numpy approach for faster image generation.

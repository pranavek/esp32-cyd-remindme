# RemindMe

A self-contained ESP32 desk gadget that nudges you to **stretch, drink water,
and walk** at intervals you set. Reminders show up as a touch-acknowledgeable
popup; a Today screen and a 14-day stats screen rotate while idle. Each event
streams to a Google Sheet via an Apps Script web app, which also publishes a
short [ntfy](https://ntfy.sh/) notification to your phone. A local web UI on
`http://remindme.local/` configures Wi-Fi, the Apps Script endpoint, the
ntfy topic, daily goals, intervals, and quiet hours.

> **Hardware:** ESP32-2432S028R Cheap Yellow Display, **the dual-USB
> revision** (ST7789 panel, XPT2046 touch). The single-USB ILI9341 boards
> need a different display setup — this firmware will boot but render the
> wrong colors on those.

## What's in the box

| Path                           | What it is                                                |
|--------------------------------|-----------------------------------------------------------|
| [src/](src/)                   | Firmware (Arduino / PlatformIO)                           |
| [data/web/](data/web/)         | Local web UI (vanilla HTML/CSS/JS, served from LittleFS)  |
| [data/icons/](data/icons/)     | 64×64 reminder icons                                      |
| [data/icons-small/](data/icons-small/) | 24×24 status-row icons                            |
| [data/fonts/](data/fonts/)     | Smooth fonts (TFT_eSPI `.vlw`) — drop yours here          |
| [backend/Code.gs](backend/Code.gs) | Google Apps Script web app — append + summary endpoints |
| [backend/README.md](backend/README.md) | Step-by-step Apps Script + ntfy setup            |
| [scripts/gen_icons.py](scripts/gen_icons.py) | Regenerates placeholder BMP icons          |
| [.github/workflows/](.github/workflows/) | CI: builds firmware + deploys web flasher to Pages |

## Build & flash (locally)

PlatformIO project, single env `[env:esp32-cyd]`.

```sh
pio run                         # compile
pio run -t upload               # flash firmware
pio run -t uploadfs             # flash LittleFS image (data/) — REQUIRED
pio device monitor              # 250000 baud
```

**Both** the firmware and the LittleFS image must be uploaded — the web UI,
the icons, and the smooth fonts all live in `data/`. Without `uploadfs`,
the device boots but the screen draws fallback text instead of icons and
the web UI returns 404s.

### Two USB ports

The dual-USB CYD has one USB-C and one micro-USB port. **Both deliver
power; only one carries the CH340 USB-serial bridge.** During first
bring-up, plug into one, run `pio device monitor`, and if you see no
output, swap to the other. Once you know which one is the serial port,
note it on the back of the case so you don't lose 30 seconds every time
you reflash.

## Flash from your browser (no PlatformIO needed)

The repo's GitHub Action builds a merged firmware image on every tagged
release and publishes a one-click flasher to GitHub Pages using
[ESP Web Tools](https://esphome.github.io/esp-web-tools/). After enabling
Pages on this repo (Settings → Pages → Source: GitHub Actions), open

```
https://<your-github-username>.github.io/esp32-cyd-remindme/
```

in Chrome, Edge, or any Chromium browser, plug in the device, click
*Connect → RemindMe* and *Install*. The page also prompts for Wi-Fi
credentials at the end so you don't even need the captive portal step.

See [.github/workflows/release.yml](.github/workflows/release.yml) and
the matching site in [docs/site/](docs/site/) for how it's wired up.

## First boot

1. **Power on.** Both USB ports work for power.
2. The device boots into Wi-Fi setup mode (RGB LED breathes magenta) and
   broadcasts an open AP named **`RemindMe-Setup`**.
3. Join that AP from your phone — the captive portal opens automatically.
   Pick your home Wi-Fi and submit your password.
4. The device reboots, joins your Wi-Fi, NTP-syncs, and lands on the
   Today screen.
5. From any phone or laptop on the same Wi-Fi, browse to
   **`http://remindme.local/`**.
6. *Backend* tab → paste your Apps Script `/exec` URL (see
   [backend/README.md](backend/README.md)). Save & reboot.
7. The next reminder will write a row to your sheet and ping ntfy.

If `remindme.local` doesn't resolve (some phones block mDNS while on
mobile data), use the IP address shown on the device's status strip —
visible during the first 60 seconds after boot.

## Recovery gestures

| Gesture                                        | What it does                                |
|------------------------------------------------|---------------------------------------------|
| Hold the screen for ~600 ms during boot        | Wipe Wi-Fi credentials, reboot into portal  |
| Long-press (≥ 800 ms) on Today/Stats           | Toggle the on-screen diagnostic overlay     |
| `POST /api/wifi/reset`                         | Same as the boot gesture, from the web UI   |
| `POST /api/touch/recalibrate`                  | Wipe `/cal.json` so calibration runs again  |
| Delete `/config.json` over the web UI's settings reset | Restore factory defaults             |

## Hardware pin map

These match the dual-USB CYD revision and **must not be reassigned** —
they're documented in `src/Settings.h` and `platformio.ini` as build flags.

| GPIO   | Use                              |
|--------|----------------------------------|
| 12–15  | TFT HSPI bus (MISO/MOSI/SCLK/CS) |
| 2      | TFT DC                           |
| 21     | Backlight (PWM ch.0)             |
| 33     | Touch CS (XPT2046)               |
| 36     | Touch IRQ (input only)           |
| 25 / 32 / 39 | Touch SCLK / MOSI / MISO   |
| 34     | LDR (input only ADC)             |
| 4 / 16 / 17 | RGB LED (active LOW PWM ch.1/2/3) |

## Architecture (firmware)

```
main → Net (WiFiManager + mDNS + AsyncWebServer)
     → TimeSync (configTzTime + dayChanged())
     → Storage (mutex-protected LittleFS, atomic tmp+rename)
     → Config (versioned schema with migration chain)
     → Touch (XPT2046, calibration on /cal.json)
     → Backlight (LDR EMA + DISPOFF/DISPON)
     → RgbLed (active-LOW PWM, sine pulse + flash states)
     → EventQueue (persistent FIFO, survives reboots)
     → Backend (HTTPS POST → Apps Script, FreeRTOS task pinned to core 0)
     → Stats (HTTPS GET summaries → LittleFS cache, also core 0)
     → Reminders (per-second scheduler, quiet hours, snooze)
     → ScreenManager (Today ↔ Stats14 carousel + ReminderPopup overlay)
     → Diagnostics (long-press overlay + /api/state snapshot)
```

**Why the Backend task is pinned to core 0:** synchronous TLS handshakes
against `script.google.com` take 1–3 s. If that runs on the main loop
(core 1), the touch handler stalls and the popup feels broken. Pinning to
core 0 (where the Wi-Fi/IP-stack interrupts already live) keeps the UI
loop responsive. The same pattern is in
[../esp32-cyd-biodome/src/TelegramBus.cpp](../esp32-cyd-biodome/src/TelegramBus.cpp).

## Test plan

A bring-up checklist lives in [docs/TESTPLAN.md](docs/TESTPLAN.md). The
short version: verify both USB ports during initial flash, walk through
captive-portal setup, save config, fire each habit reminder once, pull
network and confirm the queue drains on reconnect.

## Replacing the icons

The committed icons are placeholders generated by
[scripts/gen_icons.py](scripts/gen_icons.py) (Pillow). Drop in nicer 24-bit
BMPs at the same paths and re-run `pio run -t uploadfs`:

```
data/icons/{stretch,water,walk}.bmp        # 64×64
data/icons-small/{stretch,water,walk}.bmp  # 24×24
```

The BMP loader (`src/Bmp.cpp`) only handles 24-bit, uncompressed BMPs.
Anything else falls back to a colored tile with a glyph letter so the
device is still readable.

## Replacing the smooth fonts

`data/fonts/NSBold15.vlw` and `data/fonts/NSBold36.vlw` are referenced as
`FONT_SMALL` / `FONT_LARGE` in `src/Settings.h`. Generate `.vlw` files
using the
[TFT_eSPI Processing tool](https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font),
drop them in `data/fonts/`, and update the `FONT_*` constants if you
rename them.

## License

MIT — see [LICENSE](LICENSE) (add one if you want to publish; the
firmware is small enough that a license file isn't auto-generated).

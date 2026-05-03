#pragma once

#include <Arduino.h>

// ─── Hardware pins (CYD ESP32-2432S028, dual-USB, ST7789) ──────────────────
// TFT and touch pins are *not* listed here — they're provided as -D build
// flags in platformio.ini and consumed directly by TFT_eSPI. Re-declaring
// names like TOUCH_CS in this namespace would clash with those macros.
namespace Pin {
    constexpr int LDR        = 34;   // ambient light sensor (input only)
    constexpr int LED_R      = 4;    // active LOW
    constexpr int LED_G      = 16;   // active LOW
    constexpr int LED_B      = 17;   // active LOW
    constexpr int BACKLIGHT  = 21;   // mirrors TFT_BL build flag
}

namespace PwmCh {
    constexpr int BACKLIGHT  = 0;
    constexpr int LED_R      = 1;
    constexpr int LED_G      = 2;
    constexpr int LED_B      = 3;
}

// ─── Backlight / LDR ───────────────────────────────────────────────────────
constexpr int   LDR_SAMPLE_MS    = 10000;
constexpr float LDR_EMA_ALPHA    = 0.125f;
constexpr int   LDR_SLEEP_RAW    = 3900;   // ↑ raw → darker (CYD wiring)
constexpr int   LDR_WAKE_RAW     = 3500;
constexpr int   BRIGHT_CEIL      = 200;
constexpr int   BRIGHT_FLOOR     = 8;

// ─── Network / mDNS defaults ───────────────────────────────────────────────
// These are factory defaults. The runtime hostname is read from Config and
// can be customised via the web UI (one save → reboot → new hostname).
constexpr char     AP_NAME[]                 = "RemindMe-Setup";
constexpr char     DEFAULT_MDNS_HOSTNAME[]   = "remindme";
constexpr uint16_t HTTP_PORT                 = 80;
constexpr uint32_t WIFI_PORTAL_TIMEOUT_S     = 180;
constexpr uint32_t NTP_SYNC_INTERVAL_S       = 6 * 60 * 60;

// POSIX TZ string. Set to your local TZ before flashing — NTP sync alone
// gives UTC, the strftime helpers need the offset.
//   "UTC0"
//   "EST5EDT,M3.2.0,M11.1.0"           // US Eastern
//   "PST8PDT,M3.2.0,M11.1.0"           // US Pacific
//   "GMT0BST,M3.5.0/1,M10.5.0"         // UK
//   "CET-1CEST,M3.5.0,M10.5.0/3"       // Central Europe
//   "IST-5:30"                         // India
constexpr char TZ_POSIX[]     = "IST-5:30";
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.google.com";

// ─── Reminder defaults (used to seed Config on first boot) ─────────────────
namespace Defaults {
    // Goals (per day)
    constexpr uint16_t STRETCH_MIN = 15;     // minutes of stretching
    constexpr uint16_t WATER_ML    = 2000;   // ml of water
    constexpr uint16_t WALK_MIN    = 30;     // minutes of walking

    // Intervals (between reminders, minutes)
    constexpr uint16_t INTERVAL_STRETCH_MIN = 60;
    constexpr uint16_t INTERVAL_WATER_MIN   = 45;
    constexpr uint16_t INTERVAL_WALK_MIN    = 90;

    // Quiet hours (24h, local) — fires suppressed in [START..END)
    constexpr uint8_t QUIET_START = 22;
    constexpr uint8_t QUIET_END   = 7;
    constexpr bool    QUIET_ON    = true;

    // Snooze duration when user taps Snooze
    constexpr uint16_t SNOOZE_MIN = 10;

    // Auto-snooze on popup if untouched for this long (90 s)
    constexpr uint32_t POPUP_AUTOSNOOZE_MS = 90UL * 1000UL;

    // Carousel auto-rotate between Today and Stats14
    constexpr uint32_t CAROUSEL_MS = 10UL * 1000UL;

    // Stats fetch cadence
    constexpr uint32_t STATS_REFRESH_MS = 5UL * 60UL * 1000UL;

    // Backend retry backoff caps
    constexpr uint32_t BACKEND_TICK_MS    = 5UL * 1000UL;
    constexpr uint32_t BACKEND_MAX_BACKOFF_MS = 5UL * 60UL * 1000UL;

    // Event queue capacity (in-memory ring; LittleFS-backed)
    constexpr size_t QUEUE_CAP = 64;
}

// ─── Fonts on LittleFS (paths without ".vlw") ──────────────────────────────
constexpr char FONT_SMALL[] = "fonts/NSBold15";
constexpr char FONT_LARGE[] = "fonts/NSBold36";

// ─── Colours (RGB565) — sleek dark theme ───────────────────────────────────
namespace Color {
    constexpr uint16_t BG          = 0x0000;   // black
    constexpr uint16_t FG          = 0xFFFF;   // white
    constexpr uint16_t DIM         = 0x7BEF;   // mid-grey
    constexpr uint16_t CARD        = 0x18C3;   // very dark grey card
    constexpr uint16_t ACCENT      = 0xFD20;   // amber today-highlight
    constexpr uint16_t ACCENT_TXT  = 0x0000;
    constexpr uint16_t WATER       = 0x06FF;   // cyan
    constexpr uint16_t WALK        = 0x07E0;   // green
    constexpr uint16_t STRETCH     = 0xFD20;   // amber
    constexpr uint16_t WIFI_OK     = 0x07E0;
    constexpr uint16_t WIFI_RETRY  = 0xFFE0;
    constexpr uint16_t WIFI_FAIL   = 0xF800;
    constexpr uint16_t BTN_FILL    = 0x39C7;
    constexpr uint16_t BTN_FG      = 0xFFFF;
    constexpr uint16_t BAR_BG      = 0x2104;
    constexpr uint16_t OK_FLASH    = 0x07E0;
}

// ─── Layout constants (320×240 landscape, rotation=1) ──────────────────────
namespace Layout {
    constexpr int W = 320;
    constexpr int H = 240;

    // Top status strip
    constexpr int STATUS_Y = 0;
    constexpr int STATUS_H = 22;

    // Habit row block (used by both Today and Stats14)
    constexpr int CONTENT_Y = 24;
    constexpr int CONTENT_H = 200;
    constexpr int ROW_H     = 64;
    constexpr int ROW_GAP   = 4;

    // Today: icon + bar layout per row
    constexpr int ICON_PAD       = 8;
    constexpr int ICON_SIZE      = 48;
    constexpr int BAR_X          = 8 + 48 + 12;       // 68
    constexpr int BAR_W          = 320 - 68 - 8;      // 244
    constexpr int BAR_H          = 14;
    constexpr int BAR_LABEL_DY   = -18;               // label above bar
    constexpr int BAR_VALUE_DY   = 18;                // value text below bar

    // Stats14: 14 bars across remaining horizontal space
    constexpr int STATS_LABEL_W  = 60;
    constexpr int STATS_BAR_X    = 8 + 60 + 4;        // 72
    constexpr int STATS_BAR_AREA_W = 320 - 72 - 8;    // 240
    constexpr int STATS_BARS     = 14;
    constexpr int STATS_BAR_GAP  = 2;
    constexpr int STATS_BAR_W    = (STATS_BAR_AREA_W - (STATS_BARS - 1) * STATS_BAR_GAP) / STATS_BARS;  // ~15
    constexpr int STATS_BAR_MAX_H = 48;

    // Reminder popup card (centred)
    constexpr int POP_W     = 280;
    constexpr int POP_H     = 200;
    constexpr int POP_X     = (W - POP_W) / 2;        // 20
    constexpr int POP_Y     = (H - POP_H) / 2;        // 20
    constexpr int POP_ICON  = 64;
    constexpr int POP_ICON_X = POP_X + 16;
    constexpr int POP_ICON_Y = POP_Y + 24;

    // Three buttons across the bottom of the popup card
    constexpr int POP_BTN_H  = 40;
    constexpr int POP_BTN_GAP = 8;
    constexpr int POP_BTN_AREA_W = POP_W - 24;
    constexpr int POP_BTN_W = (POP_BTN_AREA_W - 2 * POP_BTN_GAP) / 3;
    constexpr int POP_BTN_Y = POP_Y + POP_H - POP_BTN_H - 12;
    constexpr int POP_BTN_X0 = POP_X + 12;
    constexpr int POP_BTN_X1 = POP_BTN_X0 + POP_BTN_W + POP_BTN_GAP;
    constexpr int POP_BTN_X2 = POP_BTN_X1 + POP_BTN_W + POP_BTN_GAP;
}

// ─── Config schema version ─────────────────────────────────────────────────
// Bump on breaking changes to the Config struct shape. Storage::loadConfig()
// runs the migration chain forward from older versions.
constexpr uint16_t CONFIG_SCHEMA_VERSION = 1;

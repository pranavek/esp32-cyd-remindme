// ESP32 CYD RemindMe — habit reminder device with Sheets/ntfy backend.
//
// Boot pipeline:
//   1. mount LittleFS, init display
//   2. Backlight, RgbLed, Touch (calibration if needed)
//   3. Long-press-on-boot → reset Wi-Fi creds
//   4. Load Config, check /wifi_reset.flag from prior /api/wifi/reset
//   5. WiFiManager portal or join saved AP → mDNS → AsyncWebServer
//   6. NTP sync → resume DailyState → start Backend + Stats core-0 tasks
//   7. ScreenManager.renderAll() takes over the display
//   8. loop(): Backlight + RgbLed + Reminders + ScreenManager + touch + Net
//
// Both firmware AND LittleFS image must be uploaded:
//   pio run -t upload
//   pio run -t uploadfs

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "Settings.h"
#include "Config.h"
#include "Storage.h"
#include "TimeSync.h"
#include "Net.h"
#include "Touch.h"
#include "Backlight.h"
#include "RgbLed.h"
#include "EventQueue.h"
#include "Backend.h"
#include "Stats.h"
#include "Reminders.h"
#include "ScreenManager.h"
#include "ReminderPopup.h"
#include "Diagnostics.h"
#include "Api.h"

TFT_eSPI tft = TFT_eSPI();

// ─── UI state ──────────────────────────────────────────────────────────────
enum class UiState : uint8_t { BOOT, WIFI_AP, NTP_SYNC, RUNNING, WIFI_FAIL };
static UiState s_state = UiState::BOOT;
static uint32_t s_bootMs = 0;

static void clearScreen() { tft.fillScreen(Color::BG); }

static void drawCentered(const char* text, int y, const char* font, uint16_t color) {
    if (LittleFS.exists(String(font) + ".vlw")) {
        tft.loadFont(font, LittleFS);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, Color::BG);
        tft.drawString(text, tft.width() / 2, y);
        tft.unloadFont();
    } else {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, Color::BG);
        tft.drawString(text, tft.width() / 2, y, 4);
    }
}

static void drawConnectingSplash() {
    clearScreen();
    drawCentered("RemindMe",     90,  FONT_LARGE, Color::FG);
    drawCentered("connecting...",140, FONT_SMALL, Color::DIM);
}

static void drawSetupScreen() {
    clearScreen();
    drawCentered("Setup mode", 50, FONT_LARGE, Color::ACCENT);

    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        tft.loadFont(FONT_SMALL, LittleFS);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(Color::FG, Color::BG);
        tft.drawString("Join WiFi:",            tft.width() / 2, 100);
        tft.setTextColor(Color::ACCENT, Color::BG);
        tft.drawString(AP_NAME,                 tft.width() / 2, 124);
        tft.setTextColor(Color::FG, Color::BG);
        tft.drawString("then open in browser:", tft.width() / 2, 160);
        tft.setTextColor(Color::ACCENT, Color::BG);
        tft.drawString(WiFi.softAPIP().toString().c_str(),
                                                tft.width() / 2, 184);
        tft.setTextColor(Color::DIM, Color::BG);
        tft.drawString("(should auto-open)",    tft.width() / 2, 210);
        tft.unloadFont();
    }
    s_state = UiState::WIFI_AP;
}

static void drawSyncingClockSplash() {
    clearScreen();
    drawCentered("Connected", 60, FONT_LARGE, Color::WIFI_OK);
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        tft.loadFont(FONT_SMALL, LittleFS);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(Color::FG, Color::BG);
        tft.drawString(WiFi.SSID().c_str(),         tft.width() / 2, 110);
        tft.setTextColor(Color::DIM, Color::BG);
        tft.drawString(WiFi.localIP().toString().c_str(),
                                                    tft.width() / 2, 134);
        tft.drawString("syncing time...",           tft.width() / 2, 180);
        tft.unloadFont();
    }
}

// Check for the deferred WiFi-reset flag written by /api/wifi/reset, then
// wipe creds and reboot before WiFiManager runs.
static void maybeApplyWifiResetFlag() {
    if (!LittleFS.exists("/wifi_reset.flag")) return;
    LittleFS.remove("/wifi_reset.flag");
    Serial.println("[boot] wifi_reset.flag found — clearing credentials");
    WiFiManager wm;
    wm.resetSettings();
    delay(200);
    ESP.restart();
}

void setup() {
    Serial.begin(250000);
    delay(50);
    Serial.println();
    Serial.println("=== ESP32 CYD RemindMe boot ===");
    Serial.printf("[boot] fw=%s\n", FW_VERSION);
    s_bootMs = millis();

    if (!LittleFS.begin(false)) {
        Serial.println("[fs] mount failed; formatting");
        LittleFS.format();
        LittleFS.begin(false);
    }
    Serial.printf("[fs] %u / %u bytes used\n",
                  (unsigned)LittleFS.usedBytes(),
                  (unsigned)LittleFS.totalBytes());

    tft.init();
    tft.setRotation(1);          // landscape (320 wide × 240 tall)
    tft.setSwapBytes(true);

    Backlight::begin(&tft);
    RgbLed::begin();
    RgbLed::setLdrSource(Backlight::ldrEma);

    Touch::begin(&tft);

    // Boot-time long-press → wipe WiFi creds. Only meaningful when calibration
    // was already loaded (otherwise the user just tapped 4 corner crosses and
    // is still touching the panel).
    if (LittleFS.exists("/cal.json")) {
        clearScreen();
        drawCentered("Hold for setup...", 130, FONT_SMALL, Color::DIM);
        uint32_t held = 0;
        for (uint32_t t0 = millis(); millis() - t0 < 2500; ) {
            if (Touch::isPressed()) ++held;
            else                    held = 0;
            if (held > 30) {
                drawCentered("clearing WiFi", 160, FONT_SMALL, Color::ACCENT);
                delay(300);
                Net::resetCredentialsAndReboot();
            }
            delay(20);
        }
    }

    Storage::begin();
    Config::Data cfg;
    Storage::loadConfig(cfg);
    Config::setCurrent(cfg);
    Serial.printf("[boot] config: host=%s id=%s\n",
                  cfg.mdns_hostname.c_str(), cfg.device_id.c_str());

    maybeApplyWifiResetFlag();

    drawConnectingSplash();
    RgbLed::set(RgbLed::State::WIFI_SETUP);
    bool ok = Net::startWifi([]() {
        drawSetupScreen();
        RgbLed::set(RgbLed::State::WIFI_SETUP);
    });
    RgbLed::set(RgbLed::State::OFF);

    if (!ok) {
        s_state = UiState::WIFI_FAIL;
        clearScreen();
        drawCentered("WiFi unavailable",      80,  FONT_LARGE, Color::WIFI_FAIL);
        drawCentered("Power-cycle to retry",  130, FONT_SMALL, Color::DIM);
        return;
    }

    s_state = UiState::NTP_SYNC;
    drawSyncingClockSplash();

    TimeSync::begin();
    EventQueue::begin();
    Api::registerRoutes(Net::server());
    Net::startMdnsAndServer();

    // Wait briefly for NTP — the reminder logic needs a clock to make sense.
    uint32_t until = millis() + 8000;
    while (!TimeSync::isSynced() && millis() < until) delay(200);
    if (TimeSync::isSynced()) {
        char y[32], h[6];
        TimeSync::formatYMD(y, sizeof(y));
        TimeSync::formatHM(h, sizeof(h));
        Serial.printf("[time] synced: %s %s\n", y, h);
    } else {
        Serial.println("[time] not yet synced (continuing in background)");
    }

    Storage::resumeTodayFromDisk();
    Reminders::begin();
    Backend::begin();
    Stats::begin();

    EventQueue::enqueue(EventQueue::makeBoot());
    Backend::poke();
    Stats::refresh();

    Diagnostics::begin(&tft);
    ScreenManager::begin(&tft);

    s_state = UiState::RUNNING;
    ScreenManager::renderAll();
}

// ─── Loop ──────────────────────────────────────────────────────────────────
void loop() {
    Backlight::tick();
    RgbLed::tick();
    Net::tick();   // applies any deferred reboot

    if (s_state != UiState::RUNNING) {
        delay(20);
        return;
    }

    Reminders::tick();
    ScreenManager::tick();

    // Day rollover — finalise yesterday's counters and start fresh.
    if (TimeSync::dayChanged()) {
        Storage::flushToday();
        Storage::resetTodayCounters();
        ScreenManager::renderAll();
    }

    if (Backlight::screenAsleep()) {
        if (Touch::isPressed()) {
            Backlight::wake();
            ScreenManager::renderAll();
        }
    } else {
        Touch::Event ev;
        if (Touch::tick(ev)) {
            if (ev.longPress) {
                Diagnostics::toggleOverlay();
                if (!Diagnostics::overlayVisible()) ScreenManager::renderAll();
            } else if (Diagnostics::overlayVisible()) {
                Diagnostics::hideOverlay();
                ScreenManager::renderAll();
            } else {
                ScreenManager::onTouch(ev);
            }
        }
    }

    delay(20);
}

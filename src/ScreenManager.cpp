#include "ScreenManager.h"
#include "Settings.h"
#include "Config.h"
#include "PageToday.h"
#include "PageStats14.h"
#include "ReminderPopup.h"
#include "Reminders.h"
#include "TimeSync.h"
#include "Net.h"
#include "Backend.h"
#include "EventQueue.h"

#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <TFT_eSPI.h>

namespace ScreenManager {

static TFT_eSPI* s_tft = nullptr;
static Screen    s_screen = Screen::Today;
static uint32_t  s_lastCycleMs = 0;
static uint32_t  s_lastStripMs = 0;
static uint32_t  s_lastSubMs   = 0;

// Status strip cache.
static char    s_lastClock[6]  = "--:--";
static uint8_t s_lastWifiState = 0xFF;
static uint8_t s_lastSyncIcon  = 0xFF;

static uint8_t wifiState() {
    if (Net::isConnected())        return 1;
    if (WiFi.getMode() & WIFI_AP)  return 2;
    return 0;
}

static uint16_t wifiColor(uint8_t s) {
    switch (s) {
    case 1: return Color::WIFI_OK;
    case 2: return Color::WIFI_RETRY;
    default: return Color::WIFI_FAIL;
    }
}

static uint8_t syncIcon() {
    // 0 = error/never, 1 = synced recently, 2 = pending events queued
    uint32_t now = millis();
    if (EventQueue::depth() > 0) return 2;
    if (Backend::lastSyncMs() == 0) return 0;
    if (now - Backend::lastSyncMs() > 30UL * 60UL * 1000UL) return 0;
    return 1;
}

static void drawStatusStrip(bool full = false) {
    using namespace Layout;
    char clock[6];
    if (TimeSync::isSynced()) TimeSync::formatHM(clock, sizeof(clock));
    else                      strcpy(clock, "--:--");

    uint8_t ws  = wifiState();
    uint8_t si  = syncIcon();

    bool clockChanged = strcmp(clock, s_lastClock) != 0;
    bool wifiChanged  = ws != s_lastWifiState;
    bool syncChanged  = si != s_lastSyncIcon;
    if (!full && !clockChanged && !wifiChanged && !syncChanged) return;

    s_tft->fillRect(0, STATUS_Y, W, STATUS_H, Color::BG);

    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::BG);
        s_tft->drawString(clock, 8, STATUS_Y + STATUS_H / 2);

        // Title in middle: depending on current screen.
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(Color::DIM, Color::BG);
        s_tft->drawString(s_screen == Screen::Today ? "Today" : "Last 14 days",
                          W / 2, STATUS_Y + STATUS_H / 2);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::BG);
        s_tft->drawString(clock, 8, STATUS_Y + STATUS_H / 2, 2);
    }

    // WiFi dot (right edge)
    int dotX = W - 14;
    int dotY = STATUS_Y + STATUS_H / 2;
    s_tft->fillCircle(dotX, dotY, 4, wifiColor(ws));

    // Sync chip (left of WiFi dot)
    uint16_t sc = (si == 0) ? Color::WIFI_FAIL : (si == 2 ? Color::WIFI_RETRY : Color::WIFI_OK);
    s_tft->fillCircle(dotX - 14, dotY, 3, sc);

    strcpy(s_lastClock, clock);
    s_lastWifiState = ws;
    s_lastSyncIcon  = si;
}

void begin(TFT_eSPI* tft) {
    s_tft = tft;
    PageToday::begin(tft);
    PageStats14::begin(tft);
    ReminderPopup::begin(tft);

    // Wire popup hooks into Reminders so it can drive the popup without
    // taking a hard dependency on this module.
    Reminders::setShowPopup(showReminderPopup);
    Reminders::setHidePopup(hideReminderPopup);
    Reminders::setPopupVisible(reminderPopupVisible);
}

void renderAll() {
    if (!s_tft) return;
    s_tft->fillScreen(Color::BG);
    drawStatusStrip(/*full=*/true);
    if (s_screen == Screen::Today) PageToday::render();
    else                           PageStats14::render();
    if (ReminderPopup::isVisible()) {
        // Re-paint popup on top of fresh background.
        ReminderPopup::show(ReminderPopup::currentHabit());
    }
}

void showToday()   { s_screen = Screen::Today;   s_lastCycleMs = millis(); renderAll(); }
void showStats14() { s_screen = Screen::Stats14; s_lastCycleMs = millis(); renderAll(); }

void showReminderPopup(Config::Habit h) {
    ReminderPopup::show(h);
}

void hideReminderPopup() {
    ReminderPopup::hide();
    renderAll();
}

bool reminderPopupVisible() { return ReminderPopup::isVisible(); }

void tick() {
    uint32_t now = millis();

    // Status strip every second.
    if (now - s_lastStripMs >= 1000) {
        s_lastStripMs = now;
        drawStatusStrip();
    }

    if (ReminderPopup::isVisible()) {
        // Refresh subtitle (overdue counter).
        if (now - s_lastSubMs >= 5000) {
            s_lastSubMs = now;
            ReminderPopup::refreshSubtitle();
        }
        // Auto-snooze on timeout.
        if (ReminderPopup::shownForMs() >= Defaults::POPUP_AUTOSNOOZE_MS) {
            Reminders::onSnooze(ReminderPopup::currentHabit());
        }
        return;   // pause carousel while popup is up
    }

    // Auto-rotate Today ↔ Stats14
    if (now - s_lastCycleMs >= Defaults::CAROUSEL_MS) {
        s_lastCycleMs = now;
        s_screen = (s_screen == Screen::Today) ? Screen::Stats14 : Screen::Today;
        renderAll();
    }
}

void onTouch(const Touch::Event& ev) {
    if (ReminderPopup::isVisible()) {
        switch (ReminderPopup::handleTap(ev.x, ev.y)) {
        case ReminderPopup::TapAction::Done:
            Reminders::onDone(ReminderPopup::currentHabit());
            break;
        case ReminderPopup::TapAction::Snooze:
            Reminders::onSnooze(ReminderPopup::currentHabit());
            break;
        case ReminderPopup::TapAction::Skip:
            Reminders::onSkip(ReminderPopup::currentHabit());
            break;
        case ReminderPopup::TapAction::None:
            break;
        }
        return;
    }

    // Tap toggles the carousel; reset the auto-cycle timer.
    if (s_screen == Screen::Today) showStats14();
    else                           showToday();
}

}  // namespace ScreenManager

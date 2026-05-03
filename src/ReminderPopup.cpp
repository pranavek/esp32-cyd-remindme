#include "ReminderPopup.h"
#include "Settings.h"
#include "TimeSync.h"
#include "Bmp.h"

#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>

namespace ReminderPopup {

static TFT_eSPI*     s_tft = nullptr;
static bool          s_visible = false;
static Config::Habit s_habit = Config::Habit::Water;
static uint32_t      s_shownAtMs = 0;

static const char* titleFor(Config::Habit h) {
    switch (h) {
    case Config::Habit::Stretch: return "Time to stretch";
    case Config::Habit::Water:   return "Drink some water";
    case Config::Habit::Walk:    return "Time to walk";
    }
    return "Take a break";
}

static const char* iconFor(Config::Habit h) {
    switch (h) {
    case Config::Habit::Stretch: return "/icons/stretch.bmp";
    case Config::Habit::Water:   return "/icons/water.bmp";
    case Config::Habit::Walk:    return "/icons/walk.bmp";
    }
    return "";
}

static uint16_t accentFor(Config::Habit h) {
    switch (h) {
    case Config::Habit::Stretch: return Color::STRETCH;
    case Config::Habit::Water:   return Color::WATER;
    case Config::Habit::Walk:    return Color::WALK;
    }
    return Color::ACCENT;
}

static void drawButton(int x, const char* label, uint16_t fill, uint16_t fg) {
    using namespace Layout;
    s_tft->fillRoundRect(x, POP_BTN_Y, POP_BTN_W, POP_BTN_H, 8, fill);
    s_tft->drawRoundRect(x, POP_BTN_Y, POP_BTN_W, POP_BTN_H, 8, Color::FG);
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(fg, fill);
        s_tft->drawString(label, x + POP_BTN_W / 2, POP_BTN_Y + POP_BTN_H / 2);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(fg, fill);
        s_tft->drawString(label, x + POP_BTN_W / 2, POP_BTN_Y + POP_BTN_H / 2, 2);
    }
}

static void drawHero() {
    using namespace Layout;

    // Try the BMP icon first; if missing, render a labelled rounded square so
    // the popup is still readable.
    if (Bmp::draw(*s_tft, iconFor(s_habit), POP_ICON_X, POP_ICON_Y)) return;

    s_tft->fillRoundRect(POP_ICON_X, POP_ICON_Y, POP_ICON, POP_ICON, 10, Color::CARD);
    s_tft->drawRoundRect(POP_ICON_X, POP_ICON_Y, POP_ICON, POP_ICON, 10, accentFor(s_habit));
    if (LittleFS.exists(String(FONT_LARGE) + ".vlw")) {
        s_tft->loadFont(FONT_LARGE, LittleFS);
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(accentFor(s_habit), Color::CARD);
        const char* glyph = (s_habit == Config::Habit::Water)   ? "W"
                          : (s_habit == Config::Habit::Walk)    ? "K"
                                                                : "S";
        s_tft->drawString(glyph, POP_ICON_X + POP_ICON / 2, POP_ICON_Y + POP_ICON / 2);
        s_tft->unloadFont();
    }
}

void show(Config::Habit habit) {
    if (!s_tft) return;
    s_visible    = true;
    s_habit      = habit;
    s_shownAtMs  = millis();

    using namespace Layout;
    s_tft->fillRoundRect(POP_X, POP_Y, POP_W, POP_H, 14, Color::CARD);
    s_tft->drawRoundRect(POP_X, POP_Y, POP_W, POP_H, 14, accentFor(habit));

    drawHero();

    // Title to the right of the icon.
    int textX = POP_ICON_X + POP_ICON + 14;
    int textY = POP_ICON_Y + POP_ICON / 2;
    if (LittleFS.exists(String(FONT_LARGE) + ".vlw")) {
        s_tft->loadFont(FONT_LARGE, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(titleFor(habit), textX, textY);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(titleFor(habit), textX, textY, 4);
    }

    refreshSubtitle();

    drawButton(POP_BTN_X0, "Done",   accentFor(habit),  Color::ACCENT_TXT);
    drawButton(POP_BTN_X1, "Snooze", Color::BTN_FILL,   Color::FG);
    drawButton(POP_BTN_X2, "Skip",   Color::BTN_FILL,   Color::FG);
}

void hide() {
    s_visible = false;
}

bool isVisible() { return s_visible; }
Config::Habit currentHabit() { return s_habit; }

void refreshSubtitle() {
    if (!s_visible || !s_tft) return;
    using namespace Layout;
    int subX = POP_X + 16;
    int subY = POP_BTN_Y - 22;
    int subW = POP_W - 32;
    int subH = 18;

    s_tft->fillRect(subX, subY, subW, subH, Color::CARD);

    char hm[6];
    if (TimeSync::isSynced()) TimeSync::formatHM(hm, sizeof(hm));
    else                      strcpy(hm, "--:--");

    char line[40];
    uint32_t shownMs = millis() - s_shownAtMs;
    int sec = shownMs / 1000UL;
    if (sec < 60) snprintf(line, sizeof(line), "%s · just now", hm);
    else          snprintf(line, sizeof(line), "%s · %dm overdue", hm, sec / 60);

    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(line, subX, subY + subH / 2);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(line, subX, subY + subH / 2, 2);
    }
}

TapAction handleTap(int x, int y) {
    if (!s_visible) return TapAction::None;
    using namespace Layout;
    if (y < POP_BTN_Y || y >= POP_BTN_Y + POP_BTN_H) return TapAction::None;
    if (x >= POP_BTN_X0 && x < POP_BTN_X0 + POP_BTN_W) return TapAction::Done;
    if (x >= POP_BTN_X1 && x < POP_BTN_X1 + POP_BTN_W) return TapAction::Snooze;
    if (x >= POP_BTN_X2 && x < POP_BTN_X2 + POP_BTN_W) return TapAction::Skip;
    return TapAction::None;
}

uint32_t shownForMs() { return s_visible ? (millis() - s_shownAtMs) : 0; }

void begin(TFT_eSPI* tft) { s_tft = tft; }

}  // namespace ReminderPopup

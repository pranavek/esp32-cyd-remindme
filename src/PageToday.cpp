#include "PageToday.h"
#include "Settings.h"
#include "Config.h"
#include "Reminders.h"
#include "Bmp.h"

#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>

namespace PageToday {

static TFT_eSPI* s_tft = nullptr;

struct Row {
    const char*    label;
    const char*    icon;
    Config::Habit  habit;
    uint16_t       (*goal)();
    uint16_t       (*done)();
    uint16_t       color;
    const char*    unit;
};

static uint16_t goalStretch() { return Config::current().goals.stretch_min; }
static uint16_t goalWater()   { return Config::current().goals.water_ml;    }
static uint16_t goalWalk()    { return Config::current().goals.walk_min;    }

static const Row ROWS[3] = {
    { "Stretch", "/icons-small/stretch.bmp", Config::Habit::Stretch, goalStretch, Reminders::todayDoneStretchMin, Color::STRETCH, "min" },
    { "Water",   "/icons-small/water.bmp",   Config::Habit::Water,   goalWater,   Reminders::todayDoneWaterMl,   Color::WATER,   "ml"  },
    { "Walk",    "/icons-small/walk.bmp",    Config::Habit::Walk,    goalWalk,    Reminders::todayDoneWalkMin,   Color::WALK,    "min" },
};

static int rowY(int i) {
    return Layout::CONTENT_Y + i * (Layout::ROW_H + Layout::ROW_GAP);
}

static void drawRow(int i) {
    using namespace Layout;
    int y = rowY(i);
    const Row& r = ROWS[i];

    // Card background.
    s_tft->fillRoundRect(4, y, W - 8, ROW_H, 8, Color::CARD);

    // Icon (24×24 from icons-small/) at left.
    if (!Bmp::draw(*s_tft, r.icon, 8 + 4, y + (ROW_H - 24) / 2)) {
        s_tft->fillRoundRect(8 + 4, y + (ROW_H - 24) / 2, 24, 24, 6, r.color);
    }

    // Label
    int labelX = 8 + 4 + 24 + 8;
    int labelY = y + 14;
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(r.label, labelX, labelY);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(r.label, labelX, labelY, 2);
    }

    // Bar
    int barX = labelX;
    int barY = y + 28;
    int barW = W - barX - 12;
    int barH = BAR_H;
    s_tft->fillRoundRect(barX, barY, barW, barH, 4, Color::BAR_BG);

    uint16_t goal = r.goal();
    uint16_t done = r.done();
    if (goal > 0) {
        int fillW = (int)((uint32_t)done * (uint32_t)barW / (uint32_t)goal);
        if (fillW > barW) fillW = barW;
        if (fillW > 0) s_tft->fillRoundRect(barX, barY, fillW, barH, 4, r.color);
    }

    // Numeric "x / y unit"
    char value[24];
    snprintf(value, sizeof(value), "%u / %u %s", (unsigned)done, (unsigned)goal, r.unit);
    int textY = y + ROW_H - 12;
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(MR_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(value, W - 12, textY);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(MR_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(value, W - 12, textY, 2);
    }
}

void begin(TFT_eSPI* tft) { s_tft = tft; }

void render() {
    if (!s_tft) return;
    s_tft->fillRect(0, Layout::CONTENT_Y, Layout::W, Layout::CONTENT_H, Color::BG);
    for (int i = 0; i < 3; ++i) drawRow(i);
}

void refresh() {
    // For now, full row repaint. Smooth fonts are slow but rows are sparse.
    if (!s_tft) return;
    for (int i = 0; i < 3; ++i) drawRow(i);
}

}  // namespace PageToday

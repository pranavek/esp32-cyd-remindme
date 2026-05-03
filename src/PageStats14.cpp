#include "PageStats14.h"
#include "Settings.h"
#include "Config.h"
#include "Stats.h"

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

namespace PageStats14 {

static TFT_eSPI* s_tft = nullptr;

struct Row {
    const char* label;
    const char* json_key;     // key inside the cached 14d JSON
    uint16_t    color;
};

static const Row ROWS[3] = {
    { "Stretch", "stretch_min", Color::STRETCH },
    { "Water",   "water_ml",    Color::WATER   },
    { "Walk",    "walk_min",    Color::WALK    },
};

static int rowY(int i) {
    return Layout::CONTENT_Y + i * (Layout::ROW_H + Layout::ROW_GAP);
}

// Read cached 14d data into per-row arrays. Returns the max value across all
// 14 days for normalisation.
static void readRowData(const Row& r, int values[14], int& maxOut) {
    for (int i = 0; i < 14; ++i) values[i] = 0;
    maxOut = 0;

    JsonDocument doc;
    if (!Stats::read14dCache(doc)) return;

    JsonArray arr = doc["days"].as<JsonArray>();
    if (arr.isNull()) return;

    int n = arr.size();
    int start = (n > 14) ? n - 14 : 0;
    for (int i = start, di = 14 - (n - start); i < n && di < 14; ++i, ++di) {
        JsonObject day = arr[i];
        int v = day[r.json_key] | 0;
        values[di] = v;
        if (v > maxOut) maxOut = v;
    }
}

static void drawRow(int i) {
    using namespace Layout;
    int y = rowY(i);
    const Row& r = ROWS[i];

    s_tft->fillRoundRect(4, y, W - 8, ROW_H, 8, Color::CARD);

    // Label
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(r.label, 12, y + ROW_H / 2);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(r.label, 12, y + ROW_H / 2, 2);
    }

    int values[14]; int vmax = 0;
    readRowData(r, values, vmax);

    // Bars
    int x0 = STATS_BAR_X;
    int yBase = y + ROW_H - 8;
    int hMax = STATS_BAR_MAX_H;

    for (int b = 0; b < STATS_BARS; ++b) {
        int bx = x0 + b * (STATS_BAR_W + STATS_BAR_GAP);
        int bh = (vmax > 0) ? (values[b] * hMax / vmax) : 0;
        if (bh < 1 && values[b] > 0) bh = 1;
        // Background rail.
        s_tft->drawFastVLine(bx + STATS_BAR_W / 2, yBase - hMax, hMax, Color::BAR_BG);
        // Filled bar.
        if (bh > 0) {
            uint16_t c = (b == STATS_BARS - 1) ? Color::ACCENT : r.color;
            s_tft->fillRect(bx, yBase - bh, STATS_BAR_W, bh, c);
        }
    }
}

void begin(TFT_eSPI* tft) { s_tft = tft; }

void render() {
    if (!s_tft) return;
    s_tft->fillRect(0, Layout::CONTENT_Y, Layout::W, Layout::CONTENT_H, Color::BG);
    for (int i = 0; i < 3; ++i) drawRow(i);
}

}  // namespace PageStats14

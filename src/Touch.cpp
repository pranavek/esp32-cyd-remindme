// Touch driver for the dual-USB CYD (ESP32-2432S028R, ST7789 panel).
//
// The XPT2046 touch chip is on its own SPI bus (not the HSPI bus the display
// uses). TFT_eSPI's built-in touch only works when both share a bus, so we
// drive XPT2046 directly via the global SPI object (re-pinned at begin()).
//
//   T_CS=33  T_IRQ=36  T_CLK=25  T_DIN(MOSI)=32  T_OUT(MISO)=39
//
// Calibration is a 4-corner UI; result is 5 × uint16_t in /cal.json.

#include "Touch.h"
#include "Settings.h"

#include <FS.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

namespace Touch {

static constexpr int T_CS   = 33;
static constexpr int T_IRQ  = 36;
static constexpr int T_CLK  = 25;
static constexpr int T_MOSI = 32;
static constexpr int T_MISO = 39;

static XPT2046_Touchscreen s_ts(T_CS, T_IRQ);
static TFT_eSPI*           s_tft = nullptr;
static const char*         CAL_PATH = "/cal.json";

struct Cal {
    uint16_t xMin = 350, xMax = 3700;
    uint16_t yMin = 350, yMax = 3700;
    uint16_t swap = 1;
} s_cal;

static bool     s_pressing      = false;
static uint32_t s_pressStartMs  = 0;
static int      s_lastX = 0, s_lastY = 0;

static bool loadCal() {
    if (!LittleFS.exists(CAL_PATH)) return false;
    fs::File f = LittleFS.open(CAL_PATH, "r");
    if (!f) return false;
    uint16_t buf[5] = {0};
    size_t got = f.read((uint8_t*)buf, sizeof(buf));
    f.close();
    if (got != sizeof(buf)) return false;
    s_cal.xMin = buf[0]; s_cal.xMax = buf[1];
    s_cal.yMin = buf[2]; s_cal.yMax = buf[3];
    s_cal.swap = buf[4];
    Serial.printf("[touch] cal loaded: x[%u..%u] y[%u..%u] swap=%u\n",
                  s_cal.xMin, s_cal.xMax, s_cal.yMin, s_cal.yMax, s_cal.swap);
    return true;
}

static bool saveCal() {
    fs::File f = LittleFS.open(CAL_PATH, "w");
    if (!f) return false;
    uint16_t buf[5] = {s_cal.xMin, s_cal.xMax, s_cal.yMin, s_cal.yMax, s_cal.swap};
    size_t n = f.write((const uint8_t*)buf, sizeof(buf));
    f.close();
    return n == sizeof(buf);
}

void resetCalibration() { LittleFS.remove(CAL_PATH); }

static void drawCross(int x, int y, uint16_t color) {
    s_tft->drawLine(x - 8, y, x + 8, y, color);
    s_tft->drawLine(x, y - 8, x, y + 8, color);
    s_tft->fillCircle(x, y, 2, color);
}

static void drawCalScreen(int step) {
    s_tft->fillScreen(Color::BG);

    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(Color::FG, Color::BG);
        s_tft->drawString("Touch the cross",
                          s_tft->width() / 2, s_tft->height() / 2 - 14);
        char hint[16];
        snprintf(hint, sizeof(hint), "%d / 4", step + 1);
        s_tft->setTextColor(Color::DIM, Color::BG);
        s_tft->drawString(hint, s_tft->width() / 2, s_tft->height() / 2 + 12);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(Color::FG, Color::BG);
        s_tft->drawString("Touch the cross",
                          s_tft->width() / 2, s_tft->height() / 2, 2);
    }
}

static void waitForCornerTouch(uint16_t& outRawX, uint16_t& outRawY) {
    while (!s_ts.touched()) delay(10);
    uint32_t sumX = 0, sumY = 0, n = 0;
    while (s_ts.touched()) {
        TS_Point p = s_ts.getPoint();
        if (p.z > 200) { sumX += p.x; sumY += p.y; ++n; }
        delay(10);
    }
    outRawX = (uint16_t)(n ? sumX / n : 0);
    outRawY = (uint16_t)(n ? sumY / n : 0);
    delay(250);
}

void runCalibration() {
    if (!s_tft) return;

    // Corners in screen-space for landscape (320×240). 10 px in from edges.
    struct Corner { int sx, sy; uint16_t rx, ry; } c[4] = {
        {  10,  10, 0, 0 },   // TL
        { 310,  10, 0, 0 },   // TR
        { 310, 230, 0, 0 },   // BR
        {  10, 230, 0, 0 },   // BL
    };

    for (int i = 0; i < 4; ++i) {
        drawCalScreen(i);
        drawCross(c[i].sx, c[i].sy, Color::ACCENT);
        waitForCornerTouch(c[i].rx, c[i].ry);
        drawCross(c[i].sx, c[i].sy, Color::OK_FLASH);
        delay(120);
    }

    uint32_t dxRawHoriz = abs((int)c[1].rx - (int)c[0].rx)
                       +  abs((int)c[2].rx - (int)c[3].rx);
    uint32_t dxRawVert  = abs((int)c[3].rx - (int)c[0].rx)
                       +  abs((int)c[2].rx - (int)c[1].rx);
    s_cal.swap = (dxRawHoriz < dxRawVert) ? 1 : 0;

    if (s_cal.swap) {
        s_cal.xMin = (c[0].ry + c[3].ry) / 2;
        s_cal.xMax = (c[1].ry + c[2].ry) / 2;
        s_cal.yMin = (c[0].rx + c[1].rx) / 2;
        s_cal.yMax = (c[2].rx + c[3].rx) / 2;
    } else {
        s_cal.xMin = (c[0].rx + c[3].rx) / 2;
        s_cal.xMax = (c[1].rx + c[2].rx) / 2;
        s_cal.yMin = (c[0].ry + c[1].ry) / 2;
        s_cal.yMax = (c[2].ry + c[3].ry) / 2;
    }

    Serial.printf("[touch] cal complete: x[%u..%u] y[%u..%u] swap=%u\n",
                  s_cal.xMin, s_cal.xMax, s_cal.yMin, s_cal.yMax, s_cal.swap);

    saveCal();

    s_tft->fillScreen(Color::BG);
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(MC_DATUM);
        s_tft->setTextColor(Color::WIFI_OK, Color::BG);
        s_tft->drawString("calibration saved",
                          s_tft->width() / 2, s_tft->height() / 2);
        s_tft->unloadFont();
    }
    delay(700);
}

void begin(TFT_eSPI* tft) {
    s_tft = tft;
    SPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
    s_ts.begin();
    s_ts.setRotation(0);

    if (loadCal()) {
        Serial.println("[touch] using stored calibration");
    } else {
        Serial.println("[touch] no calibration — running 4-corner setup");
        runCalibration();
    }
}

static void rawToScreen(uint16_t rx, uint16_t ry, int& sx, int& sy) {
    int rawForX = s_cal.swap ? ry : rx;
    int rawForY = s_cal.swap ? rx : ry;

    int spanX = (int)s_cal.xMax - (int)s_cal.xMin;
    int spanY = (int)s_cal.yMax - (int)s_cal.yMin;
    if (spanX == 0) spanX = 1;
    if (spanY == 0) spanY = 1;

    int W = s_tft ? s_tft->width()  : Layout::W;
    int H = s_tft ? s_tft->height() : Layout::H;

    long mx = (long)(rawForX - (int)s_cal.xMin) * W / spanX;
    long my = (long)(rawForY - (int)s_cal.yMin) * H / spanY;

    if (mx < 0) mx = 0; else if (mx >= W) mx = W - 1;
    if (my < 0) my = 0; else if (my >= H) my = H - 1;
    sx = (int)mx;
    sy = (int)my;
}

bool isPressed() {
    if (!s_ts.touched()) return false;
    TS_Point p = s_ts.getPoint();
    if (p.z < 200) return false;
    rawToScreen(p.x, p.y, s_lastX, s_lastY);
    return true;
}

bool tick(Event& out) {
    uint32_t now = millis();
    bool pressed = false;

    if (s_ts.touched()) {
        TS_Point p = s_ts.getPoint();
        if (p.z >= 200) {
            rawToScreen(p.x, p.y, s_lastX, s_lastY);
            pressed = true;
        }
    }

    if (pressed) {
        if (!s_pressing) {
            s_pressing     = true;
            s_pressStartMs = now;
        }
        return false;
    }

    if (s_pressing) {
        s_pressing    = false;
        out.x         = s_lastX;
        out.y         = s_lastY;
        out.durMs     = now - s_pressStartMs;
        out.longPress = out.durMs >= LONG_PRESS_MS;
        return true;
    }
    return false;
}

}  // namespace Touch

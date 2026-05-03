#include "Backlight.h"
#include "Settings.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace Backlight {

static TFT_eSPI* s_tft = nullptr;
static uint32_t  s_lastSampleMs = 0;
static float     s_ema = 0.f;
static bool      s_haveSample = false;
static bool      s_asleep = false;
static uint8_t   s_currentLevel = BRIGHT_CEIL;

static uint32_t s_lastTickMs = 0;
static uint32_t s_litMsAccum = 0;

void set(uint8_t level) {
    s_currentLevel = level;
    ledcWrite(PwmCh::BACKLIGHT, level);
}

void wake() {
    if (!s_asleep) return;
    s_asleep = false;
    if (s_tft) s_tft->writecommand(0x29);   // ST7789 DISPON
    set(s_currentLevel);
}

void sleep() {
    if (s_asleep) return;
    s_asleep = true;
    set(0);
    if (s_tft) s_tft->writecommand(0x28);   // ST7789 DISPOFF
}

void begin(TFT_eSPI* tft) {
    s_tft = tft;
    pinMode(Pin::LDR, INPUT);
    ledcSetup(PwmCh::BACKLIGHT, 5000, 8);
    ledcAttachPin(Pin::BACKLIGHT, PwmCh::BACKLIGHT);
    set(BRIGHT_CEIL);
    s_lastTickMs = millis();
}

void tick() {
    uint32_t now = millis();
    uint32_t dt = now - s_lastTickMs;
    s_lastTickMs = now;
    if (!s_asleep) s_litMsAccum += dt;

    if (now - s_lastSampleMs < (uint32_t)LDR_SAMPLE_MS) return;
    s_lastSampleMs = now;

    int raw = analogRead(Pin::LDR);
    if (!s_haveSample) {
        s_ema = (float)raw;
        s_haveSample = true;
    } else {
        s_ema = s_ema * (1.0f - LDR_EMA_ALPHA) + (float)raw * LDR_EMA_ALPHA;
    }

    int rawEma = (int)s_ema;
    if (!s_asleep && rawEma > LDR_SLEEP_RAW) sleep();
    else if (s_asleep && rawEma < LDR_WAKE_RAW) wake();

    if (!s_asleep) {
        int level = map(rawEma, 0, 4095, BRIGHT_CEIL, BRIGHT_FLOOR);
        if (level < BRIGHT_FLOOR) level = BRIGHT_FLOOR;
        if (level > BRIGHT_CEIL)  level = BRIGHT_CEIL;
        set((uint8_t)level);
    }
}

bool     screenAsleep() { return s_asleep; }
int      ldrEma()       { return s_haveSample ? (int)s_ema : 0; }
uint32_t litMs()        { return s_litMsAccum; }

}  // namespace Backlight

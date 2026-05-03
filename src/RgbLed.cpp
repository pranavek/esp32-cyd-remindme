#include "RgbLed.h"
#include "Settings.h"

#include <Arduino.h>
#include <math.h>

namespace RgbLed {

static State    s_state = State::OFF;
static State    s_prevState = State::OFF;
static uint32_t s_stateStartMs = 0;

static int (*s_ldrFn)() = nullptr;
void setLdrSource(int (*fn)()) { s_ldrFn = fn; }

static void apply(uint8_t r, uint8_t g, uint8_t b) {
    if (s_ldrFn) {
        int raw = s_ldrFn();
        if (raw > 3000) {
            r = (uint8_t)((int)r * 30 / 100);
            g = (uint8_t)((int)g * 30 / 100);
            b = (uint8_t)((int)b * 30 / 100);
        }
    }
    // Active LOW.
    ledcWrite(PwmCh::LED_R, 255 - r);
    ledcWrite(PwmCh::LED_G, 255 - g);
    ledcWrite(PwmCh::LED_B, 255 - b);
}

void begin() {
    pinMode(Pin::LED_R, OUTPUT);
    pinMode(Pin::LED_G, OUTPUT);
    pinMode(Pin::LED_B, OUTPUT);
    ledcSetup(PwmCh::LED_R, 5000, 8);
    ledcSetup(PwmCh::LED_G, 5000, 8);
    ledcSetup(PwmCh::LED_B, 5000, 8);
    ledcAttachPin(Pin::LED_R, PwmCh::LED_R);
    ledcAttachPin(Pin::LED_G, PwmCh::LED_G);
    ledcAttachPin(Pin::LED_B, PwmCh::LED_B);
    apply(0, 0, 0);
    s_state = State::OFF;
    s_stateStartMs = millis();
}

void set(State s) {
    if (s == s_state) return;
    bool isFlash = (s == State::DONE_FLASH || s == State::SNOOZE_FLASH || s == State::SKIP_FLASH);
    if (isFlash) {
        bool wasFlash = (s_state == State::DONE_FLASH || s_state == State::SNOOZE_FLASH || s_state == State::SKIP_FLASH);
        if (!wasFlash) s_prevState = s_state;
    } else {
        s_prevState = s;
    }
    s_state = s;
    s_stateStartMs = millis();
}

State current() { return s_state; }
void  doneFlash()   { set(State::DONE_FLASH); }
void  snoozeFlash() { set(State::SNOOZE_FLASH); }
void  skipFlash()   { set(State::SKIP_FLASH); }

static uint8_t sinePulse(uint32_t elapsed, uint32_t periodMs, uint8_t peak) {
    float phase = (elapsed % periodMs) / (float)periodMs * 2.0f * (float)PI;
    float v = 0.5f * (1.0f - cosf(phase));
    return (uint8_t)(v * peak);
}

void tick() {
    uint32_t now = millis();
    uint32_t elapsed = now - s_stateStartMs;

    switch (s_state) {
    case State::OFF:
        apply(0, 0, 0);
        break;

    case State::REMINDER_PULSE: {
        uint8_t b = sinePulse(elapsed, 2000, 200);
        apply(0, 0, b);
        break;
    }
    case State::OVERDUE_PULSE: {
        uint8_t v = sinePulse(elapsed, 1000, 220);
        apply(v, (uint8_t)(v * 60 / 100), 0);
        break;
    }
    case State::DONE_FLASH:
        if (elapsed > 600) { set(State::OFF); break; }
        apply(0, 220, 0);
        break;

    case State::SNOOZE_FLASH:
        if (elapsed > 200) { set(State::OFF); break; }
        apply(0, 200, 220);
        break;

    case State::SKIP_FLASH:
        if (elapsed > 200) { set(State::OFF); break; }
        apply(220, 0, 0);
        break;

    case State::WIFI_SETUP: {
        uint8_t v = sinePulse(elapsed, 3000, 180);
        apply(v, 0, v);
        break;
    }
    case State::WIFI_LOST_BLIP: {
        uint32_t cycle = elapsed % 30000;
        apply((cycle < 80) ? 220 : 0, 0, 0);
        break;
    }
    case State::BACKEND_ERROR: {
        uint8_t v = sinePulse(elapsed, 4000, 100);
        apply(v, 0, 0);
        break;
    }
    }
}

}  // namespace RgbLed

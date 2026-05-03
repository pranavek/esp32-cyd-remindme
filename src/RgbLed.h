#pragma once

#include <Arduino.h>

namespace RgbLed {

enum class State : uint8_t {
    OFF,
    REMINDER_PULSE,     // gentle blue sine pulse while popup is open
    OVERDUE_PULSE,      // faster amber pulse if user lets popup linger >30s
    DONE_FLASH,         // 600 ms green
    SNOOZE_FLASH,       // 200 ms cyan
    SKIP_FLASH,         // 200 ms red
    WIFI_SETUP,         // slow magenta breathing (portal active)
    WIFI_LOST_BLIP,     // brief red blip every 30 s
    BACKEND_ERROR,      // slow red breathing (queue draining failures)
};

void begin();
void tick();
void set(State s);
State current();

void doneFlash();
void snoozeFlash();
void skipFlash();

void setLdrSource(int (*ldrEmaFn)());

}  // namespace RgbLed

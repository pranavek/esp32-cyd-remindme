#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace Touch {

struct Event {
    int      x, y;
    uint32_t durMs;
    bool     longPress;
};

constexpr uint32_t LONG_PRESS_MS = 800;

void begin(TFT_eSPI* tft);
bool tick(Event& out);
bool isPressed();
void runCalibration();
void resetCalibration();

}  // namespace Touch

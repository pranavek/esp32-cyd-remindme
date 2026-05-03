#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace Backlight {

void begin(TFT_eSPI* tft);
void tick();
void set(uint8_t level);
void sleep();
void wake();
bool screenAsleep();
int  ldrEma();
uint32_t litMs();

}  // namespace Backlight

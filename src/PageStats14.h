#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace PageStats14 {

void begin(TFT_eSPI* tft);
void render();

}  // namespace PageStats14

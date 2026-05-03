#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace PageToday {

void begin(TFT_eSPI* tft);
void render();           // full repaint
void refresh();          // partial repaint (only progress bars + values)

}  // namespace PageToday

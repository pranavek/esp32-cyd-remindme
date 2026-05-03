#pragma once

#include <Arduino.h>

class TFT_eSPI;

namespace Bmp {

// Stream a 24-bit uncompressed BMP from LittleFS to the display at (x,y).
// Returns false if the file is missing, malformed, or not 24-bit. Caller is
// responsible for clearing the underlying region first if needed.
bool draw(TFT_eSPI& tft, const char* path, int x, int y);

}  // namespace Bmp

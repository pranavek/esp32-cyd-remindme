#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class TFT_eSPI;

namespace Diagnostics {

void begin(TFT_eSPI* tft);

// Long-press handler — toggles the on-screen overlay.
void toggleOverlay();
bool overlayVisible();
void hideOverlay();

// Repaint while overlay is up.
void renderOverlay();

// Populate `out` with a diagnostic snapshot: WiFi, mDNS, queue depth,
// last sync, free heap, fw version, uptime. Used by /api/state.
void snapshot(JsonObject out);

}  // namespace Diagnostics

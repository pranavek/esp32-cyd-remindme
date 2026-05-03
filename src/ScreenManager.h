#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Touch.h"

class TFT_eSPI;

namespace ScreenManager {

enum class Screen : uint8_t { Today, Stats14 };

void begin(TFT_eSPI* tft);

// Repaint the current screen + status strip.
void renderAll();

// Per-loop tick: handles auto-cycling between Today ↔ Stats14, popup
// auto-snooze timeout, status-strip refresh.
void tick();

// Override the carousel — UI calls these when the user taps to cycle.
void showToday();
void showStats14();

// Popup hooks (wired into Reminders).
void showReminderPopup(Config::Habit h);
void hideReminderPopup();
bool reminderPopupVisible();

// Touch dispatch: call from main when Touch::tick fires.
void onTouch(const Touch::Event& ev);

}  // namespace ScreenManager

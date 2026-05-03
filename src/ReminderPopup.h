#pragma once

#include <Arduino.h>
#include "Config.h"

class TFT_eSPI;

namespace ReminderPopup {

enum class TapAction : uint8_t { None, Done, Snooze, Skip };

void begin(TFT_eSPI* tft);

// Show the popup for the given habit. Cycles the icon/title accordingly.
void show(Config::Habit habit);
void hide();
bool isVisible();
Config::Habit currentHabit();

// Per-second redraw of the small "x m overdue" subtitle.
void refreshSubtitle();

// Returns Done / Snooze / Skip if a tap landed on a button.
TapAction handleTap(int x, int y);

// Time since the popup opened (ms). Used by the auto-snooze timer.
uint32_t shownForMs();

}  // namespace ReminderPopup

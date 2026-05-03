#pragma once

#include <Arduino.h>
#include "Config.h"

namespace Reminders {

// Initialise scheduler state from persisted DailyState.
void begin();

// Poll roughly once per second from the main loop. Decides whether a
// reminder should fire and updates the screen manager / RGB LED. Should be
// called regardless of carousel state — it's a noop when a popup is already
// up.
void tick();

// Hooks called from the popup when the user taps a button.
void onDone(Config::Habit habit, uint16_t amount = 0);
void onSnooze(Config::Habit habit);
void onSkip(Config::Habit habit);

// True if quiet hours are currently active per the config + local clock.
bool inQuietHours();

// Which habit is currently armed (most-overdue), if any. Used by the popup
// to know which icon/label to show.
bool currentlyArmed(Config::Habit& out, uint32_t& dueSecondsAgo);

// Force-fire a habit reminder right now. Used by the web UI's "test
// reminder" button and by the boot sequence on the very first run when
// time isn't synced yet.
void forceFire(Config::Habit habit);

// Convenience getters for the UI / diagnostics.
uint16_t todayDoneStretchMin();
uint16_t todayDoneWaterMl();
uint16_t todayDoneWalkMin();

// Wire screen-manager callbacks. Avoids a hard include cycle between
// Reminders and ScreenManager: ScreenManager registers its show/hide/visible
// hooks here at boot, and Reminders calls them when it fires/clears popups.
void setShowPopup(void (*fn)(Config::Habit));
void setHidePopup(void (*fn)());
void setPopupVisible(bool (*fn)());

}  // namespace Reminders

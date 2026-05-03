#include "Reminders.h"
#include "Settings.h"
#include "Config.h"
#include "Storage.h"
#include "EventQueue.h"
#include "Backend.h"
#include "TimeSync.h"
#include "RgbLed.h"

namespace Reminders {

// Forward decls into the screen manager — declared as namespace function
// pointers to avoid a circular include between Reminders and ScreenManager.
namespace ScreenHook {
    void (*showPopup)(Config::Habit) = nullptr;
    void (*hidePopup)()               = nullptr;
    bool (*popupVisible)()            = nullptr;
}

void setShowPopup(void (*fn)(Config::Habit)) { ScreenHook::showPopup = fn; }
void setHidePopup(void (*fn)())              { ScreenHook::hidePopup = fn; }
void setPopupVisible(bool (*fn)())           { ScreenHook::popupVisible = fn; }

// Last-fire timestamps live in DailyState (Storage) so they survive a
// reboot mid-day. We use TimeSync::nowLocal() (seconds since epoch) for
// these — easier to reason about across reboots than millis().

static Config::Habit s_armed = Config::Habit::Stretch;
static bool          s_haveArmed = false;
static uint32_t      s_armedSinceSec = 0;

static uint16_t intervalMinFor(Config::Habit h) {
    const auto& iv = Config::current().intervals;
    switch (h) {
    case Config::Habit::Stretch: return iv.stretch_min;
    case Config::Habit::Water:   return iv.water_min;
    case Config::Habit::Walk:    return iv.walk_min;
    }
    return 60;
}

static const char* habitName(Config::Habit h) {
    switch (h) {
    case Config::Habit::Stretch: return "stretch";
    case Config::Habit::Water:   return "water";
    case Config::Habit::Walk:    return "walk";
    }
    return "unknown";
}

bool inQuietHours() {
    const auto& q = Config::current().quiet;
    if (!q.enabled) return false;
    if (!TimeSync::isSynced()) return false;   // be conservative: fire normally
    int h = TimeSync::currentHour();
    if (q.start_hour == q.end_hour) return false;
    if (q.start_hour < q.end_hour) {
        return h >= q.start_hour && h < q.end_hour;
    }
    // Wraps midnight: e.g. 22..7 means 22..23 + 0..6
    return h >= q.start_hour || h < q.end_hour;
}

// One-shot: stamp any zero last-fire timestamps so the first reminder fires
// ~30 s after the clock first becomes available, then settles into normal
// cadence. Idempotent — only touches slots that are 0. Called from begin()
// and from tick() the first time TimeSync becomes ready.
static void seedFirstFires() {
    if (!TimeSync::isSynced()) return;
    Storage::DailyState st = Storage::today();
    uint32_t nowSec = (uint32_t)TimeSync::nowLocal();
    bool changed = false;
    auto seed = [&](uint32_t& slot, uint16_t intervalMin) {
        if (slot != 0) return;
        uint32_t intervalSec = (uint32_t)intervalMin * 60UL;
        slot = (nowSec > intervalSec) ? (nowSec - intervalSec + 30) : 30;
        changed = true;
    };
    seed(st.last_fire_stretch_ts, Config::current().intervals.stretch_min);
    seed(st.last_fire_water_ts,   Config::current().intervals.water_min);
    seed(st.last_fire_walk_ts,    Config::current().intervals.walk_min);
    if (changed) {
        Storage::setToday(st);
        Storage::flushToday();
    }
}

void begin() {
    Storage::resumeTodayFromDisk();
    seedFirstFires();   // best-effort; tick() retries until clock is up
}

bool currentlyArmed(Config::Habit& out, uint32_t& dueSecondsAgo) {
    if (!s_haveArmed) return false;
    out = s_armed;
    uint32_t now = TimeSync::isSynced() ? (uint32_t)TimeSync::nowLocal() : (uint32_t)(millis() / 1000UL);
    dueSecondsAgo = now > s_armedSinceSec ? now - s_armedSinceSec : 0;
    return true;
}

static uint32_t lastFireSecFor(const Storage::DailyState& s, Config::Habit h) {
    switch (h) {
    case Config::Habit::Stretch: return s.last_fire_stretch_ts;
    case Config::Habit::Water:   return s.last_fire_water_ts;
    case Config::Habit::Walk:    return s.last_fire_walk_ts;
    }
    return 0;
}

static void setLastFireSec(Storage::DailyState& s, Config::Habit h, uint32_t v) {
    switch (h) {
    case Config::Habit::Stretch: s.last_fire_stretch_ts = v; break;
    case Config::Habit::Water:   s.last_fire_water_ts   = v; break;
    case Config::Habit::Walk:    s.last_fire_walk_ts    = v; break;
    }
}

void tick() {
    static uint32_t s_lastTickMs = 0;
    uint32_t now = millis();
    if (now - s_lastTickMs < 1000) return;
    s_lastTickMs = now;

    if (ScreenHook::popupVisible && ScreenHook::popupVisible()) {
        // Popup already open — pulse stays on, scheduler defers.
        return;
    }
    if (inQuietHours()) {
        if (RgbLed::current() == RgbLed::State::REMINDER_PULSE
         || RgbLed::current() == RgbLed::State::OVERDUE_PULSE) {
            RgbLed::set(RgbLed::State::OFF);
        }
        return;
    }
    if (!TimeSync::isSynced()) return;   // wait for clock before firing anything

    seedFirstFires();   // no-op once seeded; cheap (state has 0/non-0 check)

    uint32_t nowSec = (uint32_t)TimeSync::nowLocal();
    Storage::DailyState st = Storage::today();

    // Find the most-overdue habit.
    Config::Habit pick = Config::Habit::Stretch;
    uint32_t mostOverdueSec = 0;
    bool anyDue = false;

    for (uint8_t i = 0; i < 3; ++i) {
        Config::Habit h = (Config::Habit)i;
        uint32_t lastSec = lastFireSecFor(st, h);
        if (lastSec == 0) continue;     // begin() seeds these; 0 means "wait for next sync"
        uint32_t intervalSec = (uint32_t)intervalMinFor(h) * 60UL;
        if (nowSec >= lastSec + intervalSec) {
            uint32_t overdue = nowSec - (lastSec + intervalSec);
            if (!anyDue || overdue > mostOverdueSec) {
                mostOverdueSec = overdue;
                pick = h;
                anyDue = true;
            }
        }
    }

    if (!anyDue) {
        if (RgbLed::current() == RgbLed::State::REMINDER_PULSE
         || RgbLed::current() == RgbLed::State::OVERDUE_PULSE) {
            RgbLed::set(RgbLed::State::OFF);
        }
        return;
    }

    s_armed       = pick;
    s_haveArmed   = true;
    s_armedSinceSec = nowSec - mostOverdueSec;

    // Mark fired in state & enqueue a "fired" event.
    setLastFireSec(st, pick, nowSec);
    st.reminders_due++;
    Storage::setToday(st);
    Storage::flushToday();

    EventQueue::enqueue(EventQueue::makeFired(habitName(pick)));
    Backend::poke();

    RgbLed::set(RgbLed::State::REMINDER_PULSE);
    if (ScreenHook::showPopup) ScreenHook::showPopup(pick);
    Serial.printf("[rem] fired %s (overdue %us)\n", habitName(pick), (unsigned)mostOverdueSec);
}

void forceFire(Config::Habit h) {
    if (ScreenHook::popupVisible && ScreenHook::popupVisible()) return;
    s_armed = h;
    s_haveArmed = true;
    s_armedSinceSec = TimeSync::isSynced() ? (uint32_t)TimeSync::nowLocal() : 0;
    EventQueue::enqueue(EventQueue::makeFired(habitName(h)));
    Backend::poke();
    RgbLed::set(RgbLed::State::REMINDER_PULSE);
    if (ScreenHook::showPopup) ScreenHook::showPopup(h);
}

// Map per-habit completion to today's accumulated counter increment.
static uint16_t increment(Config::Habit h, uint16_t amount) {
    if (amount > 0) return amount;
    switch (h) {
    case Config::Habit::Stretch: return 5;        // assume 5-min stretch
    case Config::Habit::Water:   return 250;      // one glass
    case Config::Habit::Walk:    return 10;       // 10-min walk
    }
    return 0;
}

void onDone(Config::Habit h, uint16_t amount) {
    Storage::DailyState st = Storage::today();
    uint16_t inc = increment(h, amount);
    switch (h) {
    case Config::Habit::Stretch: st.stretch_min = (uint16_t)min<int>(st.stretch_min + inc, 65535); break;
    case Config::Habit::Water:   st.water_ml    = (uint16_t)min<int>(st.water_ml    + inc, 65535); break;
    case Config::Habit::Walk:    st.walk_min    = (uint16_t)min<int>(st.walk_min    + inc, 65535); break;
    }
    st.reminders_done++;
    Storage::setToday(st);
    Storage::flushToday();

    char meta[32];
    snprintf(meta, sizeof(meta), "{\"amount\":%u}", (unsigned)inc);
    EventQueue::enqueue(EventQueue::makeCompleted(habitName(h), meta));
    Backend::poke();

    s_haveArmed = false;
    if (ScreenHook::hidePopup) ScreenHook::hidePopup();
    RgbLed::doneFlash();
}

void onSnooze(Config::Habit h) {
    // Roll last_fire forward so the next due is SNOOZE_MIN from now.
    Storage::DailyState st = Storage::today();
    uint32_t nowSec = (uint32_t)TimeSync::nowLocal();
    uint32_t intervalSec = (uint32_t)intervalMinFor(h) * 60UL;
    uint32_t snoozeSec   = (uint32_t)Defaults::SNOOZE_MIN * 60UL;
    setLastFireSec(st, h, nowSec - intervalSec + snoozeSec);
    st.reminders_snoozed++;
    Storage::setToday(st);
    Storage::flushToday();

    EventQueue::enqueue(EventQueue::makeSnoozed(habitName(h)));
    Backend::poke();

    s_haveArmed = false;
    if (ScreenHook::hidePopup) ScreenHook::hidePopup();
    RgbLed::snoozeFlash();
}

void onSkip(Config::Habit h) {
    Storage::DailyState st = Storage::today();
    st.reminders_skipped++;
    Storage::setToday(st);
    Storage::flushToday();

    EventQueue::enqueue(EventQueue::makeSkipped(habitName(h)));
    Backend::poke();

    s_haveArmed = false;
    if (ScreenHook::hidePopup) ScreenHook::hidePopup();
    RgbLed::skipFlash();
}

uint16_t todayDoneStretchMin() { return Storage::today().stretch_min; }
uint16_t todayDoneWaterMl()    { return Storage::today().water_ml;    }
uint16_t todayDoneWalkMin()    { return Storage::today().walk_min;    }

}  // namespace Reminders

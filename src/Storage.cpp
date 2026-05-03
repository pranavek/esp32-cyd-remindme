#include "Storage.h"
#include "Settings.h"
#include "TimeSync.h"

#include <FS.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace Storage {

static SemaphoreHandle_t s_mutex = nullptr;
static const char* PATH_STATE     = "/state.json";
static const char* PATH_STATE_TMP = "/state.tmp";

static DailyState s_state;
static uint32_t   s_lastFlushMs = 0;

struct Lock {
    Lock()  { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
    ~Lock() { if (s_mutex) xSemaphoreGive(s_mutex); }
};

void lock()   { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { if (s_mutex) xSemaphoreGive(s_mutex); }

// ─── Generic JSON file helpers ─────────────────────────────────────────────
bool readJson(const char* path, JsonDocument& doc) {
    Lock l;
    if (!LittleFS.exists(path)) return false;
    fs::File f = LittleFS.open(path, "r");
    if (!f) return false;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[storage] parse %s: %s\n", path, err.c_str());
        return false;
    }
    return true;
}

bool writeJson(const char* path, const JsonDocument& doc) {
    Lock l;
    String tmp = String(path) + ".tmp";
    fs::File f = LittleFS.open(tmp.c_str(), "w");
    if (!f) {
        Serial.printf("[storage] open tmp '%s' failed\n", tmp.c_str());
        return false;
    }
    if (serializeJson(doc, f) == 0) {
        f.close();
        Serial.printf("[storage] write 0 bytes to '%s'\n", tmp.c_str());
        return false;
    }
    f.close();
    LittleFS.remove(path);
    if (!LittleFS.rename(tmp.c_str(), path)) {
        Serial.printf("[storage] rename '%s' -> '%s' failed\n", tmp.c_str(), path);
        return false;
    }
    return true;
}

// ─── Config ────────────────────────────────────────────────────────────────
bool loadConfig(Config::Data& out) {
    Lock l;
    return Config::load(out);
}

bool saveConfig(const Config::Data& cfg) {
    Lock l;
    return Config::save(cfg);
}

// ─── State persistence ────────────────────────────────────────────────────
static void readStateInto(DailyState& s) {
    if (!LittleFS.exists(PATH_STATE)) return;
    fs::File f = LittleFS.open(PATH_STATE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[storage] state parse: %s\n", err.c_str());
        return;
    }
    JsonObject o = doc.as<JsonObject>();
    if (o.isNull()) return;

    const char* d = o["date"] | "";
    strncpy(s.date, d, sizeof(s.date) - 1);
    s.date[sizeof(s.date) - 1] = '\0';

    s.stretch_min = (uint16_t)(int)(o["stretch_min"] | 0);
    s.water_ml    = (uint16_t)(int)(o["water_ml"]    | 0);
    s.walk_min    = (uint16_t)(int)(o["walk_min"]    | 0);

    s.reminders_due     = (uint16_t)(int)(o["reminders_due"]     | 0);
    s.reminders_done    = (uint16_t)(int)(o["reminders_done"]    | 0);
    s.reminders_snoozed = (uint16_t)(int)(o["reminders_snoozed"] | 0);
    s.reminders_skipped = (uint16_t)(int)(o["reminders_skipped"] | 0);

    s.last_fire_stretch_ts = (uint32_t)(int64_t)(o["last_fire_stretch_ts"] | (int64_t)0);
    s.last_fire_water_ts   = (uint32_t)(int64_t)(o["last_fire_water_ts"]   | (int64_t)0);
    s.last_fire_walk_ts    = (uint32_t)(int64_t)(o["last_fire_walk_ts"]    | (int64_t)0);
}

DailyState today() {
    Lock l;
    return s_state;
}

void setToday(const DailyState& s) {
    Lock l;
    s_state = s;
}

bool flushToday() {
    Lock l;
    JsonDocument doc;
    JsonObject o = doc.to<JsonObject>();
    o["date"]                 = s_state.date;
    o["stretch_min"]          = s_state.stretch_min;
    o["water_ml"]             = s_state.water_ml;
    o["walk_min"]             = s_state.walk_min;
    o["reminders_due"]        = s_state.reminders_due;
    o["reminders_done"]       = s_state.reminders_done;
    o["reminders_snoozed"]    = s_state.reminders_snoozed;
    o["reminders_skipped"]    = s_state.reminders_skipped;
    o["last_fire_stretch_ts"] = (int64_t)s_state.last_fire_stretch_ts;
    o["last_fire_water_ts"]   = (int64_t)s_state.last_fire_water_ts;
    o["last_fire_walk_ts"]    = (int64_t)s_state.last_fire_walk_ts;

    fs::File f = LittleFS.open(PATH_STATE_TMP, "w");
    if (!f) return false;
    if (serializeJson(doc, f) == 0) { f.close(); return false; }
    f.close();
    LittleFS.remove(PATH_STATE);
    bool ok = LittleFS.rename(PATH_STATE_TMP, PATH_STATE);
    if (ok) s_lastFlushMs = millis();
    return ok;
}

void resetTodayCounters() {
    Lock l;
    s_state = DailyState{};
    if (TimeSync::isSynced()) TimeSync::formatYMD(s_state.date, sizeof(s_state.date));
}

void resumeTodayFromDisk() {
    Lock l;
    DailyState s;
    readStateInto(s);

    if (!TimeSync::isSynced()) {
        // Best effort: even without a clock we can resume counters; date
        // alignment will be checked on the first day-rollover tick.
        s_state = s;
        return;
    }
    char ymd[11]; TimeSync::formatYMD(ymd, sizeof(ymd));
    if (strcmp(s.date, ymd) == 0) {
        s_state = s;
        Serial.printf("[storage] resumed today (%s): stretch=%u water=%u walk=%u\n",
                      ymd, s.stretch_min, s.water_ml, s.walk_min);
    } else {
        s_state = DailyState{};
        strncpy(s_state.date, ymd, sizeof(s_state.date) - 1);
        Serial.printf("[storage] new day %s (was %s); counters reset\n", ymd, s.date);
    }
}

bool flushDue() {
    return millis() - s_lastFlushMs >= 5UL * 60UL * 1000UL;
}

// ─── Init ──────────────────────────────────────────────────────────────────
void begin() {
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
}

}  // namespace Storage

#include "EventQueue.h"
#include "Settings.h"
#include "Storage.h"
#include "Config.h"
#include "TimeSync.h"

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace EventQueue {

static const char* PATH_QUEUE     = "/events_pending.json";
static const char* PATH_QUEUE_TMP = "/events_pending.tmp";

static Event s_ring[Defaults::QUEUE_CAP];
static size_t s_count = 0;

static void readFromDisk() {
    s_count = 0;
    if (!LittleFS.exists(PATH_QUEUE)) return;
    fs::File f = LittleFS.open(PATH_QUEUE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[queue] parse: %s — discarding queue\n", err.c_str());
        return;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return;
    for (JsonObject o : arr) {
        if (s_count >= Defaults::QUEUE_CAP) break;
        Event& e = s_ring[s_count++];
        e.ts        = (const char*)(o["ts"]        | "");
        e.device_id = (const char*)(o["device_id"] | "");
        e.habit     = (const char*)(o["habit"]     | "");
        e.action    = (const char*)(o["action"]    | "");
        e.meta_json = (const char*)(o["meta_json"] | "");
    }
    Serial.printf("[queue] resumed %u pending events\n", (unsigned)s_count);
}

static bool writeToDisk() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < s_count; ++i) {
        JsonObject o = arr.add<JsonObject>();
        o["ts"]        = s_ring[i].ts;
        o["device_id"] = s_ring[i].device_id;
        o["habit"]     = s_ring[i].habit;
        o["action"]    = s_ring[i].action;
        o["meta_json"] = s_ring[i].meta_json;
    }
    fs::File f = LittleFS.open(PATH_QUEUE_TMP, "w");
    if (!f) return false;
    if (serializeJson(doc, f) == 0) { f.close(); return false; }
    f.close();
    LittleFS.remove(PATH_QUEUE);
    return LittleFS.rename(PATH_QUEUE_TMP, PATH_QUEUE);
}

void begin() {
    Storage::lock();
    readFromDisk();
    Storage::unlock();
}

size_t depth()   { return s_count; }
bool   isFull()  { return s_count >= Defaults::QUEUE_CAP; }

bool enqueue(const Event& e) {
    Storage::lock();
    if (s_count >= Defaults::QUEUE_CAP) {
        // Drop the oldest to make room — the most-recent events are most
        // useful for the user.
        for (size_t i = 1; i < s_count; ++i) s_ring[i - 1] = s_ring[i];
        --s_count;
        Serial.println("[queue] full; dropped oldest");
    }
    s_ring[s_count++] = e;
    bool ok = writeToDisk();
    Storage::unlock();
    return ok;
}

size_t peek(Event* out, size_t max) {
    Storage::lock();
    size_t n = s_count < max ? s_count : max;
    for (size_t i = 0; i < n; ++i) out[i] = s_ring[i];
    Storage::unlock();
    return n;
}

bool popFront(size_t n) {
    Storage::lock();
    if (n > s_count) n = s_count;
    for (size_t i = n; i < s_count; ++i) s_ring[i - n] = s_ring[i];
    s_count -= n;
    bool ok = writeToDisk();
    Storage::unlock();
    return ok;
}

// ─── Constructors ──────────────────────────────────────────────────────────
static String nowTs() {
    if (TimeSync::isSynced()) {
        char buf[32];
        TimeSync::formatIsoUtc(buf, sizeof(buf));
        return String(buf);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "boot+%lums", (unsigned long)millis());
    return String(buf);
}

Event makeFired(const char* habit) {
    Event e;
    e.ts = nowTs();
    e.device_id = Config::current().device_id;
    e.habit = habit;
    e.action = "fired";
    return e;
}

Event makeCompleted(const char* habit, const char* meta_json) {
    Event e = makeFired(habit);
    e.action = "completed";
    if (meta_json) e.meta_json = meta_json;
    return e;
}

Event makeSnoozed(const char* habit) {
    Event e = makeFired(habit);
    e.action = "snoozed";
    return e;
}

Event makeSkipped(const char* habit) {
    Event e = makeFired(habit);
    e.action = "skipped";
    return e;
}

Event makeBoot() {
    Event e;
    e.ts = nowTs();
    e.device_id = Config::current().device_id;
    e.habit = "system";
    e.action = "boot";
    return e;
}

Event makeConfigChanged() {
    Event e;
    e.ts = nowTs();
    e.device_id = Config::current().device_id;
    e.habit = "system";
    e.action = "config_changed";
    return e;
}

}  // namespace EventQueue

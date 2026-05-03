#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Config {

enum class Habit : uint8_t { Stretch = 0, Water = 1, Walk = 2 };

struct Goals {
    uint16_t stretch_min = 15;
    uint16_t water_ml    = 2000;
    uint16_t walk_min    = 30;
};

struct Intervals {
    uint16_t stretch_min = 60;
    uint16_t water_min   = 45;
    uint16_t walk_min    = 90;
};

struct QuietHours {
    uint8_t start_hour = 22;
    uint8_t end_hour   = 7;
    bool    enabled    = true;
};

struct Data {
    uint16_t   schema_version  = 1;
    String     mdns_hostname;        // "remindme"
    String     apps_script_url;      // https://script.google.com/macros/s/.../exec
    String     sheet_url;            // for display in UI
    String     ntfy_topic;           // optional fallback if Apps Script doesn't own it
    String     device_id;            // default = MAC suffix
    Goals      goals;
    Intervals  intervals;
    QuietHours quiet;
};

// Load config from /config.json on LittleFS, applying any migrations needed.
// Returns false only if LittleFS is unavailable; on parse error it falls back
// to factory defaults and rewrites the file.
bool load(Data& out);

// Persist config atomically (tmp + rename).
bool save(const Data& cfg);

// Populate `out` with factory defaults — derives device_id from the MAC.
void factoryDefaults(Data& out);

// Validate config in place. Returns "" on OK, or a human-readable error
// string. Does not mutate `cfg` — use clamp() for permissive correction.
String validate(const Data& cfg);

// Clamp out-of-range numeric values, trim/sanitize string fields.
// Used after JSON load when the file came from a hand-edit or future
// version. Never throws; idempotent.
void clamp(Data& cfg);

// JSON helpers exposed for the web UI handlers.
void toJson(const Data& cfg, JsonObject out, bool include_sensitive = false);
bool fromJson(JsonObjectConst in, Data& out);

// Singleton accessors. Read-only access from any thread (the underlying
// fields are immutable after a load); writes go through Storage::saveConfig
// which takes the storage mutex.
const Data& current();
void        setCurrent(const Data& cfg);

}  // namespace Config

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

namespace Storage {

// Initialise LittleFS-backed storage and load config into memory. Must be
// called before any other accessor.
void begin();

// Mutex helper exposed so cross-cutting modules (Backend, EventQueue) can
// take the same lock that protects Storage internals when they need to do
// multi-step read/write sequences against shared files.
void lock();
void unlock();

// ─── Config ────────────────────────────────────────────────────────────────
// Convenience wrappers around Config::load/save that take the storage mutex.
bool loadConfig(Config::Data& out);
bool saveConfig(const Config::Data& cfg);

// ─── Today/state counters (per-day, persisted to /state.json) ──────────────
struct DailyState {
    char     date[11]   = {0};   // "YYYY-MM-DD"
    uint16_t stretch_min = 0;    // accumulated minutes
    uint16_t water_ml    = 0;    // accumulated ml
    uint16_t walk_min    = 0;    // accumulated minutes
    uint16_t reminders_due       = 0;
    uint16_t reminders_done      = 0;
    uint16_t reminders_snoozed   = 0;
    uint16_t reminders_skipped   = 0;
    uint32_t last_fire_stretch_ts = 0;
    uint32_t last_fire_water_ts   = 0;
    uint32_t last_fire_walk_ts    = 0;
};

DailyState today();
void       setToday(const DailyState& s);
bool       flushToday();             // atomic write to /state.json
void       resetTodayCounters();
void       resumeTodayFromDisk();    // call after TimeSync is up
bool       flushDue();               // 5-min rate-limit check

// ─── Generic JSON file helpers ─────────────────────────────────────────────
// Read a JSON document from LittleFS. Caller passes the path and an empty
// JsonDocument to populate. Returns false on missing file or parse error.
bool readJson(const char* path, JsonDocument& doc);

// Write a JSON document atomically (tmp + rename).
bool writeJson(const char* path, const JsonDocument& doc);

}  // namespace Storage

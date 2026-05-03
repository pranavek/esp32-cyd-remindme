#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace EventQueue {

// Persistent FIFO of pending events. In-memory bounded ring (cap = QUEUE_CAP)
// backed by /events_pending.json on LittleFS — every enqueue is durably
// written through, every dequeue is durably written through. Survives reboots
// and Wi-Fi outages.
//
// Schema of each event payload (matches what Backend POSTs to Apps Script):
//   {
//     "ts":        "2026-05-01T14:23:11Z",
//     "device_id": "remindme-7f3a",
//     "habit":     "water" | "walk" | "stretch" | "system",
//     "action":    "fired" | "completed" | "snoozed" | "skipped" |
//                  "config_changed" | "boot",
//     "meta":      { ...optional }
//   }

struct Event {
    String ts;
    String device_id;
    String habit;
    String action;
    String meta_json;   // raw JSON object literal, may be ""
};

void   begin();
size_t depth();
bool   isFull();

// Enqueue an event. Writes through to disk atomically; safe to call from
// any thread that can take the storage mutex (Storage::lock guards the
// underlying file).
bool enqueue(const Event& e);

// Peek up to `max` events without removing them. Returns the count copied
// into `out`. Used by Backend before POSTing.
size_t peek(Event* out, size_t max);

// Remove the front `n` events after a successful POST. Atomic disk write.
bool popFront(size_t n);

// Convenience constructors. `ts` is auto-filled from TimeSync if synced,
// else left as ms-since-boot for client-side correlation.
Event makeFired   (const char* habit);
Event makeCompleted(const char* habit, const char* meta_json = nullptr);
Event makeSnoozed (const char* habit);
Event makeSkipped (const char* habit);
Event makeBoot();
Event makeConfigChanged();

}  // namespace EventQueue

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace Stats {

// Spawns a low-priority FreeRTOS task pinned to core 0 that GETs the
// `?summary=today` and `?summary=14d` endpoints from Apps Script every
// STATS_REFRESH_MS, caches the response in /stats_cache.json. The display
// reads from the cache so it stays useful when offline.
void begin();

// Force an immediate fetch on the next tick.
void refresh();

// Read the cached today summary into `out`. Returns false if the cache is
// missing or unparseable.
bool readTodayCache(JsonDocument& out);

// Read the cached 14-day summary into `out`.
bool read14dCache(JsonDocument& out);

uint32_t lastSyncMs();

}  // namespace Stats

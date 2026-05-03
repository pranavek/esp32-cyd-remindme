#pragma once

#include <Arduino.h>

namespace Backend {

// Spawns a FreeRTOS task pinned to **core 0** that drains EventQueue via
// HTTPS POST to the configured Apps Script endpoint. Synchronous HTTPS
// must NOT live in the main loop — it freezes the touch handler. This
// pattern is lifted from biodome/src/TelegramBus.cpp.
void begin();

// Force the task to re-check the queue immediately (after enqueue). The
// task wakes on a periodic timer anyway, but this notify avoids a 5 s lag
// when the user just tapped Done.
void poke();

bool   isReady();
String lastError();
uint32_t lastSyncMs();      // millis() of last successful POST, 0 if none
uint32_t lastErrorMs();

}  // namespace Backend

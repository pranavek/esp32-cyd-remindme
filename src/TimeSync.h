#pragma once

#include <Arduino.h>
#include <time.h>

namespace TimeSync {

// Configures SNTP using TZ_POSIX + NTP_SERVER_* from Settings.h. Non-blocking.
void begin();

bool isSynced();
time_t nowLocal();

void  formatHM(char* buf, size_t n);
void  formatYMD(char* buf, size_t n);
void  formatYMDFor(time_t t, char* buf, size_t n);
void  formatIsoUtc(char* buf, size_t n);    // "2026-05-01T14:23:11Z"

int currentYear();
int currentMonth();
int currentDay();
int currentDow();
int currentHour();
int currentMinute();

bool dayChanged();

}  // namespace TimeSync

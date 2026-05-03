#include "TimeSync.h"
#include "Settings.h"

#include <sys/time.h>

namespace TimeSync {

static int lastDayOfMonth = -1;

void begin() {
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
}

bool isSynced() {
    return time(nullptr) > 1672531200L;   // anything past 2023-01-01
}

time_t nowLocal() { return time(nullptr); }

static void fillTm(struct tm* out) {
    time_t now = time(nullptr);
    localtime_r(&now, out);
}

void formatHM(char* buf, size_t n) {
    struct tm t; fillTm(&t);
    strftime(buf, n, "%H:%M", &t);
}

void formatYMD(char* buf, size_t n) {
    struct tm t; fillTm(&t);
    strftime(buf, n, "%Y-%m-%d", &t);
}

void formatYMDFor(time_t t, char* buf, size_t n) {
    struct tm tmnow;
    localtime_r(&t, &tmnow);
    strftime(buf, n, "%Y-%m-%d", &tmnow);
}

void formatIsoUtc(char* buf, size_t n) {
    time_t now = time(nullptr);
    struct tm tmnow;
    gmtime_r(&now, &tmnow);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tmnow);
}

int currentYear()   { struct tm t; fillTm(&t); return t.tm_year + 1900; }
int currentMonth()  { struct tm t; fillTm(&t); return t.tm_mon + 1; }
int currentDay()    { struct tm t; fillTm(&t); return t.tm_mday; }
int currentDow()    { struct tm t; fillTm(&t); return t.tm_wday; }
int currentHour()   { struct tm t; fillTm(&t); return t.tm_hour; }
int currentMinute() { struct tm t; fillTm(&t); return t.tm_min; }

bool dayChanged() {
    if (!isSynced()) return false;
    int d = currentDay();
    if (lastDayOfMonth == -1) {
        lastDayOfMonth = d;
        return false;
    }
    if (d != lastDayOfMonth) {
        lastDayOfMonth = d;
        return true;
    }
    return false;
}

}  // namespace TimeSync

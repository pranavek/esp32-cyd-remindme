// Read-side companion to Backend.cpp: pulls aggregate today/14d JSON from
// Apps Script and caches it on LittleFS so the screen renders even when
// offline. Same core-0 pinning rule as Backend.

#include "Stats.h"
#include "Settings.h"
#include "Config.h"
#include "Storage.h"

#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace Stats {

static const char* PATH_TODAY = "/stats_today.json";
static const char* PATH_14D   = "/stats_14d.json";

static TaskHandle_t s_taskHandle = nullptr;
static uint32_t     s_lastSyncMs = 0;

static bool getJson(WiFiClientSecure& sec, HTTPClient& http,
                    const String& base, const char* summary,
                    const char* outPath) {
    String url = base;
    url += (base.indexOf('?') >= 0) ? "&" : "?";
    url += "summary=";
    url += summary;
    url += "&device_id=";
    url += Config::current().device_id;

    if (!http.begin(sec, url)) return false;
    http.setTimeout(15000);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[stats] GET %s -> %d\n", summary, code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    // Validate it parses, then write through Storage for atomicity.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[stats] parse %s: %s\n", summary, err.c_str());
        return false;
    }
    return Storage::writeJson(outPath, doc);
}

static void task(void*) {
    WiFiClientSecure sec;
    sec.setInsecure();
    HTTPClient http;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(Defaults::STATS_REFRESH_MS));

        if (WiFi.status() != WL_CONNECTED) continue;
        const String& base = Config::current().apps_script_url;
        if (base.isEmpty()) continue;

        bool a = getJson(sec, http, base, "today", PATH_TODAY);
        bool b = getJson(sec, http, base, "14d",   PATH_14D);
        if (a || b) s_lastSyncMs = millis();
    }
}

void begin() {
    if (s_taskHandle) return;
    xTaskCreatePinnedToCore(task, "stats", 6144, nullptr, 1, &s_taskHandle, 0);
}

void refresh() {
    if (s_taskHandle) xTaskNotifyGive(s_taskHandle);
}

bool readTodayCache(JsonDocument& out) {
    return Storage::readJson(PATH_TODAY, out);
}

bool read14dCache(JsonDocument& out) {
    return Storage::readJson(PATH_14D, out);
}

uint32_t lastSyncMs() { return s_lastSyncMs; }

}  // namespace Stats

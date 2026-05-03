// Outbound HTTPS task pinned to core 0.
//
// Why this pattern: synchronous TLS handshakes against script.google.com
// take 1-3 seconds during the cert exchange. Running that on the main loop
// freezes the touch handler — the user tries to Done a popup, nothing
// happens, they tap again, the popup pops twice. Pinning the task to core 0
// (the same one that handles WiFi/IP stack interrupts) keeps the UI loop on
// core 1 responsive. See deskmate's CLAUDE.md "What NOT to do" §3.
//
// On TLS verification: we use `setInsecure()`. The script.google.com
// endpoint is a Google service so the cert chain *could* be pinned, but
// pinning a CA cert costs ~6 KB of RAM and the CYD has ~150 KB free. The
// payload here is event metadata (no credentials), so an active MITM gains
// nothing useful. Documented tradeoff.

#include "Backend.h"
#include "Settings.h"
#include "Config.h"
#include "EventQueue.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace Backend {

static TaskHandle_t s_taskHandle = nullptr;
static String       s_lastErr;
static bool         s_ready = false;
static uint32_t     s_lastSyncMs  = 0;
static uint32_t     s_lastErrorMs = 0;
static uint32_t     s_backoffUntilMs = 0;   // millis() at which we may retry
static uint32_t     s_backoffMs      = 0;   // current retry interval

static String buildPayload(const EventQueue::Event* events, size_t n) {
    JsonDocument doc;
    doc["device_id"] = Config::current().device_id;
    JsonArray arr = doc["events"].to<JsonArray>();
    for (size_t i = 0; i < n; ++i) {
        JsonObject o = arr.add<JsonObject>();
        o["ts"]        = events[i].ts;
        o["device_id"] = events[i].device_id;
        o["habit"]     = events[i].habit;
        o["action"]    = events[i].action;
        if (!events[i].meta_json.isEmpty()) {
            // meta_json is a raw object literal; embed it as nested object.
            JsonDocument m;
            DeserializationError err = deserializeJson(m, events[i].meta_json);
            if (!err) o["meta"] = m.as<JsonObject>();
        }
    }
    String out;
    serializeJson(doc, out);
    return out;
}

static bool postBatch(WiFiClientSecure& sec, HTTPClient& http,
                      const String& url,
                      const EventQueue::Event* events, size_t n) {
    if (!http.begin(sec, url)) {
        s_lastErr = "http.begin failed";
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);

    String payload = buildPayload(events, n);
    int code = http.POST(payload);
    String body = (code > 0) ? http.getString() : String("");
    http.end();

    if (code < 200 || code >= 400) {
        // Apps Script returns 302 to a googleusercontent.com URL on first
        // call. HTTPClient should follow automatically; if not, we'd hit
        // this branch. Surface the code for debugging.
        s_lastErr = "POST " + String(code);
        if (body.length() > 0 && body.length() < 200) s_lastErr += ": " + body;
        return false;
    }
    return true;
}

static void task(void*) {
    WiFiClientSecure sec;
    sec.setInsecure();
    HTTPClient http;

    constexpr size_t BATCH = 16;
    EventQueue::Event batch[BATCH];

    for (;;) {
        // Wait for a notify or the periodic tick.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(Defaults::BACKEND_TICK_MS));

        if (WiFi.status() != WL_CONNECTED) {
            s_ready = false;
            continue;
        }

        const String& url = Config::current().apps_script_url;
        if (url.isEmpty()) {
            s_ready = false;
            continue;
        }

        size_t n = EventQueue::peek(batch, BATCH);
        if (n == 0) {
            s_ready = true;
            continue;
        }

        if (s_backoffUntilMs && (int32_t)(millis() - s_backoffUntilMs) < 0) continue;

        if (postBatch(sec, http, url, batch, n)) {
            EventQueue::popFront(n);
            s_lastSyncMs     = millis();
            s_backoffUntilMs = 0;
            s_backoffMs      = 0;
            s_lastErr        = "";
            s_ready          = true;
            Serial.printf("[backend] flushed %u events\n", (unsigned)n);
        } else {
            s_lastErrorMs = millis();
            s_backoffMs   = s_backoffMs ? s_backoffMs * 2 : 5000;
            if (s_backoffMs > Defaults::BACKEND_MAX_BACKOFF_MS) s_backoffMs = Defaults::BACKEND_MAX_BACKOFF_MS;
            s_backoffUntilMs = millis() + s_backoffMs;
            Serial.printf("[backend] %s; retry in %u ms\n", s_lastErr.c_str(), s_backoffMs);
        }
    }
}

void begin() {
    if (s_taskHandle) return;
    xTaskCreatePinnedToCore(task, "backend", 8192, nullptr, 1, &s_taskHandle, 0);
}

void poke() {
    if (s_taskHandle) xTaskNotifyGive(s_taskHandle);
}

bool   isReady()       { return s_ready; }
String lastError()     { return s_lastErr; }
uint32_t lastSyncMs()  { return s_lastSyncMs; }
uint32_t lastErrorMs() { return s_lastErrorMs; }

}  // namespace Backend

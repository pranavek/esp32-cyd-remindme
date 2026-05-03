// REST API routes for the web UI. All Async handler bodies are tiny — heavy
// work is delegated to background tasks (Backend, Stats) so the HTTP stack
// never blocks. Reboots after config save are scheduled via Net::scheduleReboot
// so the response gets to flush first.

#include "Api.h"
#include "Settings.h"
#include "Config.h"
#include "Storage.h"
#include "Net.h"
#include "Reminders.h"
#include "Diagnostics.h"
#include "EventQueue.h"
#include "Backend.h"
#include "Stats.h"
#include "Touch.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

namespace Api {

static void sendJson(AsyncWebServerRequest* req, const JsonDocument& doc, int code = 200) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(code);
    serializeJson(doc, *res);
    req->send(res);
}

static void sendError(AsyncWebServerRequest* req, int code, const String& msg) {
    JsonDocument doc;
    doc["error"] = msg;
    sendJson(req, doc, code);
}

void registerRoutes(AsyncWebServer& server) {

    // GET /api/config — current config
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        Config::toJson(Config::current(), doc.to<JsonObject>());
        sendJson(req, doc);
    });

    // POST /api/config — replace config (full or partial)
    AsyncCallbackJsonWebHandler* cfgHandler = new AsyncCallbackJsonWebHandler(
        "/api/config",
        [](AsyncWebServerRequest* req, JsonVariant& body) {
            if (!body.is<JsonObject>()) {
                sendError(req, 400, "expected JSON object body");
                return;
            }
            // Merge into current — partial updates are friendly for the UI.
            Config::Data merged = Config::current();

            // We re-use fromJson which fully populates from a JSON object —
            // for partial updates, copy keys from current first and then
            // overlay user fields. Since fromJson reads with `| default`
            // semantics, missing keys will reset to defaults; merge by
            // reserialising current and overlaying.
            JsonDocument curDoc;
            JsonObject curObj = curDoc.to<JsonObject>();
            Config::toJson(merged, curObj);

            JsonObjectConst inObj = body.as<JsonObjectConst>();
            for (JsonPairConst kv : inObj) {
                curObj[kv.key()] = kv.value();
            }

            Config::Data parsed;
            Config::fromJson(curDoc.as<JsonObject>(), parsed);
            Config::clamp(parsed);
            String err = Config::validate(parsed);
            if (!err.isEmpty()) {
                sendError(req, 400, err);
                return;
            }
            if (!Storage::saveConfig(parsed)) {
                sendError(req, 500, "save failed");
                return;
            }
            Config::setCurrent(parsed);
            EventQueue::enqueue(EventQueue::makeConfigChanged());
            Backend::poke();

            JsonDocument out;
            JsonObject o = out.to<JsonObject>();
            o["status"] = "ok";
            o["reboot_in_ms"] = 1500;
            sendJson(req, out);

            // Reboot so hostname/portal/services pick up the new config.
            Net::scheduleReboot(1500);
        });
    server.addHandler(cfgHandler);

    // GET /api/state — runtime status snapshot
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        Diagnostics::snapshot(doc.to<JsonObject>());
        sendJson(req, doc);
    });

    // POST /api/event — manual habit completion logged from the phone UI
    AsyncCallbackJsonWebHandler* evtHandler = new AsyncCallbackJsonWebHandler(
        "/api/event",
        [](AsyncWebServerRequest* req, JsonVariant& body) {
            JsonObjectConst o = body.as<JsonObjectConst>();
            const char* habitStr = o["habit"]  | "";
            const char* action   = o["action"] | "completed";
            uint16_t amount      = (uint16_t)(int)(o["amount"] | 0);

            Config::Habit h;
            if      (!strcmp(habitStr, "stretch")) h = Config::Habit::Stretch;
            else if (!strcmp(habitStr, "water"))   h = Config::Habit::Water;
            else if (!strcmp(habitStr, "walk"))    h = Config::Habit::Walk;
            else { sendError(req, 400, "habit must be stretch|water|walk"); return; }

            if      (!strcmp(action, "completed")) Reminders::onDone(h, amount);
            else if (!strcmp(action, "snoozed"))   Reminders::onSnooze(h);
            else if (!strcmp(action, "skipped"))   Reminders::onSkip(h);
            else if (!strcmp(action, "fire"))      Reminders::forceFire(h);
            else { sendError(req, 400, "unknown action"); return; }

            JsonDocument out;
            out.to<JsonObject>()["status"] = "ok";
            sendJson(req, out);
        });
    server.addHandler(evtHandler);

    // POST /api/wifi/reset — clear creds and reboot into the portal
    server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc.to<JsonObject>()["status"] = "rebooting";
        sendJson(req, doc);
        Net::scheduleReboot(800);
        // Actual creds reset happens after reboot — schedule a flag file
        // to trigger it on next boot.
        fs::File f = LittleFS.open("/wifi_reset.flag", "w");
        if (f) { f.print("1"); f.close(); }
    });

    // POST /api/touch/recalibrate — wipe /cal.json and reboot
    server.on("/api/touch/recalibrate", HTTP_POST, [](AsyncWebServerRequest* req) {
        Touch::resetCalibration();
        JsonDocument doc;
        doc.to<JsonObject>()["status"] = "rebooting";
        sendJson(req, doc);
        Net::scheduleReboot(800);
    });

    // POST /api/reboot
    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc.to<JsonObject>()["status"] = "rebooting";
        sendJson(req, doc);
        Net::scheduleReboot(800);
    });

    // GET /api/stats/today — cached today summary (proxy to LittleFS cache)
    server.on("/api/stats/today", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        if (Stats::readTodayCache(doc)) sendJson(req, doc);
        else                            sendError(req, 503, "no cached data");
    });

    server.on("/api/stats/14d", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        if (Stats::read14dCache(doc)) sendJson(req, doc);
        else                          sendError(req, 503, "no cached data");
    });

    // POST /api/stats/refresh — kick the Stats task
    server.on("/api/stats/refresh", HTTP_POST, [](AsyncWebServerRequest* req) {
        Stats::refresh();
        JsonDocument doc;
        doc.to<JsonObject>()["status"] = "queued";
        sendJson(req, doc);
    });

    // Static files at "/" — index.html, app.css, app.js. If the LittleFS
    // image hasn't been uploaded the directory is empty and `/` falls
    // through to the onNotFound handler below.
    server.serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // 404 fallback. We deliberately do NOT redirect non-/api/ paths back
    // to "/" — if /web/index.html is missing on LittleFS, that would loop
    // the browser. Instead serve a small inline diagnostic page that tells
    // the user exactly what to do (run `pio run -t uploadfs`).
    server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->url().startsWith("/api/")) {
            sendError(req, 404, "no such route");
            return;
        }
        bool haveIndex = LittleFS.exists("/web/index.html");
        AsyncWebServerResponse* res = req->beginResponse(
            haveIndex ? 404 : 503,
            "text/html; charset=utf-8",
            String(R"HTML(<!doctype html>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RemindMe — setup</title>
<style>
  body{margin:0;background:#0a0a0c;color:#f5f5f7;
    font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:32px 22px}
  h1{margin:0 0 6px;font-size:22px}
  .card{background:#15161a;border-radius:12px;padding:18px 18px 14px;margin:14px 0;max-width:560px}
  code,pre{background:#000;color:#ffb84d;padding:2px 6px;border-radius:4px;font-size:13px}
  pre{padding:10px 12px;overflow:auto}
  .dim{color:#8a8a92}
  a{color:#ffb84d}
</style>
<h1>RemindMe</h1>
<p class="dim">Web UI assets are missing on the device.</p>
<div class="card">
  <p>The firmware is running and the HTTP server is up
  (you got this page, which proves it). But the
  <code>data/</code> folder hasn't been uploaded yet — the file
  <code>/web/index.html</code> doesn't exist on LittleFS.</p>
  <p><strong>Fix:</strong> from your dev machine, run</p>
  <pre>pio run -t uploadfs</pre>
  <p class="dim">…then power-cycle. The full UI will appear here.</p>
  <p class="dim">Both the firmware and the LittleFS image must be flashed.
  The merged <code>firmware-merged.bin</code> from the web flasher
  includes both, so this page should never appear if you flashed via
  GitHub Pages — only when flashing locally with
  <code>pio run -t upload</code> alone.</p>
</div>
<div class="card">
  <p class="dim">Sanity checks:</p>
  <ul>
    <li><a href="/ping">/ping</a> → should return <code>pong</code></li>
    <li><a href="/api/state">/api/state</a> → JSON snapshot</li>
    <li><a href="/api/config">/api/config</a> → current config</li>
  </ul>
</div>
)HTML"));
        req->send(res);
    });
}

}  // namespace Api

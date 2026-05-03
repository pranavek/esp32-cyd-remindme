#include "Diagnostics.h"
#include "Settings.h"
#include "Net.h"
#include "Backend.h"
#include "Stats.h"
#include "EventQueue.h"
#include "Reminders.h"
#include "TimeSync.h"
#include "Config.h"

#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <TFT_eSPI.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

namespace Diagnostics {

static TFT_eSPI* s_tft = nullptr;
static bool      s_visible = false;

void begin(TFT_eSPI* tft) { s_tft = tft; }

void toggleOverlay() {
    s_visible = !s_visible;
    if (s_visible) renderOverlay();
}

void hideOverlay() { s_visible = false; }
bool overlayVisible() { return s_visible; }

static void drawLine(int y, const char* k, const String& v) {
    if (LittleFS.exists(String(FONT_SMALL) + ".vlw")) {
        s_tft->loadFont(FONT_SMALL, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(k, 14, y);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(v, 110, y);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::DIM, Color::CARD);
        s_tft->drawString(k, 14, y, 2);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString(v, 110, y, 2);
    }
}

void renderOverlay() {
    if (!s_tft || !s_visible) return;
    using namespace Layout;

    int x = 8, y = 26, w = W - 16, h = H - 34;
    s_tft->fillRoundRect(x, y, w, h, 10, Color::CARD);
    s_tft->drawRoundRect(x, y, w, h, 10, Color::DIM);

    if (LittleFS.exists(String(FONT_LARGE) + ".vlw")) {
        s_tft->loadFont(FONT_LARGE, LittleFS);
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString("Diagnostics", x + 12, y + 18);
        s_tft->unloadFont();
    } else {
        s_tft->setTextDatum(ML_DATUM);
        s_tft->setTextColor(Color::FG, Color::CARD);
        s_tft->drawString("Diagnostics", x + 12, y + 18, 4);
    }

    int row = y + 44;
    drawLine(row, "WiFi",   Net::isConnected() ? Net::ssid() : String("--"));
    row += 16;
    drawLine(row, "RSSI",   Net::isConnected() ? (String(Net::rssi()) + " dBm") : String("--"));
    row += 16;
    drawLine(row, "IP",     Net::isConnected() ? Net::localIP().toString() : String("--"));
    row += 16;
    drawLine(row, "mDNS",   Config::current().mdns_hostname + ".local");
    row += 16;
    drawLine(row, "Queue",  String(EventQueue::depth()));
    row += 16;

    String last;
    if (Backend::lastSyncMs() == 0) last = "never";
    else last = String((millis() - Backend::lastSyncMs()) / 1000UL) + "s ago";
    drawLine(row, "Sync",   last); row += 16;

    drawLine(row, "Heap",   String(ESP.getFreeHeap()) + " B"); row += 16;
    drawLine(row, "Uptime", String(millis() / 1000UL) + "s"); row += 16;
    drawLine(row, "FW",     String(FW_VERSION));
}

void snapshot(JsonObject out) {
    out["fw_version"]      = FW_VERSION;
    out["uptime_s"]        = millis() / 1000UL;
    out["heap"]            = ESP.getFreeHeap();
    out["wifi_connected"]  = Net::isConnected();
    out["wifi_ssid"]       = Net::isConnected() ? Net::ssid() : String("");
    out["wifi_rssi"]       = Net::isConnected() ? Net::rssi() : 0;
    out["ip"]              = Net::isConnected() ? Net::localIP().toString() : String("");
    out["mdns"]            = Config::current().mdns_hostname + ".local";
    out["queue_depth"]     = EventQueue::depth();
    out["last_sync_ms_ago"] = Backend::lastSyncMs() ? (millis() - Backend::lastSyncMs()) : 0;
    out["last_error"]      = Backend::lastError();
    out["backend_ready"]   = Backend::isReady();

    JsonObject today = out["today"].to<JsonObject>();
    today["stretch_min"] = Reminders::todayDoneStretchMin();
    today["water_ml"]    = Reminders::todayDoneWaterMl();
    today["walk_min"]    = Reminders::todayDoneWalkMin();
}

}  // namespace Diagnostics

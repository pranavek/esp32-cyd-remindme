#include "Net.h"
#include "Settings.h"
#include "Config.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>

namespace Net {

static AsyncWebServer s_server(HTTP_PORT);
static WiFiManager   s_wm;
static PortalCallback s_portalCb = nullptr;

static uint32_t s_rebootAtMs = 0;

static void onConfigModeCallback(WiFiManager*) {
    Serial.printf("[net] entered config portal as AP '%s' on %s\n",
                  AP_NAME, WiFi.softAPIP().toString().c_str());
    if (s_portalCb) s_portalCb();
}

bool startWifi(PortalCallback onPortalActive) {
    s_portalCb = onPortalActive;

    const char* host = Config::current().mdns_hostname.c_str();
    if (!host || !*host) host = DEFAULT_MDNS_HOSTNAME;

    s_wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    s_wm.setAPCallback(onConfigModeCallback);
    s_wm.setHostname(host);
    s_wm.setDebugOutput(false);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(host);

    Serial.printf("[net] WiFiManager autoConnect (host=%s)\n", host);
    bool ok = s_wm.autoConnect(AP_NAME);
    if (ok) {
        Serial.printf("[net] connected to '%s' as %s\n",
                      WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[net] WiFi unavailable (portal timeout)");
    }
    return ok;
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }
IPAddress localIP() { return WiFi.localIP(); }
String ssid() { return WiFi.SSID(); }
int rssi() { return WiFi.RSSI(); }

[[noreturn]] void resetCredentialsAndReboot() {
    Serial.println("[net] resetting WiFi credentials and rebooting");
    s_wm.resetSettings();
    delay(200);
    ESP.restart();
    while (true) { delay(1000); }
}

AsyncWebServer& server() { return s_server; }

void startMdnsAndServer() {
    if (!isConnected()) {
        Serial.println("[net] startMdnsAndServer skipped: not connected");
        return;
    }
    const char* host = Config::current().mdns_hostname.c_str();
    if (!host || !*host) host = DEFAULT_MDNS_HOSTNAME;

    if (MDNS.begin(host)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[net] mDNS up: http://%s.local/\n", host);
    } else {
        Serial.println("[net] mDNS begin() failed");
    }

    s_server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "pong\n");
    });
    s_server.begin();
    Serial.println("[net] AsyncWebServer started on :80");
}

void scheduleReboot(uint32_t delayMs) {
    s_rebootAtMs = millis() + delayMs;
    Serial.printf("[net] reboot scheduled in %u ms\n", delayMs);
}

void tick() {
    if (s_rebootAtMs && (int32_t)(millis() - s_rebootAtMs) >= 0) {
        Serial.println("[net] rebooting");
        delay(50);
        ESP.restart();
    }
}

}  // namespace Net

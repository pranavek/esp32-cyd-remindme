#pragma once

#include <Arduino.h>
#include <IPAddress.h>

class AsyncWebServer;

namespace Net {

using PortalCallback = void(*)();

// Try to connect using saved creds; if none/fails, start the captive portal AP.
// Reads `mdns_hostname` from Config::current() so the network identity matches
// what the user configured.
bool startWifi(PortalCallback onPortalActive);

bool      isConnected();
IPAddress localIP();
String    ssid();
int       rssi();

[[noreturn]] void resetCredentialsAndReboot();

AsyncWebServer& server();
void startMdnsAndServer();

// Schedule an `ESP.restart()` to fire ~delayMs after now. Used by API
// handlers — never restart synchronously inside an Async handler context
// or the response won't flush.
void scheduleReboot(uint32_t delayMs = 500);

// Should be called every loop tick by main; performs the deferred reboot.
void tick();

}  // namespace Net

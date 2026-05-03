#pragma once

#include <ESPAsyncWebServer.h>

namespace Api {

// Register all /api/* routes. Call before Net::startMdnsAndServer().
void registerRoutes(AsyncWebServer& server);

}  // namespace Api

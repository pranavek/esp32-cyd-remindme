#include "Config.h"
#include "Settings.h"

#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>

namespace Config {

static Data        s_current;
static const char* PATH_CONFIG     = "/config.json";
static const char* PATH_CONFIG_TMP = "/config.tmp";

// ─── Validation helpers ────────────────────────────────────────────────────
static bool isHostnameOk(const String& s) {
    if (s.length() < 1 || s.length() > 32) return false;
    if (s.charAt(0) == '-' || s.charAt(s.length() - 1) == '-') return false;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) return false;
    }
    return true;
}

static bool isTopicOk(const String& s) {
    if (s.length() < 1 || s.length() > 64) return false;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
               || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

static bool isDeviceIdOk(const String& s) {
    if (s.length() < 1 || s.length() > 32) return false;
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s.charAt(i);
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
               || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

String validate(const Data& cfg) {
    if (!isHostnameOk(cfg.mdns_hostname))
        return "mdns_hostname: 1-32 chars, [a-z0-9-], no leading/trailing -";

    if (!cfg.apps_script_url.isEmpty()) {
        if (!cfg.apps_script_url.startsWith("https://script.google.com/macros/s/"))
            return "apps_script_url: must start with https://script.google.com/macros/s/";
        if (cfg.apps_script_url.length() > 256)
            return "apps_script_url: too long (max 256)";
    }

    if (!cfg.sheet_url.isEmpty() &&
        !cfg.sheet_url.startsWith("https://docs.google.com/spreadsheets/d/"))
        return "sheet_url: must start with https://docs.google.com/spreadsheets/d/";

    if (!cfg.ntfy_topic.isEmpty() && !isTopicOk(cfg.ntfy_topic))
        return "ntfy_topic: 1-64 chars, [A-Za-z0-9_-]";

    if (!isDeviceIdOk(cfg.device_id))
        return "device_id: 1-32 chars, [A-Za-z0-9_-]";

    if (cfg.goals.stretch_min < 1 || cfg.goals.stretch_min > 60)
        return "goals.stretch_min: 1-60";
    if (cfg.goals.water_ml < 100 || cfg.goals.water_ml > 5000)
        return "goals.water_ml: 100-5000";
    if (cfg.goals.walk_min < 1 || cfg.goals.walk_min > 120)
        return "goals.walk_min: 1-120";

    auto okIv = [](uint16_t m) { return m >= 5 && m <= 480; };
    if (!okIv(cfg.intervals.stretch_min)) return "intervals.stretch_min: 5-480";
    if (!okIv(cfg.intervals.water_min))   return "intervals.water_min: 5-480";
    if (!okIv(cfg.intervals.walk_min))    return "intervals.walk_min: 5-480";

    if (cfg.quiet.start_hour > 23 || cfg.quiet.end_hour > 23)
        return "quiet hours: 0-23";

    return "";
}

void clamp(Data& cfg) {
    cfg.mdns_hostname.toLowerCase();
    if (cfg.mdns_hostname.isEmpty()) cfg.mdns_hostname = DEFAULT_MDNS_HOSTNAME;

    auto clampU16 = [](uint16_t& v, uint16_t lo, uint16_t hi, uint16_t fb) {
        if (v < lo || v > hi) v = fb;
    };
    clampU16(cfg.goals.stretch_min, 1, 60,    Defaults::STRETCH_MIN);
    clampU16(cfg.goals.water_ml,    100, 5000, Defaults::WATER_ML);
    clampU16(cfg.goals.walk_min,    1, 120,   Defaults::WALK_MIN);

    clampU16(cfg.intervals.stretch_min, 5, 480, Defaults::INTERVAL_STRETCH_MIN);
    clampU16(cfg.intervals.water_min,   5, 480, Defaults::INTERVAL_WATER_MIN);
    clampU16(cfg.intervals.walk_min,    5, 480, Defaults::INTERVAL_WALK_MIN);

    if (cfg.quiet.start_hour > 23) cfg.quiet.start_hour = Defaults::QUIET_START;
    if (cfg.quiet.end_hour   > 23) cfg.quiet.end_hour   = Defaults::QUIET_END;
}

// ─── Defaults ──────────────────────────────────────────────────────────────
void factoryDefaults(Data& out) {
    out = Data{};
    out.schema_version = CONFIG_SCHEMA_VERSION;
    out.mdns_hostname  = DEFAULT_MDNS_HOSTNAME;

    // Derive device_id from MAC suffix: "remindme-AABBCC".
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[20];
    snprintf(id, sizeof(id), "remindme-%02x%02x%02x", mac[3], mac[4], mac[5]);
    out.device_id = id;

    out.goals.stretch_min = Defaults::STRETCH_MIN;
    out.goals.water_ml    = Defaults::WATER_ML;
    out.goals.walk_min    = Defaults::WALK_MIN;

    out.intervals.stretch_min = Defaults::INTERVAL_STRETCH_MIN;
    out.intervals.water_min   = Defaults::INTERVAL_WATER_MIN;
    out.intervals.walk_min    = Defaults::INTERVAL_WALK_MIN;

    out.quiet.start_hour = Defaults::QUIET_START;
    out.quiet.end_hour   = Defaults::QUIET_END;
    out.quiet.enabled    = Defaults::QUIET_ON;
}

// ─── JSON ──────────────────────────────────────────────────────────────────
void toJson(const Data& cfg, JsonObject out, bool include_sensitive) {
    out["schema_version"]  = cfg.schema_version;
    out["mdns_hostname"]   = cfg.mdns_hostname;
    out["apps_script_url"] = cfg.apps_script_url;
    out["sheet_url"]       = cfg.sheet_url;
    out["ntfy_topic"]      = cfg.ntfy_topic;
    out["device_id"]       = cfg.device_id;

    JsonObject g = out["goals"].to<JsonObject>();
    g["stretch_min"] = cfg.goals.stretch_min;
    g["water_ml"]    = cfg.goals.water_ml;
    g["walk_min"]    = cfg.goals.walk_min;

    JsonObject iv = out["intervals"].to<JsonObject>();
    iv["stretch_min"] = cfg.intervals.stretch_min;
    iv["water_min"]   = cfg.intervals.water_min;
    iv["walk_min"]    = cfg.intervals.walk_min;

    JsonObject q = out["quiet"].to<JsonObject>();
    q["start_hour"] = cfg.quiet.start_hour;
    q["end_hour"]   = cfg.quiet.end_hour;
    q["enabled"]    = cfg.quiet.enabled;

    (void)include_sensitive;   // reserved for future fields the UI shouldn't see
}

bool fromJson(JsonObjectConst in, Data& out) {
    if (in.isNull()) return false;

    out.schema_version  = (uint16_t)(in["schema_version"]  | (int)CONFIG_SCHEMA_VERSION);
    out.mdns_hostname   = (const char*)(in["mdns_hostname"]   | DEFAULT_MDNS_HOSTNAME);
    out.apps_script_url = (const char*)(in["apps_script_url"] | "");
    out.sheet_url       = (const char*)(in["sheet_url"]       | "");
    out.ntfy_topic      = (const char*)(in["ntfy_topic"]      | "");
    out.device_id       = (const char*)(in["device_id"]       | "");

    JsonObjectConst g = in["goals"];
    out.goals.stretch_min = (uint16_t)(int)(g["stretch_min"] | (int)Defaults::STRETCH_MIN);
    out.goals.water_ml    = (uint16_t)(int)(g["water_ml"]    | (int)Defaults::WATER_ML);
    out.goals.walk_min    = (uint16_t)(int)(g["walk_min"]    | (int)Defaults::WALK_MIN);

    JsonObjectConst iv = in["intervals"];
    out.intervals.stretch_min = (uint16_t)(int)(iv["stretch_min"] | (int)Defaults::INTERVAL_STRETCH_MIN);
    out.intervals.water_min   = (uint16_t)(int)(iv["water_min"]   | (int)Defaults::INTERVAL_WATER_MIN);
    out.intervals.walk_min    = (uint16_t)(int)(iv["walk_min"]    | (int)Defaults::INTERVAL_WALK_MIN);

    JsonObjectConst q = in["quiet"];
    out.quiet.start_hour = (uint8_t)(int)(q["start_hour"] | (int)Defaults::QUIET_START);
    out.quiet.end_hour   = (uint8_t)(int)(q["end_hour"]   | (int)Defaults::QUIET_END);
    out.quiet.enabled    = q["enabled"] | Defaults::QUIET_ON;

    if (out.device_id.isEmpty()) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char id[20];
        snprintf(id, sizeof(id), "remindme-%02x%02x%02x", mac[3], mac[4], mac[5]);
        out.device_id = id;
    }
    return true;
}

// ─── Migration chain ───────────────────────────────────────────────────────
// When the schema version on disk is older than CONFIG_SCHEMA_VERSION we walk
// forward through migrate_v{N}_to_v{N+1} steps. Today there's only one
// version, so this is a no-op stub — but the chain is wired now so future
// migrations can be added without restructuring the loader.
static void migrate(Data& cfg) {
    if (cfg.schema_version == CONFIG_SCHEMA_VERSION) return;
    Serial.printf("[config] migrating schema_version %u -> %u\n",
                  cfg.schema_version, CONFIG_SCHEMA_VERSION);
    // Future: while (cfg.schema_version < N) { migrate_vX_to_vY(cfg); }
    cfg.schema_version = CONFIG_SCHEMA_VERSION;
}

// ─── Persistence ───────────────────────────────────────────────────────────
bool load(Data& out) {
    if (!LittleFS.exists(PATH_CONFIG)) {
        Serial.println("[config] no /config.json — writing factory defaults");
        factoryDefaults(out);
        return save(out);
    }
    fs::File f = LittleFS.open(PATH_CONFIG, "r");
    if (!f) {
        Serial.println("[config] open failed; using factory defaults");
        factoryDefaults(out);
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[config] parse: %s — rewriting defaults\n", err.c_str());
        factoryDefaults(out);
        save(out);
        return true;
    }
    if (!fromJson(doc.as<JsonObject>(), out)) {
        factoryDefaults(out);
        save(out);
        return true;
    }
    migrate(out);
    clamp(out);
    return true;
}

bool save(const Data& cfg) {
    fs::File f = LittleFS.open(PATH_CONFIG_TMP, "w");
    if (!f) {
        Serial.println("[config] open tmp for write failed");
        return false;
    }
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    toJson(cfg, obj, /*include_sensitive=*/true);
    if (serializeJson(doc, f) == 0) {
        f.close();
        Serial.println("[config] write 0 bytes — disk full?");
        return false;
    }
    f.close();
    LittleFS.remove(PATH_CONFIG);
    if (!LittleFS.rename(PATH_CONFIG_TMP, PATH_CONFIG)) {
        Serial.println("[config] rename failed");
        return false;
    }
    return true;
}

const Data& current() { return s_current; }
void setCurrent(const Data& cfg) { s_current = cfg; }

}  // namespace Config

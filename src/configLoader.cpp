#include "configLoader.h"
#include <ctype.h>
#include <stdlib.h>

// ── Compile-time fallback config ─────────────────────────────────────────────
// Used when /config.json is absent or unreadable (e.g. right after migrating
// SPIFFS → LittleFS, when the data partition gets reformatted empty). Baking the
// per-board network settings in via build_flags means a wiped filesystem can
// never take a board offline — so OTA migration is safe without USB.
// Override per board in platformio.ini, e.g.:
//   build_flags = -DDEFAULT_MDNS=\"window2\" -DDEFAULT_IP=\"10.0.0.102\"
#ifndef DEFAULT_MDNS
  #define DEFAULT_MDNS "window1"
#endif
#ifndef DEFAULT_IP
  #define DEFAULT_IP "10.0.0.101"
#endif
#ifndef DEFAULT_GATEWAY
  #define DEFAULT_GATEWAY "10.0.0.1"
#endif
#ifndef DEFAULT_SUBNET
  #define DEFAULT_SUBNET "255.255.255.0"
#endif
#ifndef DEFAULT_REVERSE0
  #define DEFAULT_REVERSE0 false
#endif
#ifndef DEFAULT_REVERSE1
  #define DEFAULT_REVERSE1 true
#endif

IPAddress ip;
IPAddress gateway;
IPAddress subnet_mask;
const char* DNSName;
bool reverseStepper0 = false;
bool reverseStepper1 = false;
uint16_t dmxAddress = 1;   // default: window 1 (channels 1-4)

static char mdnsNameBuffer[32];   // persistent storage for DNSName

// Apply settings from `doc`; any missing key falls back to the compile-time
// default, so an empty doc yields a fully valid (baked-in) configuration.
static void applyConfig(JsonDocument& doc) {
    strlcpy(mdnsNameBuffer, doc["mdnsName"] | DEFAULT_MDNS, sizeof(mdnsNameBuffer));
    DNSName = mdnsNameBuffer;

    ip.fromString(doc["ipadress"] | DEFAULT_IP);
    gateway.fromString(doc["gateway"] | DEFAULT_GATEWAY);
    subnet_mask.fromString(doc["subnet"] | DEFAULT_SUBNET);

    reverseStepper0 = doc["reverseStepper0"] | DEFAULT_REVERSE0;
    reverseStepper1 = doc["reverseStepper1"] | DEFAULT_REVERSE1;

    // DMX start channel (1-based): window reads dmxAddress..dmxAddress+3.
    // Layout: Window1=1, Window2=5, Window3=9, Window4=13.
    // Auto-derived from the number in mdnsName ("window3" -> 9) unless an
    // explicit "dmxAddress" is given in config.
    uint16_t cfgAddr = doc["dmxAddress"] | 0;
    if (cfgAddr >= 1) {
        dmxAddress = cfgAddr;
    } else {
        int windowNum = 1;
        const char* p = mdnsNameBuffer;
        while (*p && !isdigit((unsigned char)*p)) p++;   // skip to first digit
        if (*p) windowNum = atoi(p);
        if (windowNum < 1) windowNum = 1;
        dmxAddress = (uint16_t)((windowNum - 1) * 4 + 1);
    }
}

bool loadConfiguration() {
    Serial.println("config loading");
    JsonDocument doc;            // empty → applyConfig uses compile-time defaults
    bool fromFile = false;

    if (LittleFS.begin(true)) {
        File configFile = LittleFS.open("/config.json");
        if (configFile) {
            DeserializationError error = deserializeJson(doc, configFile);
            configFile.close();
            if (error) {
                Serial.println("config.json parse failed — using compiled-in defaults");
                doc.clear();
            } else {
                fromFile = true;
            }
        } else {
            Serial.println("config.json not found — using compiled-in defaults");
        }
    } else {
        Serial.println("LittleFS mount failed — using compiled-in defaults");
    }

    applyConfig(doc);
    Serial.printf("config %s | mDNS %s | DMX %u\n",
                  fromFile ? "from file" : "DEFAULTS", DNSName, dmxAddress);
    return true;   // always returns a valid config (file or baked-in)
}

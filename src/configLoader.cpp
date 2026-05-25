#include "configLoader.h"
#include <ctype.h>
#include <stdlib.h>


IPAddress ip;
IPAddress gateway;
IPAddress subnet_mask;
const char* DNSName;
bool reverseStepper0 = false;
bool reverseStepper1 = false;
uint16_t dmxAddress = 1;   // default: window 1 (channels 1-4)

bool loadConfiguration() {
    Serial.println("config loading");
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return false;
    }

    File configFile = SPIFFS.open("/config.json");
    if (!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }
    JsonDocument doc;
    auto error = deserializeJson(doc, configFile);
    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }

    // Make a persistent copy of the string
    static char mdnsNameBuffer[32]; // Buffer to store the mdnsName
    strlcpy(mdnsNameBuffer, doc["mdnsName"] | "window1", sizeof(mdnsNameBuffer));
    DNSName = mdnsNameBuffer;
    const char* ch_configIPaddress = doc["ipadress"];
    const char* ch_configGateway = doc["gateway"];
    const char* ch_configSubnet = doc["subnet"];

    
    
    ip.fromString(ch_configIPaddress);
    
    gateway.fromString(ch_configGateway);
    
    subnet_mask.fromString(ch_configSubnet);

    // Load stepper direction reversal flags (default to false if not present)
    reverseStepper0 = doc["reverseStepper0"] | false;
    reverseStepper1 = doc["reverseStepper1"] | false;

    // DMX start channel (1-based): window reads dmxAddress..dmxAddress+3.
    // Layout: Window1=1, Window2=5, Window3=9, Window4=13.
    // Auto-derived from the number in mdnsName ("window3" -> 9) so no extra
    // per-device config is needed; an explicit "dmxAddress" in config overrides.
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
    Serial.printf("DMX address: %u (from %s)\n", dmxAddress, mdnsNameBuffer);

    return true;
}
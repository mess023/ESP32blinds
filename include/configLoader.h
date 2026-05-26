#pragma once

#include <ArduinoJson.h>
#include <LittleFS.h>   // SPIFFS is not in this (Tasmota-built) framework; LittleFS is the replacement



extern const char* DNSName;
extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet_mask;
extern bool reverseStepper0;
extern bool reverseStepper1;
extern uint16_t dmxAddress;   // DMX start channel (1-512); this window reads dmxAddress..dmxAddress+3

bool loadConfiguration();
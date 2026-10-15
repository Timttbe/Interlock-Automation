#pragma once
#include "Arduino.h"
#include "IPAddress.h"

struct WiFiSim {
  int RSSI() { return -55; }
};
extern WiFiSim WiFi;

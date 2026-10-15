#pragma once
// Tudo que o firmware inclui do "sistema", incluído fora do namespace de cada placa.
#include <stdarg.h>
#include <time.h>
#include "Arduino.h"
#include "IPAddress.h"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "Preferences.h"
#include "WebServer.h"
#include "esp_task_wdt.h"
#include "mbedtls/md.h"

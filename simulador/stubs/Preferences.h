#pragma once
// Preferences em memória (some quando a "placa" é recarregada, como se fosse gravada do zero).
#include <map>
#include "Arduino.h"

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  void end() {}
  uint32_t getUInt(const char* k, uint32_t p) { return tem(k) ? std::stoul(m()[k]) : p; }
  uint16_t getUShort(const char* k, uint16_t p) { return tem(k) ? std::stoul(m()[k]) : p; }
  uint8_t getUChar(const char* k, uint8_t p) { return tem(k) ? std::stoul(m()[k]) : p; }
  bool getBool(const char* k, bool p) { return tem(k) ? m()[k] == "1" : p; }
  size_t getString(const char* k, char* v, size_t n) {
    if (!tem(k)) return 0;
    strlcpy(v, m()[k].c_str(), n);
    return m()[k].size();
  }
  void putUInt(const char* k, uint32_t v) { m()[k] = std::to_string(v); }
  void putUShort(const char* k, uint16_t v) { m()[k] = std::to_string(v); }
  void putUChar(const char* k, uint8_t v) { m()[k] = std::to_string(v); }
  void putBool(const char* k, bool v) { m()[k] = v ? "1" : "0"; }
  void putString(const char* k, const char* v) { m()[k] = v; }

 private:
  static std::map<std::string, std::string>& m() {
    static std::map<std::string, std::string> mapa[3];  // um por placa (sobrevive ao reinício)
    return mapa[sim::dispositivo()];
  }
  bool tem(const char* k) { return m().count(k) > 0; }
};

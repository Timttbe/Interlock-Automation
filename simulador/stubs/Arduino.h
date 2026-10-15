#pragma once
// Arduino mínimo para simular o firmware no PC. Tudo que toca hardware vai para o host (sim_host.h).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <string>
#include "sim_host.h"

#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

inline size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t n = strlen(src);
  if (size) {
    size_t c = n >= size ? size - 1 : n;
    memcpy(dst, src, c);
    dst[c] = 0;
  }
  return n;
}

class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  String& operator+=(int v) { s_ += std::to_string(v); return *this; }
  String& operator+=(unsigned v) { s_ += std::to_string(v); return *this; }
  String& operator+=(long v) { s_ += std::to_string(v); return *this; }
  String& operator+=(unsigned long v) { s_ += std::to_string(v); return *this; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == o; }
  bool operator!=(const char* o) const { return s_ != o; }
  void reserve(size_t n) { s_.reserve(n); }
  std::string s_;
};
inline String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
inline String operator+(const String& a, const char* b) { return String(a.s_ + b); }
inline String operator+(const char* a, const String& b) { return String(std::string(a) + b.s_); }
inline String operator+(const String& a, char b) { return String(a.s_ + b); }

inline unsigned long millis() { return sim::host()->millis(sim::dispositivo()); }
inline void delayMicroseconds(unsigned) {}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t pino) { return sim::host()->digitalRead(sim::dispositivo(), pino); }
inline void digitalWrite(uint8_t pino, uint8_t v) { sim::host()->digitalWrite(sim::dispositivo(), pino, v); }
inline uint32_t esp_random() { return sim::host()->aleatorio(); }
inline void enableLoopWDT() {}
inline void feedLoopWDT() {}

struct SerialSim {
  void begin(unsigned long) {}
  void flush() {}
  int printf(const char* f, ...) __attribute__((format(printf, 2, 3))) {
    char b[512];
    va_list a;
    va_start(a, f);
    int n = vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    sim::host()->log(sim::dispositivo(), b);
    return n;
  }
};
extern SerialSim Serial;

struct EspSim {
  void restart() { sim::host()->reiniciar(sim::dispositivo()); }
  uint32_t getFreeHeap() { return 200000; }
};
extern EspSim ESP;

typedef enum { ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
               ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO } esp_reset_reason_t;
inline esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

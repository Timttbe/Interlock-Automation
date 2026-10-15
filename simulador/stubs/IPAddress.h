#pragma once
#include "Arduino.h"

class IPAddress {
 public:
  IPAddress() : ip_(0) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : ip_(a | (b << 8) | (c << 16) | ((uint32_t)d << 24)) {}
  IPAddress(uint32_t ip) : ip_(ip) {}
  operator uint32_t() const { return ip_; }
  uint8_t operator[](int i) const { return (ip_ >> (8 * i)) & 0xFF; }
  bool operator==(const IPAddress& o) const { return ip_ == o.ip_; }
  bool operator!=(const IPAddress& o) const { return ip_ != o.ip_; }
  String toString() const {
    char b[20];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", (*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    return String(b);
  }

 private:
  uint32_t ip_;
};

#pragma once
#include "Arduino.h"
#include "IPAddress.h"

class WiFiUDP {
 public:
  uint8_t begin(uint16_t) { return 1; }
  int beginPacket(IPAddress ip, uint16_t) {
    destino_ = ip;
    tx_.clear();
    return 1;
  }
  size_t write(const uint8_t* d, size_t n) {
    tx_.append(reinterpret_cast<const char*>(d), n);
    return n;
  }
  int endPacket() {
    sim::host()->udpEnviar(sim::dispositivo(), destino_, reinterpret_cast<const uint8_t*>(tx_.data()), tx_.size());
    return 1;
  }
  int parsePacket() {
    uint32_t de = 0;
    int n = sim::host()->udpReceber(sim::dispositivo(), rx_, sizeof(rx_), &de);
    if (n <= 0) return 0;
    rxTam_ = n;
    rxPos_ = 0;
    origem_ = de;
    return n;
  }
  int read(char* buf, size_t n) {
    size_t c = rxTam_ - rxPos_ < n ? rxTam_ - rxPos_ : n;
    memcpy(buf, rx_ + rxPos_, c);
    rxPos_ += c;
    return c;
  }
  IPAddress remoteIP() { return origem_; }
  void flush() { rxTam_ = rxPos_ = 0; }

 private:
  IPAddress destino_;
  std::string tx_;
  uint8_t rx_[1500];
  size_t rxTam_ = 0, rxPos_ = 0;
  IPAddress origem_;
};

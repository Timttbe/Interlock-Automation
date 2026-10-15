# Interlock Automation – ESP8266 version (v6.0)

Previous generation of the firmware, kept for reference. It ran on three **NodeMCU (ESP8266)** boards
built from a single Arduino IDE sketch (`main.ino`, role chosen with `#define DEVICE_NAME`).

In that design the PORTEIRO created its own Wi-Fi AP (`PORTEIRO_AP`) and the doors connected to it;
all messages were plain-text UDP broadcasts (`OPEN|PORTA_A`, `STATUS|...`, `BYPASS|ON`).

It was replaced by the ESP32 firmware at the root of this repository (PlatformIO project), which
uses the building's router, an authenticated protocol, the PORTEIRO as interlock arbiter, a web page
on every board and over-the-air updates. See the main [README](../README.md) for what changed and why.

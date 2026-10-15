# Interlock Automation Project

![Main Device](https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif)
![Framework](https://img.shields.io/badge/Framework-Arduino%20%2B%20PlatformIO-orange?logo=platformio)
![Version](https://img.shields.io/badge/Firmware-v7.0.0-informational)
![Status](https://img.shields.io/badge/Status-Active-success)

---

## Overview

**Wires Interlock** is a two-door interlock (security vestibule) built with three **ESP32** boards
and 4-channel relay modules, talking over the building's Wi-Fi.

**Only one door can be unlocked or open at a time.** A door opens by face recognition, by its own
button, or by the doorman (physical buttons or the web page). While door A is unlocked, or its sensor
says it is open (even after the opening time is over), door B stays locked, and vice versa. A bypass
switch lets both doors open together. If a door stays open too long, a buzzer sounds at the doorman's
desk until it closes.

| Board | Inputs | Outputs |
|---|---|---|
| **PORTEIRO** (doorman) | button "open A", button "open B", bypass switch | buzzer A, buzzer B |
| **PORTA_A** / **PORTA_B** (doors) | door sensor, face reader (dry contact), push button | electromagnetic lock, push/pull sign |

Every board has its own web page (status, virtual buttons, network settings, firmware update) and its
own fallback Wi-Fi network for when the router is unreachable.

---

## Demo

<p align="center">
  <a href="https://youtu.be/LeEWyhVxUkA">
    <img src="https://img.youtube.com/vi/LeEWyhVxUkA/0.jpg" width="600">
  </a>
</p>

---

## What changed from v6 (ESP8266)

| | v6 (ESP8266, `Prototype 2/`) | v7 (ESP32, this folder) |
|---|---|---|
| Network | Doors joined the PORTEIRO's own AP | All boards join the building router; each opens its own AP after 30 s without it and keeps retrying the router |
| Who decides | Each board, from broadcast status | The PORTEIRO is the single arbiter; doors double-check, and decide between themselves when the PORTEIRO is down |
| Simultaneous requests | Could unlock both doors | Serialized by the PORTEIRO; simulated 60× with and without packet loss |
| Security | Plain `OPEN|PORTA_A` UDP: anyone on the Wi-Fi could open a door; web page without password | Messages signed (HMAC-SHA256) with replay protection; web page and AP with password |
| Door left open | Not implemented | Buzzer per door at the PORTEIRO after a configurable time |
| Web page | PORTEIRO only, fonts from the internet, full reload every 4 s | Every board, works offline (fallback AP), live updates, event log |
| Updates | USB only | Over the network, from the page or from VS Code; a board refuses another role's firmware |
| Code | One 630-line `.ino` | PlatformIO project split in modules, unit tests and a full-system simulator |
| Other fixes | Bypass on GPIO2 (boot strapping pin); doors stuck in `setup()` until the AP appeared; state unknown after boot treated as "closed" | Safe pins only; nothing blocks the loop; watchdog; unknown state handled explicitly |

---

## System architecture

<div align="center">
<pre>
                    Building Wi-Fi router
          ┌───────────────┼───────────────┐
          │               │               │
   ┌──────┴──────┐ ┌──────┴──────┐ ┌──────┴──────┐
   │  PORTEIRO   │ │   PORTA_A   │ │   PORTA_B   │
   │  arbiter    │ │ lock+sensor │ │ lock+sensor │
   │ buttons,    │ │ face reader │ │ face reader │
   │ bypass,     │ │ button      │ │ button      │
   │ buzzers     │ │             │ │             │
   └─────────────┘ └─────────────┘ └─────────────┘
       UDP 4211: signed messages, state every second
</pre>
</div>

1. Face reader or button at door A → door A sends a **request** to the PORTEIRO.
2. The PORTEIRO checks door B (unlocked? open? being unlocked right now?) and the bypass, and answers.
3. Door A unlocks for the opening time (default 5 s) and lights the push/pull sign. It locks again
   when the time is over or, earlier, as soon as the door opens and closes.
4. Doors also tell each other their state directly, so each door can veto an unlock it knows is unsafe.

### When something fails

| Situation | Behaviour |
|---|---|
| Everything online | The PORTEIRO decides; doors double-check what they know of each other |
| PORTEIRO offline (no news for 5 s), doors still see each other | Doors decide between themselves. If both unlock at the same instant, one locks again within milliseconds (B yields; a door already open never re-locks) |
| PORTEIRO online but its answer got lost | The door **denies** ("try again") instead of guessing: deciding alone could unlock together with a door the PORTEIRO just released |
| PORTEIRO just rebooted (first 5 s) | Doors decide between themselves |
| A door has no news of the other one (router down, board off) | Configurable on the web page: **release anyway** (default, nobody gets trapped between doors, interlock not guaranteed) or **block** (interlock guaranteed, nobody passes until the network is back) |
| A board crashes or reboots | Relay off → lock energized. A hung program is reset by the watchdog after 15 s |

> ⚠️ The boards talk through the router. If the router goes down, each door is on its own and the
> "no news of the other door" rule applies. Put the router (and the boards' power supply) on a UPS.

---

## Hardware

Any ESP32 DevKit (ESP32-WROOM-32, 30 or 38 pins). All inputs use the internal pull-up: wire the
contact between the pin and **GND**. The pins avoid every ESP32 boot-strapping pin.

### PORTA_A / PORTA_B

| GPIO | Function | Notes |
|---|---|---|
| 32 | Door sensor | Normally-open reed closed by the magnet: closed door = LOW. A broken wire reads as *open* (safe side: blocks the other door) |
| 33 | Face reader | Dry contact (relay output) of the face terminal |
| 25 | Push button | |
| 26 | Relay 1 → electromagnetic lock | Wire the lock on the relay's **NC** contact: relay off = lock energized = locked |
| 27 | Relay 2 → push/pull sign | Lit while the door is unlocked |

### PORTEIRO

| GPIO | Function |
|---|---|
| 32 | Button "open door A" |
| 33 | Button "open door B" |
| 25 | Bypass switch (closed = bypass on) |
| 26 | Buzzer of door A (HIGH = sounding) |
| 27 | Buzzer of door B (HIGH = sounding) |

The buzzers are **active** buzzers wired straight to the pin (+ on the GPIO, − on GND). A GPIO
supplies about 20 mA at 3.3 V; for a louder 5 V/12 V buzzer, drive it through an NPN transistor
(e.g. BC547 with a 1 kΩ base resistor). The logic stays the same.

Common to all boards: **GPIO 2** status LED (on = Wi-Fi OK, slow blink = no Wi-Fi, fast blink = own
network on) and the **BOOT** button (hold 10 s until the LED blinks very fast, then release: restores
factory network settings).

On the doors, relay channels 3 and 4 are free. Pins, relay/buzzer active level and sensor polarity
are in [`include/config.h`](include/config.h).

> **Relay modules:** the firmware is set for a module that switches the relay on with the input
> at **LOW** (trigger jumper on **L**; `RELE_ATIVO_EM` in `config.h`). If a relay is on at rest and
> turns off when a door is released, the jumper and `RELE_ATIVO_EM` do not match. LOW-triggered
> modules powered at 5 V may not fully turn off with the ESP32's 3.3 V "HIGH": if a relay stays on,
> chatters or its LED glows dimly at rest, set the jumper to **H** and `RELE_ATIVO_EM` to `HIGH`.

### Power supply

- 5 V / 2 A per board; relay coils switching cause current spikes.
- A 100 µF–470 µF electrolytic capacitor between 5 V and GND near each ESP32.
- If the page shows *"último reinício: queda de tensão (brownout)"*, the supply is too weak.

---

## Getting started

### 1. Tools

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension.
- Open **this folder** (the one with `platformio.ini`) in VS Code.

### 2. Passwords

Copy `include/segredos.exemplo.h` to `include/segredos.h` (ignored by git) and change both values:

```cpp
#define SENHA_ADMIN "..."  // web page (user "admin"), fallback Wi-Fi and network updates. 8–63 chars
#define CHAVE_REDE  "..."  // signs the messages between boards. 16+ chars
```

All three boards must be built with the same `segredos.h`. While the example values are in use, every
page shows a red warning.

### 3. Flash each board (USB)

In the PlatformIO sidebar: **Project Tasks → `porteiro` / `porta_a` / `porta_b` → Upload**, or:

```bash
pio run -e porta_a -t upload
```

The role is chosen by the environment; the same code goes on all three boards.

### 4. First boot

With no Wi-Fi configured, each board opens its own network:

| Board | Network | Page |
|---|---|---|
| PORTEIRO | `Interlock-Portaria` | `http://192.168.4.1` |
| PORTA_A | `Interlock-PortaA` | `http://192.168.4.1` |
| PORTA_B | `Interlock-PortaB` | `http://192.168.4.1` |

Network password and page password: `SENHA_ADMIN` (user `admin`). In **Rede**, pick the router
(**Procurar redes**), type its password, choose automatic IP or a fixed IP/mask/gateway, and save.
The board restarts and joins the router. Do the same on the other two boards.

Then open any board by its router IP or by name (`http://interlock-portaria.local`,
`http://interlock-porta-a.local`, `http://interlock-porta-b.local`). The PORTEIRO's page lists all
three boards with links to each one.

> Use `http://`, not `https://`: the ESP32 does not serve HTTPS.

---

## Web interface

**PORTEIRO:** a card per door (locked / unlocked with countdown / open / open-too-long alarm / no
communication, whether it may open right now and why not), **ABRIR** buttons, bypass state (switch +
web toggle), inputs and buzzers, the rule for a door without communication, the list of boards, and
the event log.

**Doors:** door state, **ABRIR** button (same path as the physical button), communication with the
PORTEIRO and the other door, live inputs (sensor, face reader, button, with a warning when a contact
looks stuck), relays, opening time, alarm time and the "no news of the other door" rule.

**All boards:** Wi-Fi status, network settings (with network scan), event log with time (when the
router has internet), firmware update, restart. The page refreshes itself every second and works
without internet (nothing is loaded from outside the board).

---

## Network behaviour

- Normal: connected to the router, automatic or fixed IP (mask, gateway and optional DNS).
- The board's own network (`Interlock-...`, `192.168.4.1`) turns on only:
  - when no Wi-Fi is configured;
  - after **30 s** without the router.
  Once the router is back it turns off (after up to 2 min if someone is still connected to it).
- Without the router, the board keeps retrying it forever: every 15 s, or every 60 s while someone
  is connected to its own network (each attempt makes that network unstable for ~2 s).
- A wrong fixed IP (the board joins the router but cannot be reached): hold the BOOT button.
  `AP_NO_BOOT_MS` in `config.h` can also keep the own network on for a few minutes after power-up.
- **Group** (1–99): only needed if two interlock sets share the same network. The three boards of a
  set must have the same group.
- Factory reset of the network settings: BOOT button (see Hardware).

The router must allow Wi-Fi clients to talk to each other (no "AP/client isolation") and must
forward broadcasts between them (default on home and small-business routers).

---

## Firmware updates over the network

- **From the page:** *Manutenção → Firmware*, choose `.pio/build/<environment>/firmware.bin`.
- **From VS Code:** set the board's IP in `upload_port` of the `*_ota` environment in
  `platformio.ini`, create an environment variable `INTERLOCK_SENHA` with the `SENHA_ADMIN`, and run
  **Project Tasks → `porta_a_ota` → Upload**.

Each board only accepts its own role's firmware (a marker inside the binary is checked), so the
PORTEIRO firmware cannot be flashed on a door by mistake. During the upload the board stops (door
locked, buzzers off) and restarts with the new version at the end. If anything fails, it keeps the
current firmware.

> Changing the partition table requires USB. The project uses `min_spiffs.csv` (two 1.9 MB app slots).

---

## Security

- Web page, fallback network and network updates require `SENHA_ADMIN` (HTTP Basic auth over plain
  HTTP on the local network).
- Every message between boards is signed with HMAC-SHA256 (`CHAVE_REDE`). A device on the network
  without the key cannot open a door.
- Replay protection: each board picks a random session at boot and numbers its messages; a recorded
  message is refused later, also after either board reboots (see `src/sessao.h`).
- Messages with a wrong signature are counted on the page ("mensagens recusadas"), which usually
  means one board was built with a different `CHAVE_REDE`.

---

## Protocol reference

UDP port `4211`, text messages:

```
ITL1|group|origin|session|seq|echo P,echo A,echo B|type|data|signature
```

| Type | Direction | Data |
|---|---|---|
| `E` state | every board → all (broadcast every 1 s, plus 3× direct when it changes) | door: `lib` unlocked, `abe` open, `alm` alarm, `rest` ms left, `ha` s open, `lbp` bypass release, `ori` origin, `modo`; PORTEIRO: `byp`, `chv`, `vir`; all: `rssi`, `fw`, `up` |
| `Q` request | door → PORTEIRO | `id`, `ori` |
| `L` release | PORTEIRO → door | `id`, `byp`, `ori` |
| `R` answer | reply to `Q` / `L` | `id`, `ok`, `mot` (reason), `byp` |

Requests and commands are repeated every 150 ms until answered; a repeated `id` gets the same answer
without acting twice.

---

## Configuration reference

Changed on the web page and kept in flash: Wi-Fi, IP, group, opening time (1–60 s, default 5),
alarm time (5–3600 s, default 60), "no news of the other door" rule.

Compile-time, in [`include/config.h`](include/config.h): pins, relay/buzzer/sensor levels, factory
Wi-Fi, default times, relock-on-close (`TRAVAR_AO_FECHAR`), intermittent buzzer
(`BUZZER_INTERMITENTE`), network timings.

---

## Testing

- **Unit tests** of the interlock rules and replay protection (`test/test_intertravamento`):
  `pio test -e testes` on any ESP32 connected by USB, or `pio test -e testes_pc` on a PC with gcc.
- **Simulator** ([`simulador/`](simulador/README.md)): runs the real firmware of the three boards on
  the PC over a simulated network with packet loss, reboots, attacks and clock overflow (20
  scenarios). Also builds the web pages for viewing in a browser without hardware.

---

## Project structure

```
platformio.ini            environments: porteiro, porta_a, porta_b (+ *_ota, testes)
include/
  config.h                pins, levels, times
  segredos.exemplo.h      passwords template (copy to segredos.h)
src/
  main.cpp                setup/loop
  porta.cpp               door logic          porteiro.cpp   doorman logic
  intertravamento.cpp     interlock rules (pure, unit tested)
  protocolo.cpp           signed UDP messages sessao.cpp     replay protection (pure, unit tested)
  rede_wifi.cpp           router, fixed IP, fallback network
  web.cpp                 common routes       web_porta.cpp / web_porteiro.cpp  role routes
  pagina_*.h              web page (HTML/CSS/JS)
  atualizacao.cpp         firmware upload     configuracao.cpp  settings in flash
  registro.cpp            event log           sistema.cpp       LED, BOOT button, watchdog
  entradas.cpp            debounced inputs, outputs
test/                     unit tests
simulador/                full-system simulator
Prototype 1/              ESP-01 + ESP-NOW prototype
Prototype 2/              ESP8266 v6 firmware (previous generation)
```

---

## Future improvements

- ESP-NOW as a second channel between the boards, so the interlock survives a router outage
- Event log kept across reboots
- MQTT / remote monitoring

---

## Author

Developed by **Davi Han Ko** — [Wires Solução e Serviços](https://github.com/Wires-Solucao-e-Servicos)

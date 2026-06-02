# Atl-1 Flight Computer — v2.1

> Avionics system for experimental water-powered rockets.
> Real-time telemetry · Triple-redundant apogee detection · Wi-Fi ground station · 7-state flight FSM · Onboard LittleFS black-box logging

[![Status](https://img.shields.io/badge/status-First%20Flight%20Complete-brightgreen)](https://github.com/DonJechu/HydroRocket-Telemetry-System)
[![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Research](https://img.shields.io/badge/research-EMI%20Mitigation%20%2F%20Avionics-purple)](docs/research/)

---

## Overview

**Atl-1** is a flight computer for water-powered PET bottle rockets. It is the hardware validation platform for ongoing research into **EMI mitigation in compact low-cost avionics**, comparing shielded vs. unshielded configurations under real flight conditions.

- Real-time telemetry over Wi-Fi WebSocket at 10 Hz
- Triple-redundant apogee detection (velocity zero-crossing + altitude drop + safety timeout)
- Hardware IIR pressure filtering on BMP280 to suppress aerodynamic noise
- Parachute deployment via servo at confirmed apogee
- Onboard black-box logging to flash via LittleFS — survives WiFi loss

> **Status:** First instrumented flight completed — **June 1, 2026, Veracruz, Mexico.**

---

## Flight Results — Atl-1

**Date:** June 1, 2026 — Veracruz, México  
**Launch site:** Open field — pressure estimated, no gauge available

| Metric | Value |
|---|---|
| Apogee (barometric) | ~14 m |
| Peak velocity | ~4.5 m/s |
| Peak G-Force (liftoff) | ~1.6 G |
| Flight duration | ~30 s |
| Parachute deployment | ❌ Not triggered — see note |
| LittleFS data recovery | ✅ CSV recovered via Serial DUMP post-landing |

> **Note on parachute:** `LIFTOFF_G` was set to 2.5G. Actual peak was 1.6G (low pressurization, no gauge). FSM remained in STANDBY throughout the flight. **Fix for Atl-2:** threshold lowered to 1.2G.

**Raw flight data:** [`data/flights/atl1_flight_20260601.csv`](data/flights/atl1_flight_20260601.csv)

![Flight Data](media/atl1_flight_20260601_graphics.png)

---

## Research Context

Experimental platform for a paper on **EMI mitigation in compact low-cost avionics** — to be submitted to IEEE Latin America Transactions.

**Hypothesis:** *"EMI mitigation techniques (twisted pairs, star ground, LC filtering, spatial separation) significantly reduce I²C sensor degradation in compact ESP32-based avionics."*

| Config A — Unmitigated | Config B — Mitigated |
|---|---|
| Parallel untwisted cables | Twisted pairs on all signal lines |
| Daisy-chain ground | Star ground |
| No LC filter on MT3608 | LC filter (47µH + 100µF) post-boost converter |
| No decoupling caps | 100nF + 10µF per sensor node |

Metrics: IMU RMS noise · I²C packet loss rate · barometric altitude drift · servo EMI events

> Claims are limited to measured deltas between configurations — not theoretical validation of electromagnetic models.

---

## Hardware

| Component | Part | Notes |
|---|---|---|
| Microcontroller | ESP32-C3 Super Mini (USB-C) | Wi-Fi AP, WebSocket server |
| IMU | GY-521 module — chip: **MPU-6500** (WHO_AM_I = 0x70) | ±16G / ±2000°/s — see library patch below |
| Barometer | BMP280 (0x76) | Hardware IIR FILTER_X16 enabled |
| Power Boost | MT3608 DC-DC step-up | LiPo 3.7V → 5V |
| Battery Charger | HW-373 (TP4056 + DW01) | USB-C charging with simultaneous load support |
| Battery | LiPo 1S — 3.7V / 600–700 mAh | Minimum flight voltage: 3.0V |
| Actuator | Servo SG90 (GPIO 13) | Parachute deployment |
| Switch | SPDT (GPIO 10) | Master power |
| Buzzer | Piezoelectric active (GPIO 3) | State audio feedback |

**I²C bus:** SDA → GPIO 21, SCL → GPIO 22. Pull-ups: 5.1 kΩ to 3V3.

**Power path:**
```
USB-C ──► HW-373 ──► LiPo 3.7V ──► MT3608 ──► 5V ──► ESP32-C3
                                              └──► 3.3V LDO ──► Sensors
```

> WiFi is disabled on LANDING to reduce idle current from ~180mA to ~20mA.

**Avionics Bay design history (v1 → v4):** [`docs/avionics_bay_evolution.md`](docs/avionics_bay_evolution.md)

---

## Flight State Machine

```
              gForce ≥ 1.2G (4 consecutive)
┌──────────┐ ────────────────────────────► ┌──────────┐
│ STANDBY  │                               │  ASCENT  │
└──────────┘                               └──────────┘
                                                │
                           ┌────────────────────┼────────────────────┐
                           │                    │                    │
                     vel ≤ 0              alt drops 1m          timeout
                    (5 samples)            (4 samples)          12,000 ms
                           └────────────────────┼────────────────────┘
                                                ▼
                                           ┌──────────┐
                                           │  APOGEE  │  servo → 90°
                                           └──────────┘
                                                │
                                           ┌──────────┐
                                           │ DESCENT  │
                                           └──────────┘
                                                │  alt < 0.75 m
                                           ┌──────────┐
                                           │ LANDING  │  WiFi OFF · log closed
                                           └──────────┘
```

| State | ID | Entry condition | Action |
|---|---|---|---|
| `STANDBY` | 0 | Boot | Transmit, log, wait for liftoff |
| `ARMED` | 1 | *(future — remote arm)* | — |
| `IGNITION` | 2 | *(future)* | — |
| `ASCENT` | 3 | gForce ≥ 1.2G × 4 | Track maxAltitude, evaluate apogee |
| `APOGEE` | 4 | Triple-redundant trigger | `servo.write(90)` |
| `DESCENT` | 5 | Post-apogee | Monitor alt, log |
| `LANDING` | 6 | alt < 0.75 m | Close log · disable WiFi |

---

## Onboard Logging — LittleFS

Each boot creates a new file: `/flight_1.csv`, `/flight_2.csv`, ... (auto-increment, never overwrites).

**Capacity:** ~60 bytes/row × 10 Hz × 60s ≈ 36 KB/flight. ~1.5 MB available → ~40 flights before FORMAT.

### Serial Recovery (115200 baud)

| Command | Action |
|---|---|
| `LIST` | List all files and sizes |
| `DUMP` | Dump all CSV files to Serial |
| `DUMP:flight_1.csv` | Dump specific file |
| `FORMAT` | Erase all log files |

---

## Sensor Calibration

BMP280 configured with hardware IIR FILTER_X16 at ~28 Hz. Boot sequence:

1. **100 reads discarded** — IIR filter needs ~70 samples to converge from cold start
2. **Reads 101–200 averaged** → `basePressure`
3. **50 samples averaged** → `groundAltitude` offset (zero reference)

```cpp
rawAlt = bmp.readAltitude(basePressure) - groundAltitude;
filteredAlt = filteredAlt * 0.8f + rawAlt * 0.2f;
```

> Calibration must be done **outdoors at the launch site**, stationary and uncapped. Indoor calibration causes negative altitude readings.

---

## Build & Flash

### Arduino Dependencies

```
Adafruit MPU6050     — requires WHO_AM_I patch (see below)
Adafruit BMP280
Adafruit Sensor      (dependency)
ESP32Servo
WebSockets           by Markus Sattler
LittleFS             (ESP32 built-in)
```

### MPU-6500 Library Patch (required)

The GY-521 module on this build contains an **MPU-6500** (WHO_AM_I = 0x70), not an MPU-6050 (0x68). Apply this one-line fix:

**File:** `Arduino/libraries/Adafruit_MPU6050/Adafruit_MPU6050.cpp` — line ~93

```cpp
// BEFORE:
if (chip_id.read() != MPU6050_DEVICE_ID) {

// AFTER:
if (chip_id.read() != MPU6050_DEVICE_ID && chip_id.read() != 0x70) {
```

### Flash Settings

- **Board:** ESP32C3 Dev Module
- **Partition Scheme:** `Default 4MB with spiffs (1.2MB App/OTA, 1.5MB SPIFFS)` ← required for LittleFS
- **Baud:** 115200

### Pre-Launch Checklist

- [ ] Power on **outdoors at launch site**, stationary and uncapped
- [ ] Wait for `=== READY FOR FLIGHT ===` (~2.6s calibration)
- [ ] Ground offset printed — confirm within ±1.0 m
- [ ] `LIST` via Serial → confirm new `/flight_N.csv` created
- [ ] Ground station connected — altitude reads near 0 m
- [ ] Only **one browser tab** open (multiple clients = duplicate CSV rows)
- [ ] Servo confirmed at 0° (parachute closed)
- [ ] LiPo at 4.2V (HW-373 blue LED solid)
- [ ] Cap and pressurize **after** READY confirmation

---

## Repository Structure

```
Atl-Flight_Computer/
├── firmware/
│   └── hidrorocket_v3.ino
├── data/flights/
│   └── atl1_flight_20260601.csv
├── analysis/
│   └── graphics.py
├── hardware/
│   └── Schematic_HidroRocket_v0_2026-04-26.pdf
├── mechanical/
│   └── AvionicsBay_v4.stl
├── docs/
│   └── avionics_bay_evolution.md
├── media/
└── README.md
```

---

## Known Issues & Roadmap

### Resolved — v2.1
| Issue | Status |
|---|---|
| LittleFS black-box logging | ✅ Implemented |
| BMP280 IIR warm-up before calibration | ✅ Fixed |
| Ground altitude zero offset | ✅ Fixed |
| MPU-6500 WHO_AM_I library rejection | ✅ Patched |
| WiFi power drain post-landing | ✅ WiFi off on LANDING |

### Pending — Atl-2
| Item | Priority |
|---|---|
| Lower `LIFTOFF_G` to 1.2G (1.6G measured on Atl-1) | 🔴 High |
| Verify CG/CP with electronics in OpenRocket (tumble at apogee) | 🔴 High |
| Pressure gauge for launch | 🔴 High |
| Replace BMP280 → MS5611 (±0.1m vs ±8m) | 🟡 Medium |
| A/B EMI bench experiment (10 reps × 2 configs) | 📄 Paper |

---

## Author

**Jesús Alberto Perea García**  
Mechatronics Engineering — IEST Anáhuac, Tamaulipas  
Founder — Vértice Labs Research Program  
[github.com/DonJechu](https://github.com/DonJechu)

*Atl-1 is the validation platform for experimental research on EMI mitigation in compact low-cost avionics. Paper in preparation for IEEE Latin America Transactions.*

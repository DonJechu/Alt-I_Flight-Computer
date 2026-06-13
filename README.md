# Atl-1 Flight Computer — v2.1

> Avionics system for experimental water-powered rockets.
> Real-time telemetry · Triple-redundant apogee detection · Wi-Fi ground station · 7-state flight FSM · Onboard LittleFS black-box logging

[![Status](https://img.shields.io/badge/status-First%20Flight%20Complete-brightgreen)](https://github.com/DonJechu/Atl-I_Flight-Computer)
[![Platform](https://img.shields.io/badge/platform-ESP32%20DevKit-blue)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Research](https://img.shields.io/badge/research-EMI%20Characterization%20%2F%20Avionics-purple)](#research-context)

---

## Overview

**Atl-1** is a flight computer for water-powered PET bottle rockets. It is the hardware validation platform for ongoing research into **replicable low-cost EMI characterization methodology for student avionics** — quantifying the effect of conducted and radiated noise sources on I²C sensors using a factorial 2×2 experimental design with A/A validation.

- Real-time telemetry over Wi-Fi WebSocket at 10 Hz
- Triple-redundant apogee detection (velocity zero-crossing + altitude drop + safety timeout)
- Hardware IIR pressure filtering on BMP280 to suppress aerodynamic noise
- Passive gravity parachute recovery on the flight-1 build — servo-based deployment logic is present in firmware, but the servo was omitted to save mass (see Hardware)
- Onboard black-box logging to flash via LittleFS — survives WiFi loss

> **Status:** First instrumented flight completed — **June 1, 2026, Veracruz, Mexico.**

---

## Flight Results — Atl-1

**Date:** June 1, 2026 — Veracruz, México  
**Launch site:** Open field — pressure estimated, no gauge available

![Atl-1 on launch pad](media/Prototype%20Gallery/atl1_rocket_on_pad.jpg)

| Metric | Value |
|---|---|
| Apogee (barometric) | ~14 m |
| Peak velocity | ~4.5 m/s |
| Peak G-Force (liftoff) | ~1.6 G |
| Flight duration | ~30 s |
| Parachute deployment | ❌ Not triggered — see note |
| LittleFS data recovery | ✅ CSV recovered via Serial DUMP post-landing |

> **Note on parachute:** `LIFTOFF_G` is set to 2.5G. Actual peak was 1.6G — rocket was under-pressurized (no gauge available). FSM remained in STANDBY throughout the flight. **Fix for Atl-2:** proper pressurization with calibrated gauge.

**Raw flight data:** [`flights/atl1_flight_20260601.csv`](flights/atl1_flight_20260601.csv)

![Flight Data](media/Prototype%20Gallery/atl1_flight1_analysis.png)
![Atl-1 in flight](media/Prototype%20Gallery/atl1_flight_vid.gif)


---

## Research Context

Atl-1 also serves as the flight demonstrator for an ongoing study on EMI behavior in compact low-cost avionics. The experimental design, dataset, and results are in preparation and will be released together with the publication.

---

## Hardware

| Component | Part | Notes |
|---|---|---|
| Microcontroller | ESP32 DevKit (standard) | Wi-Fi AP, WebSocket server |
| IMU | GY-521 module — chip: **MPU-6500** (WHO_AM_I = 0x70) | ±16G / ±2000°/s — see library patch below |
| Barometer | BMP280 (0x76) | Hardware IIR FILTER_X16 enabled |
| Power Boost | MT3608 DC-DC step-up | LiPo 3.7V → 5V |
| Battery Charger | HW-373 (TP4056 + DW01) | USB-C charging with simultaneous load support |
| Battery | LiPo 1S — 3.7V / 1200 mAh | Minimum flight voltage: 3.0V |
| Actuator | *(none on flight-1)* — SG90 on GPIO 13 is **firmware-defined only** | Flight-1 flew **passive gravity recovery**; physical servo omitted to save mass — planned for Atl-2 |
| Switch | SPDT (GPIO 10) | Master power |
| Buzzer | Piezoelectric active (GPIO 3) | State audio feedback |

**I²C bus:** SDA → GPIO 21, SCL → GPIO 22. Pull-ups: 5.1 kΩ to 3V3.

**Power path:**
```
USB-C ──► HW-373 ──► LiPo 3.7V ──► MT3608 ──► 5V ──► ESP32 DevKit (5V pin)
                                                      └──► DevKit onboard LDO ──► 3V3 ──► Sensors
```

> **As-built decoupling:** one electrolytic cap at the MT3608 VOUT and one ceramic cap at the sensor supply. **Atl-1 has no LC filter** — the LC filter belongs to the bench experiment's noisy config, not this flight build.

> WiFi is disabled on LANDING to reduce idle current from ~180mA to ~20mA.

**Avionics Bay design history (v1 → v4):** [`docs/docs_avionics_bay_evolution.md`](docs/docs_avionics_bay_evolution.md)

---

## Flight State Machine

```
              gForce ≥ 2.5G (4 consecutive)
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
| `ASCENT` | 3 | gForce ≥ 2.5G × 4 | Track maxAltitude, evaluate apogee |
| `APOGEE` | 4 | Triple-redundant trigger | `servo.write(90)` |
| `DESCENT` | 5 | Post-apogee | Monitor alt, log |
| `LANDING` | 6 | alt < 0.75 m | Close log · disable WiFi |

> **Recovery, as-built:** the FSM executes `servo.write(90)` at apogee, but the **flight-1 build has no servo installed** — recovery is passive gravity. Servo-actuated deployment is the original design and the planned path for Atl-2.

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

- **Board:** ESP32 Dev Module  *(classic ESP32 DevKit — confirm against your Arduino IDE selection)*
- **Partition Scheme:** `Default 4MB with spiffs (1.2MB App/OTA, 1.5MB SPIFFS)` ← required for LittleFS
- **Baud:** 115200

### Pre-Launch Checklist

- [ ] Power on **outdoors at launch site**, stationary and uncapped
- [ ] Wait for `=== READY FOR FLIGHT ===` (~2.6s calibration)
- [ ] Ground offset printed — confirm within ±1.0 m
- [ ] `LIST` via Serial → confirm new `/flight_N.csv` created
- [ ] Ground station connected — altitude reads near 0 m
- [ ] Only **one browser tab** open (multiple clients = duplicate CSV rows)
- [ ] Parachute packed for passive gravity deployment *(no servo on flight-1 build)*
- [ ] LiPo at 4.2V (HW-373 blue LED solid)
- [ ] Cap and pressurize **after** READY confirmation

---

## Repository Structure

```
Atl-I_Flight-Computer/
├── firmware/
│   └── HydroRocket.ino
├── flights/
│   ├── atl1_flight_20260601.csv
│   └── graphics.py
├── hardware/
│   ├── Schematic_HidroRocket_v0_2026-04-20.pdf
│   ├── Schematic_HidroRocket_v0_2026-04-26.pdf
│   └── Schematic_HidroRocket_v0_2026-04-26-(actual).pdf
├── mechanical/
│   ├── AvionicsBay_v4.1.0.1.stl
│   └── archive/
├── docs/
│   └── docs_avionics_bay_evolution.md
├── media/Prototype Gallery/
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
| Proper pressurization with gauge (Atl-1 under-pressurized → 1.6G, threshold 2.5G) | ✅ Gauge acquired |
| Verify CG/CP with electronics in OpenRocket (tumble at apogee) | 🔴 High |
| Replace BMP280 → MS5611 (±0.1m vs ±8m) | 🟡 Medium |
| EMI characterization — factorial 2×2 bench experiment (4 cells × 10 reps, A/A validation) | 📄 Paper |

---

## Author

**Jesús Alberto Perea García**  
Mechatronics Engineering — IEST Anáhuac, Tamaulipas  
Founder — Vértice Labs Research Program  
[github.com/DonJechu](https://github.com/DonJechu)

*Atl-1 is the flight validation platform for experimental research on EMI characterization methodology in compact low-cost avionics. Paper in preparation for IEEE regional conference.*

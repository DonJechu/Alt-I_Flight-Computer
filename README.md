# 🚀 Atl-1 Flight Computer — v2.1

> **Avionics system for experimental water-powered rockets.**
> Real-time telemetry · Triple-redundant apogee detection · Wi-Fi ground station · 7-state flight FSM · Onboard LittleFS black-box logging

[![Status](https://img.shields.io/badge/status-First%20Flight%20Complete-brightgreen)](https://github.com/DonJechu/HydroRocket-Telemetry-System)
[![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Research](https://img.shields.io/badge/research-EMI%20Mitigation%20%2F%20Avionics-purple)](docs/research/)

---

## 📋 Table of Contents

1. [Project Overview](#project-overview)
2. [Flight Results — Atl-1](#flight-results--atl-1)
3. [Research Context](#research-context)
4. [Hardware Architecture](#hardware-architecture)
5. [Avionics Bay — Design Evolution](#avionics-bay--design-evolution)
6. [Firmware — Flight State Machine](#firmware--flight-state-machine)
7. [Sensor Processing & Filters](#sensor-processing--filters)
8. [Onboard Data Logging — LittleFS](#onboard-data-logging--littlefs)
9. [Telemetry System](#telemetry-system)
10. [Repository Structure](#repository-structure)
11. [Build & Flash](#build--flash)
12. [Known Issues & Roadmap](#known-issues--roadmap)

---

## Project Overview

**Atl-1** is a flight computer designed for water-powered PET bottle rockets. It is the hardware validation platform for ongoing research into **EMI mitigation in compact low-cost avionics**, comparing shielded vs. unshielded wiring configurations under real flight conditions.

The system provides:
- **Real-time telemetry** over Wi-Fi WebSocket at 10 Hz
- **Triple-redundant apogee detection** (velocity zero-crossing + altitude drop + safety timeout)
- **Hardware IIR pressure filtering** on BMP280 to suppress aerodynamic transients
- **Parachute deployment** via servo at confirmed apogee
- **Onboard black-box logging** to flash memory via LittleFS — survives WiFi loss

> ✅ **Status:** First instrumented flight completed — **June 1, 2026, Veracruz, Mexico.**
> Apogee: ~14m. Onboard CSV recovered via Serial after landing. See [Flight Results](#flight-results--atl-1).

---

## Flight Results — Atl-1

**Date:** June 1, 2026 — Veracruz, México
**Launch site:** Open field — no pressure gauge available, pressure estimated

| Metric | Value |
|---|---|
| **Apogee (barometric)** | ~14 m |
| **Peak velocity** | ~4.5 m/s |
| **Peak G-Force (liftoff)** | ~1.6 G |
| **Flight duration** | ~30 s |
| **Parachute deployment** | ❌ Not triggered — see note |
| **LittleFS data recovery** | ✅ CSV recovered via Serial DUMP post-landing |

> **Note on parachute:** The FSM `LIFTOFF_G` threshold was set to 2.5G. The rocket only produced 1.6G at liftoff due to low pressurization (no gauge). The FSM remained in STANDBY throughout the flight. **Fix for Atl-2:** lower threshold to 1.2G.

**Raw flight data:** [`data/flights/atl1_flight_20260601.csv`](data/flights/atl1_flight_20260601.csv)

![Flight Data](media/atl1_flight_20260601_graphics.png)

---

## Research Context

This project is the experimental platform for a paper on **EMI mitigation strategies in compact low-cost avionics**, to be submitted to IEEE Latin America Transactions.

### Hypothesis

> *"EMI mitigation techniques (twisted pairs, star ground, LC filtering, spatial separation) significantly reduce I²C sensor degradation in compact ESP32-based avionics."*

The experiment compares two wiring configurations on a static bench:

| Config A — Unmitigated | Config B — Mitigated |
|---|---|
| Parallel untwisted cables | Twisted pairs on all signal lines |
| Daisy-chain ground | Star ground |
| No LC filter on MT3608 | LC filter (47µH + 100µF) post-boost converter |
| No decoupling | 100nF + 10µF per sensor node |

Metrics measured: IMU RMS noise, I²C packet loss rate, barometric altitude drift, servo EMI events.

> **Important framing:** This paper documents an *experimental implementation* of field-theoretic EMI mitigation principles in a real avionics platform. Claims are limited to measured deltas between configurations — not theoretical validation of electromagnetic models.

### Why a water rocket?

The Atl-1 wiring harness operates under real constraints: motor switching transients, servo PWM, active WiFi RF at 2.4 GHz, and mechanical vibration. This makes it a representative — and reproducible — low-cost testbed for avionics signal integrity experiments.

---

## Hardware Architecture

### Component Stack

| Component | Part | Role |
|---|---|---|
| **Microcontroller** | ESP32-C3 Super Mini (USB-C) | Main processor, Wi-Fi AP, WebSocket server |
| **IMU** | GY-521 module — physical chip: **MPU-6500** (WHO_AM_I = 0x70) | 6-axis IMU — configured ±16G / ±2000°/s |
| **Barometer** | BMP280 (0x76) | Altitude via pressure — hardware IIR FILTER_X16 enabled |
| **Power Boost** | MT3608 DC-DC step-up | LiPo 3.7V → 5V regulated |
| **Battery Charger** | HW-373 (TP4056 + DW01 protection) | USB-C LiPo charging with simultaneous load support |
| **Battery** | LiPo 1S — 3.7V / 600–700 mAh | Flight power supply |
| **Actuator** | Servo SG90 (GPIO 2) | Parachute deployment |
| **Switch** | SPDT (GPIO 10) | Master power ON/OFF |
| **Buzzer** | Piezoelectric active (GPIO 3) | State audio feedback |

> **IMU library note:** The GY-521 module on this build contains an **MPU-6500**, not an MPU-6050. The Adafruit MPU6050 library requires a one-line patch in `Adafruit_MPU6050.cpp` to accept WHO_AM_I = 0x70. See [Build & Flash](#build--flash).

### I²C Bus

| Signal | ESP32-C3 Pin | Device |
|---|---|---|
| SDA | GPIO 21 | MPU-6500, BMP280 |
| SCL | GPIO 22 | MPU-6500, BMP280 |
| — | 0x68 | MPU-6500 I²C address |
| — | 0x76 | BMP280 I²C address |

Pull-ups: 5.1 kΩ from SDA → 3V3 and SCL → 3V3.

### Power Path

```
USB-C ──► HW-373 IN+ ──► [charges] ──► LiPo 3.7V ──► MT3608 ──► 5V ──► ESP32-C3
          HW-373 OUT+ ─────────────────────────────────────┘
                                               └──► 3.3V LDO ──► Sensors
```

> On landing detection, WiFi is disabled in firmware to reduce idle current from ~180mA to ~20mA.

---

## Avionics Bay — Design Evolution

The avionics bay has gone through four major design iterations. Each failure was documented and fed directly into the next version. This is the engineering record.

### Version Comparison

| Parameter | v1 | v2 | v3 | v4 (current) |
|---|---|---|---|---|
| **Height** | 118.5 mm | 118 mm | 102 mm | TBD |
| **Base OD** | 52 mm | 81 mm | 80.9 mm | TBD |
| **Printed Mass** | 33.01 g | 66.62 g | 46.22 g | 25.63 g |
| **Pieces** | 1 | 2 | 2 | 1 |
| **PCB mount** | Screwed direct | Slide rail | Slide rail (revised) | TBD |
| **Battery space** | None | Insufficient | In revision | TBD |
| **Fits 1.35L bottle** | ❌ | ❌ | ❌ | ✅ |
| **Architecture** | 1 piece | 2 pieces | 2 pieces | 1 piece |
| **Structure** | Solid shell | Rail + shell | Honeycomb panel | Truss / celosía |

> *Masses are slicer estimates (PETG, no supports). Final printed mass will differ.*

---

### v1 — Structural Baseline

**Goal:** Minimum viable bay. One-piece design, direct PCB mount.

**Result — Failed integration:**
- PCB mounting nuts and standoff contractions caused mechanical interference; the board could not be seated flush
- No dedicated battery compartment — LiPo placement undefined
- Ring dimensions incorrect: OD too small to fit inside a 1.35L Coca-Cola PET bottle (~85mm ID required)
- Single-piece design meant any revision required reprinting the entire part

**Mass:** 33.01 g | **Height:** 118.5 mm | **OD:** 52 mm

![AvionicsBay v1 Render](media/Prototype%20Gallery/AvionicsBay_v1_render.png)

---

### v2 — Rail System

**Goal:** Solve PCB access problem. Introduce modularity.

**Key change:** Split into two parts — a structural sled that mounts to the bottle, and a PCB carrier plate that slides into a rail. This means PCB changes only require reprinting the carrier plate, not the full bay.

**Result — Too heavy:**
- Rail + structural shell geometry increased mass to 66.62 g (+100% vs v1)
- Battery space still insufficient for LiPo cell
- OD corrected but not validated against physical bottle measurement

**Mass:** 66.62 g | **Height:** 118 mm | **OD:** 81 mm

![AvionicsBay v2 Render](media/Prototype%20Gallery/AvionicsBay_v2_render.png)

---

### v3 — Mass-Optimized Honeycomb

**Goal:** Reduce mass below 40g while keeping v2's rail modularity. Introduce honeycomb infill geometry on the main panel.

**Key changes:**
- Reduced height by 16 mm (102 mm total)
- Honeycomb cutout pattern on main structural panel (mass reduction + ventilation)
- Maintained two-piece rail system from v2

**Failures documented (v3.0 Alpha):**
1. **Mechanical:** Nut/standoff interference on PCB mount — same root cause as v1, not fully resolved
2. **Volumetric:** Battery compartment still does not accommodate LiPo + cable routing
3. **Geometric:** OD not validated against physical bottle. Nominal 80.9 mm but actual 1.35L bottle ID varies 84–87 mm by batch
4. **Structural:** Print fracture during support removal on thin-wall sections (<2 mm)

**Mass:** 46.22 g | **Height:** 102 mm | **OD:** 80.9 mm

![AvionicsBay v3 Render](media/Prototype%20Gallery/AvionicsBay_v3_render.png)

---

### v4 — Single-Piece Truss Architecture (Current)

**Goal:** Eliminate over-engineering. Return to single-piece design with truss/celosía geometry for maximum mass reduction while maintaining structural integrity.

**Key changes:**
- Single-piece design — removes rail interface, assembly complexity, and inter-part tolerance issues
- Truss skeleton replaces solid honeycomb panel — material only where structurally necessary
- Estimated model mass: 25.63 g (slicer, no supports) — best result across all versions

**Print history:**
- `ABv4.0` — Failed. Top Z distance 0mm caused supports to fuse to structure. Removed with damage.
- `ABv4.1` — Successful. Top Z distance corrected to 0.25mm, Normal supports, threshold 30°. Structure intact post-support removal.

**Design rationale:** Previous versions (v2, v3) introduced modularity to solve PCB access — but added mass and complexity. v4 returns to v1's simplicity with v3's mass consciousness. Less is more.

**Integration status:** PCB assembly fits inside 1.35L bottle. ✅

![AvionicsBay v4 Render](media/Prototype%20Gallery/AvionicsBay_v4_render.png)

### PCB — Physical Assembly

| Bare board (v1 iteration) | Full assembly with ESP32 + sensors |
|---|---|
| ![PCB bare](media/Prototype%20Gallery/PCB_v1_bare_assembly.jpg) | ![PCB full](media/Prototype%20Gallery/PCB_v2_full_assembly.jpg) |

---

## Firmware — Flight State Machine

The flight computer implements a 7-state FSM. States `ARMED` and `IGNITION` are defined for future remote-arming via the ground station.

```
                    gForce ≥ 1.2G *
                   (4 consecutive)
  ┌──────────┐ ─────────────────────► ┌──────────┐
  │ STANDBY  │                        │  ASCENT  │
  └──────────┘                        └──────────┘
                                           │
                          ┌────────────────┼────────────────┐
                          │                │                │
                    vel ≤ 0           alt drops        timeout
                   (5 samples)         1.0m (4s)       12,000 ms
                          │                │                │
                          └────────────────┼────────────────┘
                                           ▼
                                      ┌──────────┐
                                      │  APOGEE  │  servo → 90°
                                      └──────────┘
                                           │
                                           ▼
                                      ┌──────────┐
                                      │ DESCENT  │
                                      └──────────┘
                                           │
                                    alt < 0.75 m
                                           │
                                           ▼
                                      ┌──────────┐
                                      │ LANDING  │  WiFi OFF · log closed
                                      └──────────┘
```

> \* **Atl-1 flight data showed peak liftoff G = 1.6G at low pressure.** `LIFTOFF_G` was corrected from 2.5G to 1.2G for Atl-2.

### State Descriptions

| State | ID | Entry Condition | Action |
|---|---|---|---|
| `STANDBY` | 0 | Boot | Transmit telemetry, log to flash, wait for liftoff |
| `ARMED` | 1 | *(future)* | — |
| `IGNITION` | 2 | *(future)* | — |
| `ASCENT` | 3 | gForce ≥ 1.2G × 4 samples | Track maxAltitude, evaluate apogee |
| `APOGEE` | 4 | Triple-redundant | `servo.write(90)` — deploy parachute |
| `DESCENT` | 5 | Post-apogee | Monitor altitude, transmit, log |
| `LANDING` | 6 | altitude < 0.75 m | Close LittleFS log · disable WiFi |

---

## Sensor Processing & Filters

### BMP280 — Dual IIR Filtering + Ground Calibration

```cpp
bmp.setSampling(
  MODE_NORMAL,
  SAMPLING_X2,   // Temperature
  SAMPLING_X16,  // Pressure — maximum resolution
  FILTER_X16,    // Hardware IIR — maximum smoothing
  STANDBY_MS_1   // ~28 Hz update rate
);
```

Software IIR (α = 0.8 for flight responsiveness):

$$h_{filtered}[n] = 0.8 \cdot h_{filtered}[n-1] + 0.2 \cdot h_{raw}[n]$$

**Calibration sequence at boot (2.6 seconds total):**

1. **Warm-up — 100 reads discarded:** IIR FILTER_X16 requires ~70 samples to converge from an unknown initial state. Early readings are biased and must be discarded.
2. **Base pressure — average of reads 101–200:** With IIR settled, 100 samples are averaged for `basePressure`.
3. **Ground offset — 50 samples:** `bmp.readAltitude(basePressure)` is averaged to establish the absolute zero reference. Compensates for any residual IIR offset.

```
rawAlt = bmp.readAltitude(basePressure) - groundAltitude
```

> **Critical:** Calibration must be performed outdoors at the launch site with the rocket stationary and uncapped. Moving to a different pressure environment (indoors, different altitude) after calibration invalidates the zero reference.

### MPU-6500 — G-Force & Orientation

Configured via Adafruit MPU6050 library (compatible after WHO_AM_I patch):

```cpp
mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
```

Raw accelerometer values converted to G:

```cpp
float ax = a.acceleration.x / 9.81f;
float ay = a.acceleration.y / 9.81f;
float az = a.acceleration.z / 9.81f;
gForce = sqrt(ax*ax + ay*ay + az*az);
```

---

## Onboard Data Logging — LittleFS

v2.1 adds persistent flash logging independent of WiFi connectivity. Flight data is preserved even if the ground station loses connection mid-flight.

**Each boot creates a new file:** `/flight_1.csv`, `/flight_2.csv`, etc. (auto-increment, never overwrites).

**Format:** same columns as WebSocket telemetry — directly compatible with ground station CSV export.

**Capacity:** ~60 bytes/row × 10 Hz × 60s flight ≈ 36 KB per flight. ~1.5 MB available → ~40 flights before needing FORMAT.

### Serial Recovery Commands

Connect via USB at 115200 baud after landing:

| Command | Action |
|---|---|
| `LIST` | List all files and sizes |
| `DUMP` | Dump all CSV files to Serial |
| `DUMP:flight_1.csv` | Dump specific file |
| `FORMAT` | Erase all log files |

---

## Telemetry System

The ESP32-C3 creates a Wi-Fi Access Point and serves a WebSocket server on port 81 at 10 Hz:

```json
{
  "t": 72421,
  "alt": 14.1,
  "vel": 5.4,
  "accel": 1.15,
  "pitch": 68.0,
  "roll": 147.5,
  "yaw": 0.0,
  "temp": 35.1,
  "pressure": 997.4,
  "phase": 0
}
```

| Field | Unit | Description |
|---|---|---|
| `t` | ms | Timestamp since boot |
| `alt` | m | Filtered altitude above calibration point |
| `vel` | m/s | Vertical velocity (positive = ascending) |
| `accel` | G | Total acceleration magnitude |
| `pitch` | ° | Nose-up/down angle |
| `roll` | ° | Rotation around longitudinal axis |
| `temp` | °C | Ambient temperature (BMP280) |
| `pressure` | hPa | Absolute pressure |
| `phase` | 0–6 | Current FSM state |

> **Ground Station Repository:** [Atl_1-Ground-Station](https://github.com/DonJechu/HYDRO-1-Ground-Station)

---

## Ground Station — HYDRO-1 GS

The ground station is a separate React + Vite web application that connects to the flight computer over WebSocket and visualizes all telemetry in real time.

![Ground Station Dashboard](media/Prototype%20Gallery/ground_station_v2.png)

### Features

| Feature | Description |
|---|---|
| **3D Attitude View** | Real-time 3D rocket model that rotates with live pitch, roll, and yaw data |
| **Flight Phase Tracker** | Left panel shows all 7 states (STANDBY → ARMED → IGNITION → ASCENT → APOGEE → DESCENT → RECOVERY), highlighting the active state |
| **Live Telemetry Panel** | Right panel — altitude (m), velocity (m/s), G-force, pitch, roll, yaw, temperature (°C), pressure (hPa) |
| **Session Records** | Tracks max altitude, max velocity, max G, and mission elapsed time (T+) |
| **Altitude & Velocity Charts** | Real-time graphs updated at 10 Hz |
| **WiFi Connect / Demo Mode** | Connect to the ESP32-C3 AP or run a simulated flight for testing without hardware |
| **CSV Export** | Download full telemetry session as `.csv` — same format as LittleFS onboard log |

### Tech Stack

```
React + Vite        — UI framework (localhost:5173 in dev)
WebSocket (port 81) — Real-time data stream from ESP32-C3
Three.js            — 3D rocket attitude visualization
```

### Connection Flow

```
ESP32-C3 (AP: ROCKET_AP) ──► WebSocket ws://192.168.4.1:81 ──► Ground Station
                                    10 Hz JSON packets
```

![Ground Station Demo](media/Prototype%20Gallery/ground_station_demo.gif)

> **Note:** Open only **one browser tab** at a time. Multiple connected WebSocket clients cause duplicate rows in the exported CSV.

---

## Repository Structure

```
Atl-Flight_Computer/
├── firmware/
│   └── hidrorocket_v3.ino       # Flight computer firmware v2.1
├── flights/
│   └── atl1_flight_20260601.csv   # Atl-1 first flight — June 1, 2026
│   └── graphics.py   # Atl Graphics generator
├── analysis/
│   └── graphics.py              # Flight data visualization
├── hardware/
│   └── Schematic_HidroRocket_v0_2026-04-26.pdf
├── mechanical/
│   └── AvionicsBay_v4.stl
├── media/
│   └── atl1_flight_analysis.png   # Atl-1 graphics of first flight — June 1, 2026
└── README.md
```

---

## Build & Flash

### Dependencies (Arduino IDE)

```
Adafruit MPU6050     by Adafruit        ← requires WHO_AM_I patch for MPU-6500
Adafruit BMP280      by Adafruit
Adafruit Sensor      by Adafruit        (dependency)
ESP32Servo           by Kevin Harrington
WebSockets           by Markus Sattler
LittleFS             (ESP32 built-in)
WiFi                 (ESP32 built-in)
```

### MPU-6500 Library Patch (required)

The GY-521 module on this build contains an MPU-6500 (WHO_AM_I = 0x70). The Adafruit library rejects it by default. Apply this one-line fix:

**File:** `Documents/Arduino/libraries/Adafruit_MPU6050/Adafruit_MPU6050.cpp` — line ~93

```cpp
// BEFORE:
if (chip_id.read() != MPU6050_DEVICE_ID) {

// AFTER:
if (chip_id.read() != MPU6050_DEVICE_ID && chip_id.read() != 0x70) {
```

### Flash Steps

1. Open `firmware/atl_flgiht-computer.ino` in Arduino IDE
2. **Board:** `ESP32C3 Dev Module`
3. **Partition Scheme:** `Default 4MB with spiffs (1.2MB App/OTA, 1.5MB SPIFFS)` ← required for LittleFS
4. Select correct COM port
5. Upload
6. Open Serial Monitor at **115200 baud**
7. Wait for calibration sequence (~2.6s) and `=== READY FOR FLIGHT ===`
8. Connect to Wi-Fi: `SSID: Atl_FC` / `Password: 12345678`

### Pre-Launch Checklist

- [ ] Power on **outdoors at launch site**, rocket stationary and uncapped
- [ ] Wait for `=== READY FOR FLIGHT ===` (2.6s calibration)
- [ ] Ground offset printed: confirm within ±1.0 m
- [ ] `LIST` via Serial → confirm new `/flight_N.csv` created
- [ ] Ground station connected — altitude reads ±1 m
- [ ] Only **one browser tab** open (duplicates cause double CSV rows)
- [ ] Servo confirmed at 0° (parachute closed)
- [ ] LiPo charged to 4.2V (HW-373 blue LED)
- [ ] Cap and pressurize **after** READY confirmation

---

## Known Issues & Roadmap

### Resolved in v2.1

| Issue | Status |
|---|---|
| LittleFS black-box logging | ✅ Implemented |
| BMP280 IIR warm-up before calibration | ✅ Fixed |
| Ground altitude zero offset | ✅ Fixed |
| MPU-6500 WHO_AM_I library rejection | ✅ Patched |
| WiFi power drain post-landing | ✅ WiFi disabled on LANDING |
| Double CSV rows (multiple WS clients) | ✅ Documented — use one tab |

### Pending — Atl-2

| Item | Priority |
|---|---|
| Lower `LIFTOFF_G` to 1.2f (flight data shows 1.6G peak at low pressure) | 🔴 High |
| Verify CG/CP with electronics integrated in OpenRocket | 🔴 High |
| Pressure gauge for launch (no more estimating) | 🔴 High |
| Replace BMP280 → MS5611 (±0.1m vs ±8m resolution) | 🟡 Medium |
| Kalman filter for pitch/roll (replace raw atan2) | 🟡 Medium |
| `ARMED` / `IGNITION` state integration with ground station | 🟢 Low |
| A/B EMI experiment on static bench (10 reps × 2 configs) | 📄 Paper |

---

## Author

**Jesús Alberto Perea García**
Mechatronics Engineering Student — IEST Anáhuac, Tamaulipas
Founder — Vértice Labs Research Program
[github.com/DonJechu](https://github.com/DonJechu)

*Atl-1 is the validation platform for experimental research on EMI mitigation in compact low-cost avionics. Paper in preparation for submission to IEEE Latin America Transactions.*

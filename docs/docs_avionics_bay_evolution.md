# Avionics Bay — Design Evolution (v1 → v4)

Engineering record of the avionics bay mechanical design for the Atl-1 flight computer. Each version documents specific failures and the reasoning behind each change.

---

## Version Comparison

| Parameter | v1 | v2 | v3 | v4 (current) |
|---|---|---|---|---|
| Height | 118.5 mm | 118 mm | 102 mm | TBD |
| Base OD | 52 mm | 81 mm | 80.9 mm | TBD |
| Printed mass (slicer est.) | 33.01 g | 66.62 g | 46.22 g | 25.63 g |
| Pieces | 1 | 2 | 2 | 1 |
| PCB mount | Direct screwed | Slide rail | Slide rail (revised) | TBD |
| Battery space | None | Insufficient | In revision | Integrated |
| Fits 1.35L bottle | ❌ | ❌ | ❌ | ✅ |
| Structure | Solid shell | Rail + shell | Honeycomb panel | Truss / celosía |

> Masses are slicer estimates (PETG, no supports). Final printed mass will differ.

---

## v1 — Structural Baseline

**Goal:** Minimum viable bay. One-piece, direct PCB mount.

**Failures:**
- PCB mounting nuts and standoff contractions caused mechanical interference — board could not seat flush
- No dedicated battery compartment — LiPo placement undefined
- OD (52 mm) too small for 1.35L Coca-Cola bottle interior (~84–87 mm required)
- Single-piece design meant any revision required reprinting the entire part

**Mass:** 33.01 g | **Height:** 118.5 mm | **OD:** 52 mm

![v1 Render](../media/Prototype%20Gallery/AvionicsBay_v1_render.png)

---

## v2 — Rail System

**Goal:** Solve PCB access. Introduce modularity.

**Key change:** Split into two parts — structural sled (mounts to bottle) + PCB carrier plate (slides into rail). PCB changes only require reprinting the carrier.

**Failures:**
- Rail + shell geometry doubled mass to 66.62 g (+100% vs v1)
- Battery space still insufficient
- OD corrected but not validated against a physical bottle

**Mass:** 66.62 g | **Height:** 118 mm | **OD:** 81 mm

![v2 Render](../media/Prototype%20Gallery/AvionicsBay_v2_render.png)

---

## v3 — Mass-Optimized Honeycomb

**Goal:** Reduce mass below 40g while keeping v2's rail modularity.

**Key changes:**
- Height reduced 16 mm (102 mm total)
- Honeycomb cutout pattern on main panel (mass reduction + ventilation)
- Maintained two-piece rail system from v2

**Failures (v3.0 Alpha):**
1. **Mechanical:** Nut/standoff interference on PCB mount — same root cause as v1, not resolved
2. **Volumetric:** Battery compartment does not fit LiPo + cable routing
3. **Geometric:** OD not validated against physical bottle. 1.35L bottle ID varies 84–87 mm by batch
4. **Structural:** Print fracture during support removal on thin-wall sections (<2 mm)

**Mass:** 46.22 g | **Height:** 102 mm | **OD:** 80.9 mm

![v3 Render](../media/Prototype%20Gallery/AvionicsBay_v3_render.png)

---

## v4 — Single-Piece Truss Architecture (Current)

**Goal:** Eliminate over-engineering. Return to single-piece with truss/celosía geometry.

**Key changes:**
- Single piece — removes rail interface, assembly complexity, and inter-part tolerance issues
- Truss skeleton replaces solid honeycomb — material only where structurally necessary
- Estimated 25.63 g — lightest across all versions

**Print history:**
- `ABv4.0` — Failed. Top Z distance 0 mm caused supports to fuse to structure. Damage on removal.
- `ABv4.1` — Successful. Top Z corrected to 0.25 mm, Normal supports, threshold 30°. Structure intact.

**Design rationale:** v2 and v3 added modularity to solve PCB access — but gained mass and complexity. v4 returns to v1's simplicity with v3's mass consciousness. Less is more.

**Integration status:** PCB assembly fits inside 1.35L bottle. ✅

![v4 Render](../media/Prototype%20Gallery/AvionicsBay_v4_render.png)

---

## PCB Physical Assembly

| Bare board (v1 iteration) | Full assembly with ESP32 + sensors |
|---|---|
| ![PCB bare](../media/Prototype%20Gallery/PCB_v1_bare_assembly.jpg) | ![PCB full](../media/Prototype%20Gallery/PCB_v2_full_assembly.jpg) |

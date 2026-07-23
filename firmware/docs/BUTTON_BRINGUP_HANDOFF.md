# Side-button bring-up handoff (DisabilityIn PCB)

## Issue summary

On the DisabilityIn demo lanyard (XIAO ESP32-S3):

- **Left** side button (GPIO8): momentary press works — firmware emits `LEFT_PRESS` and the web twin flashes the left button.
- **Right** side button (GPIO9): **momentary press / web UI cue unreliable or missing**.
- **Both-hold** personal alert (~2.5 s from NVS `pa_btn_dly`) generally **works**.
- Schematic for both sides is identical: **active-low** tact switch to GND, **1 kΩ pull-up to 3V3**, **1 µF** to GND.

Observed in serial logs (production `SideButtons` path):

- Shortly after init, right often fires `BUTTON_LONG_PRESS_START` / `RIGHT_HOLD` at exactly the hold threshold with **no intentional press** → GPIO9 often reads **stuck active** (logic “pressed”) from boot.
- Right then shows `LONG_PRESS_*` / participates in `BOTH_HOLD`, but rarely/never `SINGLE_CLICK` or a short `PRESS` UI cue.
- Left continues to get `BUTTON_SINGLE_CLICK` / momentary `LEFT_PRESS`.

Web twin expects `APP_CODE_BUTTON` (0x10) with side left=0 / right=1 on momentary press (`DigitalTwinApplication::onSidePress` → `BleTwin::emitButtonEvent`).

---

## Hardware / firmware map

| Item | Value |
|------|--------|
| Board define | `HARDWAREPROTOTYPE__USING_DISABILITYIN_PCB` |
| Left pin | `HARDWARE_BUTTON_LEFT_PIN` = GPIO8 |
| Right pin | `HARDWARE_BUTTON_RIGHT_PIN` = GPIO9 |
| Intended polarity | `HARDWARE_BUTTON_ACTIVE_HIGH 0` (active low) |
| Pull define | `HARDWARE_BUTTON_PULLUP` 0 or 1 (see matrix) |
| Button stack | `espressif/button` (`iot_button`) in `src/features/side-buttons/SideButtons.*` |
| App consumer | `DigitalTwinApplication` callback: `BOTH_HOLD`, `LEFT_PRESS`, `RIGHT_PRESS` only |
| Hold threshold | From State / NVS, often **2500 ms** (not 5 s) |

Schematic nets: `PERSONAL_ALERT_BUTTON_LEFT` / `_RIGHT` — pull-up + cap + switch to GND.

---

## Define-driven button test (use this first)

In `src/application/Hardware.h`:

```c
#define ILSS_TEMP_BUTTON_BRINGUP_TEST 1
```

Knobs (defaults in `src/application/ButtonBringupTest/ButtonBringupTest.h`, overridable in `Hardware.h`):

| Define | Meaning |
|--------|---------|
| `ILSS_BTN_TEST_ACTIVE_HIGH` | 0 = active low, 1 = active high |
| `ILSS_BTN_TEST_PULL` | 0 = none, 1 = pull-up, 2 = pull-down |
| `ILSS_BTN_TEST_SWAP_PINS` | 1 = swap GPIO8/9 roles |
| `ILSS_BTN_TEST_USE_IOT_BUTTON` | 1 = also log `iot_button` events |
| `ILSS_BTN_TEST_LONG_MS` / `SHORT_MS` | hold / short times |

Build, flash, monitor @ 115200. App does **not** start; logs `BtnBringup` EDGE/POLL + `iot_button` events.

**Suggested matrix**

1. `ACTIVE_HIGH=0`, `PULL=1`, `SWAP=0`, `IOT=1` — schematic match  
2. `ACTIVE_HIGH=0`, `PULL=0`, `SWAP=0`, `IOT=1` — external pull only (old prod)  
3. `ACTIVE_HIGH=0`, `PULL=2`, `SWAP=0`, `IOT=1` — wrong pull (control)  
4. `ACTIVE_HIGH=1`, `PULL=2`, `SWAP=0`, `IOT=1` — active-high + pulldown  
5. Best of above with `SWAP=1` — rule out left/right wiring swap  
6. Best of above with `IOT=0` — raw GPIO only  

Record for each: idle `raw L/R`, edge behaviour on tap, whether `SINGLE_CLICK` appears on right.

Disable test (`#undef` / comment `ILSS_TEMP_BUTTON_BRINGUP_TEST`) before shipping twin app again.

---

## Things already tried (production path)

1. **Quiet logging** — demote `PRESS_DOWN`/`PRESS_UP` to DEBUG; drop `ESP_LOGI` spam in callbacks (BLE/CPU load during alert EMI).
2. **EMI “edge flood” lockout on right only** — rate-limit/ignore bursts. Caused WARN spam; later suppressed real clicks. **Removed.**
3. **`HARDWARE_BUTTON_PULLUP 1`** — internal pull-up with active-low. Reduced EMI float historically; user reported it broke right momentary / both-hold when bundled with other changes. Re-tried; still no reliable right momentary.
4. **`HARDWARE_BUTTON_PULLUP 0`** — disable internal pull (`SideButtons` “mismatch” → `disable_pull=true`). Restored both-hold; right still flaky for short press.
5. **Restore original `SideButtons` from git `5a53545`** — known baseline; both-hold OK; right short click still inconsistent vs left.
6. **Momentary on `PRESS_UP` by duration** (not `SINGLE_CLICK`) — worked briefly in one session (`Right button PRESS (momentary 315 ms)` + web cue); later regressing / unreliable as right stayed in `LONG_PRESS` path.
7. **`both_hold_fired_` latch** — stop `LONG_PRESS_HOLD` re-arming both-hold timer (was re-firing `BOTH_HOLD` ~every hold period → app toggled PA off / indicators restarted). Latch **also blocked** later right momentaries when one side looked stuck. **Removed from SideButtons.**
8. **App-level BOTH_HOLD debounce (3 s)** in `DigitalTwinApplication::onBothHold` — keep PA from double-firing without SideButtons latch.
9. **`LONG_PRESS_HOLD` no longer re-arms** both-hold timer (minimal PA fix).
10. **Per-press `press_with_other_` suppress** for momentary — did not restore right.
11. **Solo UI cue on `PRESS_DOWN`** + 250 ms cooldown — still reported not working by user (may need bring-up matrix / may not have been flashed, or GPIO never edges).
12. **Idle GPIO log at init** — expect `1,1` for active-low+pull-up; right `0` implies stuck-active.
13. **BS fast sweep audio** (unrelated) — firmware `playMediumSweep` ignored speed; fixed cycle_ms 150; web buzz rings decoupled from `BUZZER_DUR`.

### Related BLE / alert notes (context)

- During buzzer/haptic alerts, right GPIO was historically very noisy (`PRESS_DOWN`/`UP` storms) with `PULLUP=0`, contributing to BLE supervision timeouts (`reason=520`).
- Browser often rejects conn-param update; supervision stays ~720 ms.
- Twin apply / haptics deferred off NimBLE host to reduce disconnects.

---

## Likely root causes to investigate next

1. **GPIO9 electrically stuck low or weakly pulled** (missing/wrong R2, short, XIAO pin mux) — explains boot-time `RIGHT_HOLD` at hold threshold.
2. **`iot_button` + large 1 µF** — slow edges; right more marginal than left (layout).
3. **Pin swap / wrong net to MCU** — use `ILSS_BTN_TEST_SWAP_PINS`.
4. **Software polarity/pull mismatch** — use bring-up matrix; align `Hardware.h` with measured idle levels.
5. Production should emit web cue on a path that matches **measured** edges (DOWN vs UP vs `SINGLE_CLICK`), only after bring-up proves GPIO toggles.

---

## Key files

- `src/application/Hardware.h` — pins, pull, `ILSS_TEMP_BUTTON_BRINGUP_TEST`
- `src/application/ButtonBringupTest/*` — this test harness
- `src/features/side-buttons/SideButtons.cpp` — production handler
- `src/application/DigitalTwinApplication/DigitalTwinApplication.cpp` — `onSidePress` / `onBothHold`
- `src/protocol/TwinState.h` — `APP_CODE_BUTTON`
- Frontend: button flash on BLE button event (simulator page)

---

## Success criteria

- Bring-up: idle raw levels stable; each physical tap → clear EDGE + (if IOT) `SINGLE_CLICK` or clean DOWN/UP on **both** sides.
- Production: right tap → log `Right button PRESS` + `Side button press → UI cue (right)` + web flash; both-hold still raises personal alert once per gesture.

---

## Bring-up result (2026-07-20) — ACTIVE_HIGH=0 PULLUP=1 SWAP=0

Serial evidence:

- Idle stable: `raw L=1 R=1 pressed L=0 R=0` (polarity/pull OK for both nets at MCU).
- **Left GPIO8**: EDGE + `PRESS_DOWN` / `PRESS_UP` / `SINGLE_CLICK` on tap — works.
- **Right GPIO9**: **no EDGE for ~40s of continuous POLL while user pressed physical right** — `R` stayed `1`.

Conclusion: this is **not** an `iot_button` / momentary-software bug for short clicks on GPIO9. Either:

1. Physical right switch is **not connected to GPIO9** (wrong pad / open / different XIAO pin), or  
2. Switch/mechanics dead on that side, or  
3. User was not actuating the net that reaches GPIO9.

### PIN_HUNT result (resolved)

```
HUNT gpio=7 raw=1 -> 0  active=1   ← physical RIGHT
HUNT gpio=8 ... (mapped LEFT)      ← physical LEFT
```

**Right button is on GPIO7 (XIAO D8), not GPIO9 (D10).**  
Production fix: `HARDWARE_BUTTON_RIGHT_PIN GPIO_NUM_7` (bring-up test disabled again).

Earlier “both-hold works / right LONG_PRESS storms” with GPIO9 + `PULLUP=0` was largely **wrong pin + float/EMI on unused GPIO9**, not a broken momentary state machine on the real switch.

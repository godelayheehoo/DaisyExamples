# Ramping (LFO Auto-Mapping) — Implementation Plan

> **See [Addendum: Clear Gesture Redesign](#addendum-clear-gesture-redesign) at the bottom of this document.**
> Section 3 below describes the original clear gesture; it has been superseded by the addendum.

## Summary

Repurpose the existing unused button, unused button LED, and unused pot as a
"ramp" control: a learn-gesture that maps any one of the 9 main pots to an
internal LFO, sweeping that parameter between two user-set values at a
user-set rate. Multiple controls can be ramped simultaneously, each with its
own independent LFO. State is RAM-only — power cycle clears all ramps by
design (this is intentional, not a gap to fix).

Existing hardware/pins being repurposed (no wiring changes needed):
- `PIN_UNUSED_BUTTON` (D10) → **Ramp button**
- `PIN_UNUSED_BUTTON_LED` (D9) → **Ramp LED**
- `PIN_UNUSED_POT` / `kUnusedPot` (A9/D24) → **Ramp rate pot**

The existing `unused_button`, `unused_button_led`, `unused_button_prev`,
`unused_led_on` variables and their init/toggle code in
`TestSidechainCompressor.cpp` should be **removed** and replaced by the state
machine below.

---

## 1. Data structures

Add to `constants.h`:

```cpp
// Ramp / LFO mapping
constexpr float RAMP_RATE_MIN_HZ   = 0.02f;  // ~50s period
constexpr float RAMP_RATE_MAX_HZ   = 8.0f;   // fast tremolo
constexpr float RAMP_MOVE_THRESHOLD = 0.03f; // 3% pot travel counts as "moved"
constexpr int   RAMP_FLASH_COUNT    = 3;     // triple-flash when touching a mapped pot
constexpr int   RAMP_FLASH_ON_MS    = 60;
constexpr int   RAMP_FLASH_OFF_MS   = 60;
```

New file `ramp_control.h` (or inline near the top of the .cpp, your call):

```cpp
enum class LfoShape
{
    kTriangle,
    // future: kSine kSquare, kSaw
};

// Returns a 0..1 unipolar value for the given 0..1 phase.
// phase = 0 -> 0 (i.e. "low"), phase = 0.5 -> 1 (i.e. "high"), phase = 1 -> 0.
inline float EvaluateLfo(float phase, LfoShape shape)
{
    switch(shape)
    {
        case LfoShape::kTriangle:
            return 1.0f - fabsf(2.0f * phase - 1.0f);
        default:
            return 0.5f * (1.0f - cosf(2.0f * (float)M_PI * phase));
    }
}

enum RampableControl
{
    kRampThreshold = 0,
    kRampRatio,
    kRampAttack,
    kRampRelease,
    kRampMix,
    kRampDrive,
    kRampWidth,
    kRampOutput,
    kRampCutoff,
    kNumRampableControls
};

struct RampState
{
    bool     active   = false;
    float    low      = 0.0f;   // raw pot-space [0,1], post-inversion
    float    high     = 0.0f;   // raw pot-space [0,1], post-inversion
    float    rate_hz  = 0.5f;
    float    phase    = 0.0f;   // 0..1, wraps
    LfoShape shape    = LfoShape::kTriangle;
};

RampState ramp_state[kNumRampableControls];
int       last_mapped_control = -1; // for the "clear most recent" gesture
```

Note: ramps are stored in **raw pot-space** (the same `1.0f - hw.adc.GetFloat(...)`
domain the existing code already computes), not in final engineering units
(dB, ms, etc.). This means the existing per-control mapping math
(`THRESHOLD_MIN_DB + val * THRESHOLD_RANGE_DB`, the exponential cutoff/ratio
maps, etc.) doesn't need to change at all — you just substitute where `val`
comes from.

---

## 2. Learn-gesture state machine

```cpp
enum class LearnPhase
{
    kIdle,
    kWaitingForTarget, // button held, watching for a pot to move
    kSettingHigh,      // target found, tracking its value + watching ramp pot for movement
    kSettingRate        // ramp pot moving, locking rate; releasing button commits
};

LearnPhase learn_phase = LearnPhase::kIdle;
int        learn_target = -1;
float      learn_snapshot[kNumRampableControls]; // pot values at button-press time
float      learn_ramp_pot_snapshot = 0.0f;
float      learn_committed_high = 0.0f;
```

### Per-loop-iteration logic (goes in the existing `while(true)` control loop,
after all 9 raw pot values + the ramp pot value have been read for this
iteration — see section 4 for how those reads change):

```cpp
bool ramp_btn_held = !dsy_gpio_read(&unused_button); // reuse existing pin/pull config, active-LOW

switch(learn_phase)
{
    case LearnPhase::kIdle:
        if(ramp_btn_held)
        {
            // Snapshot everything at the moment of press
            for(int i = 0; i < kNumRampableControls; i++)
                learn_snapshot[i] = GetRawPotValue(i); // see section 4
            learn_ramp_pot_snapshot = raw_ramp_pot_value;
            learn_target = -1;
            learn_phase  = LearnPhase::kWaitingForTarget;
        }
        break;

    case LearnPhase::kWaitingForTarget:
        if(!ramp_btn_held)
        {
            // Released before a target was found -> cancel, nothing mapped
            learn_phase = LearnPhase::kIdle;
            break;
        }
        for(int i = 0; i < kNumRampableControls; i++)
        {
            if(fabsf(GetRawPotValue(i) - learn_snapshot[i]) > RAMP_MOVE_THRESHOLD)
            {
                learn_target = i;
                learn_phase  = LearnPhase::kSettingHigh;
                dsy_gpio_write(&unused_button_led, 1); // solid ON
                break;
            }
        }
        break;

    case LearnPhase::kSettingHigh:
        if(!ramp_btn_held)
        {
            // Released before rate was set -> cancel, nothing mapped
            learn_phase = LearnPhase::kIdle;
            dsy_gpio_write(&unused_button_led, 0);
            break;
        }
        learn_committed_high = GetRawPotValue(learn_target);
        if(fabsf(raw_ramp_pot_value - learn_ramp_pot_snapshot) > RAMP_MOVE_THRESHOLD)
        {
            learn_phase = LearnPhase::kSettingRate;
        }
        break;

    case LearnPhase::kSettingRate:
    {
        if(!ramp_btn_held)
        {
            // Commit the ramp
            RampState &rs = ramp_state[learn_target];
            rs.active  = true;
            rs.low     = learn_snapshot[learn_target];
            rs.high    = learn_committed_high;
            rs.rate_hz = RAMP_RATE_MIN_HZ
                         * powf(RAMP_RATE_MAX_HZ / RAMP_RATE_MIN_HZ, raw_ramp_pot_value);
            rs.phase   = 0.0f;
            rs.shape   = LfoShape::kTriangle;
            last_mapped_control = learn_target;

            learn_phase = LearnPhase::kIdle;
            dsy_gpio_write(&unused_button_led, 0);
            break;
        }
        // Live preview: pulse LED at the rate currently dialed in
        float preview_rate = RAMP_RATE_MIN_HZ
                              * powf(RAMP_RATE_MAX_HZ / RAMP_RATE_MIN_HZ, raw_ramp_pot_value);
        UpdateLearnPreviewLed(preview_rate); // simple square-wave blink, non-blocking (see section 5)
        break;
    }
}
```

Design choice worth flagging to the user: if the button is released during
`kWaitingForTarget` or `kSettingHigh`, the gesture is **cancelled entirely**
(safe default — no accidental half-configured ramp). Only reaching
`kSettingRate` and then releasing commits anything.

---

## 3. Clear gesture (ramp button + footswitch)

The footswitch is a **latching** switch — a press is a state *change*, not a
momentary edge, so it must be edge-detected and its normal bypass-toggle
suppressed while the ramp button is held.

In the existing footswitch-read section:

```cpp
static bool footswitch_prev_raw = false;
bool footswitch_raw = !dsy_gpio_read(&footswitch); // active-LOW, true = closed

bool footswitch_edge = footswitch_raw && !footswitch_prev_raw;
footswitch_prev_raw  = footswitch_raw;

if(ramp_btn_held && footswitch_edge)
{
    // Clear gesture: do NOT toggle effect_engaged this press
    if(last_mapped_control >= 0)
    {
        ramp_state[last_mapped_control].active = false;
        last_mapped_control = -1;
    }
}
else
{
    // Existing normal behavior, unchanged
    effect_engaged = !dsy_gpio_read(&footswitch);
}
```

Note this changes the footswitch from level-based to needing an edge-detect
variable — check that this doesn't conflict with how `effect_engaged` is
currently read every loop iteration (currently it's read continuously, which
is fine and can stay for the non-ramp case; just skip that line on the frame
where a clear-gesture edge fires).

---

## 4. Reading pot values while some may be ramped

Add a helper that returns the *effective* raw value for any of the 9
controls — either the live pot reading, or the LFO-driven value if that
control is currently ramped:

```cpp
float GetRawPotValue(int control_index)
{
    // Maps control_index -> the actual ADC channel + inversion,
    // exactly matching the existing per-control raw-value lines.
    switch(control_index)
    {
        case kRampThreshold: return 1.0f - hw.adc.GetFloat(kThresholdPot);
        case kRampRatio:     return 1.0f - hw.adc.GetFloat(kRatioPot);
        case kRampAttack:    return 1.0f - hw.adc.GetFloat(kAttackPot);
        case kRampRelease:   return 1.0f - hw.adc.GetFloat(kReleasePot);
        case kRampMix:       return 1.0f - hw.adc.GetFloat(kMixPot);
        case kRampDrive:     return 1.0f - hw.adc.GetFloat(kDrivePot);
        case kRampWidth:     return 1.0f - hw.adc.GetFloat(kWidthPot);
        case kRampOutput:    return 1.0f - hw.adc.GetFloat(kOutputPot);
        case kRampCutoff:    return 1.0f - hw.adc.GetFloat(kFilterCutoffPot);
        default:             return 0.0f;
    }
}

float GetEffectiveRawValue(int control_index)
{
    RampState &rs = ramp_state[control_index];
    if(!rs.active)
        return GetRawPotValue(control_index);

    float lfo = EvaluateLfo(rs.phase, rs.shape); // 0..1
    return rs.low + lfo * (rs.high - rs.low);
}
```

Then in the main control loop, replace each existing line like:

```cpp
float thresh_val = 1.0f - hw.adc.GetFloat(kThresholdPot);
```

with:

```cpp
float thresh_val = GetEffectiveRawValue(kRampThreshold);
```

...and likewise for ratio, attack, release, mix, drive, width, output, and
cutoff. Everything downstream (dB/ms/etc. mapping, `comp.Set*()` calls) is
**unchanged** — this is a drop-in substitution at the raw-value stage.

`raw_ramp_pot_value` (used in section 2) is just:
```cpp
float raw_ramp_pot_value = 1.0f - hw.adc.GetFloat(kUnusedPot);
```

---

## 5. Advancing the LFOs (phase accumulator, needs real dt)

The current `while(true)` loop has no fixed period, so timing must be
measured explicitly. Use `daisy::System::GetNow()` (milliseconds):

```cpp
static uint32_t last_loop_ms = 0;
uint32_t now_ms = daisy::System::GetNow();
float dt = (last_loop_ms == 0) ? 0.0f : (now_ms - last_loop_ms) * 0.001f;
last_loop_ms = now_ms;

for(int i = 0; i < kNumRampableControls; i++)
{
    if(!ramp_state[i].active) continue;
    ramp_state[i].phase += ramp_state[i].rate_hz * dt;
    if(ramp_state[i].phase >= 1.0f)
        ramp_state[i].phase -= floorf(ramp_state[i].phase);
}
```

Place this near the top of the loop body, before `GetEffectiveRawValue()` is
called for anything.

---

## 6. LED behavior — three distinct states, one physical LED

1. **Learn mode, `kSettingHigh`**: solid ON (already shown in section 2).
2. **Learn mode, `kSettingRate`**: pulsing at the currently-dialed rate —
   simple non-blocking square blink, e.g. `sinf` of a preview phase > 0
   → on, else off. Reuse the same `EvaluateLfo`/phase-accumulator pattern
   with its own local `preview_phase`, advanced by `dt` same as above.
3. **Touching an already-ramped pot outside learn mode**: a quick **triple
   flash** (not sustained), so it's visually distinct from states 1/2.
   Implement as a small non-blocking flash queue, not `hw.DelayMs()` (that
   would stall pot reads):

```cpp
int  flash_remaining = 0; // counts down flash "half-cycles"
bool flash_led_on    = false;
uint32_t flash_next_toggle_ms = 0;

void TriggerTripleFlash()
{
    flash_remaining = RAMP_FLASH_COUNT * 2; // on+off per flash
    flash_next_toggle_ms = daisy::System::GetNow();
}

void UpdateFlash(uint32_t now_ms)
{
    if(flash_remaining <= 0) return;
    if(now_ms >= flash_next_toggle_ms)
    {
        flash_led_on = !flash_led_on;
        dsy_gpio_write(&unused_button_led, flash_led_on ? 1 : 0);
        flash_next_toggle_ms = now_ms
            + (flash_led_on ? RAMP_FLASH_ON_MS : RAMP_FLASH_OFF_MS);
        flash_remaining--;
        if(flash_remaining == 0)
            dsy_gpio_write(&unused_button_led, 0);
    }
}
```

Trigger `TriggerTripleFlash()` when, outside of learn mode
(`learn_phase == LearnPhase::kIdle`), a ramped control's live pot reading
moves more than `RAMP_MOVE_THRESHOLD` from its last-seen value. This needs
one more small piece of state — last raw value per control, checked only
when `ramp_state[i].active`:

```cpp
static float last_seen_raw[kNumRampableControls] = {0};

if(learn_phase == LearnPhase::kIdle)
{
    for(int i = 0; i < kNumRampableControls; i++)
    {
        if(!ramp_state[i].active) continue;
        float live = GetRawPotValue(i);
        if(fabsf(live - last_seen_raw[i]) > RAMP_MOVE_THRESHOLD)
            TriggerTripleFlash();
        last_seen_raw[i] = live;
    }
}
```

Call `UpdateFlash(now_ms)` once per loop iteration, and make sure it doesn't
fight with the learn-mode LED writes above (they're mutually exclusive by
`learn_phase`, so this should just work as long as flash logic only runs
when `learn_phase == kIdle`).

---

## 7. Cleanup / removal

Remove from `TestSidechainCompressor.cpp`:
- `unused_led_on`, `unused_button_prev` and the momentary-toggle block in the
  main loop (the "Unused button toggle" section).
- The `dsy_gpio_write(&unused_button_led, 1); // LED on at startup` init line
  — the ramp LED should start OFF, not ON.

Keep unchanged: all `dsy_gpio_init()` calls for `unused_button` and
`unused_button_led` (pin/mode/pull config is correct as-is) and
`PIN_UNUSED_POT` reads.

---

## 8. Open questions for you to confirm while implementing

- Confirm `RAMP_MOVE_THRESHOLD = 0.03f` feels right on the bench — pot
  jitter varies a bit by hardware; bump up if you get false-positive target
  detection or false-positive triple-flashes on idle pots.
- Confirm whether `GetEffectiveRawValue()` calls should be memoized once per
  loop iteration rather than potentially called twice (once for flash
  detection, once for the actual control read) — likely fine either way
  given ADC read cost, but worth a glance if loop timing gets tight.

---

## Addendum: Clear Gesture Redesign

*Implemented 2026-08-13. Supersedes Section 3 above.*

### What changed

The original clear gesture (hold Ramp Button + tap Footswitch to remove the
most recently mapped ramp) was replaced with a simpler, dedicated button:
**pressing `PIN_CLEAR_SW` (D8, the bootloader button) once clears all active
ramp states simultaneously.**

Motivation:
- The ramp-button + footswitch combo was awkward — it required edge-detecting
  the latching footswitch and suppressing its normal bypass toggle on the same
  press, adding complexity and a subtle interaction hazard.
- Tracking `last_mapped_control` to support "remove most-recent" was state
  that existed solely to serve that gesture; removing it simplifies the code.
- `PIN_CLEAR_SW` is already initialized at boot for the bootloader-entry check
  and can be reused in the main loop at zero hardware cost.

### What was removed

- `last_mapped_control` global variable and all assignments to it.
- Footswitch edge-detection (`footswitch_prev_raw`, `footswitch_edge`).
- The `if(ramp_btn_held && footswitch_edge)` clear-gesture branch.

### What was added / changed

- The local `boot_sw` variable in `main()` was replaced by the global
  `clear_sw` (`dsy_gpio`), initialized identically. The bootloader-entry
  check now reads `clear_sw` directly.
- In the main `while(true)` loop, a rising-edge detect on `clear_sw` clears
  all ramp states:

```cpp
static bool clear_sw_prev = false;
bool        clear_sw_raw  = !dsy_gpio_read(&clear_sw); // active-LOW
if(clear_sw_raw && !clear_sw_prev)
{
    for(int i = 0; i < kNumRampableControls; i++)
        ramp_state[i].active = false;
}
clear_sw_prev = clear_sw_raw;
```

- The footswitch is now a clean unconditional level-read again:

```cpp
bool footswitch_raw = !dsy_gpio_read(&footswitch);
effect_engaged = footswitch_raw;
```

- The `PIN_CLEAR_SW` comment in `pins.h` was already updated when the pin
  was renamed; no further pin changes were needed.

### Note on bootloader safety

Holding `PIN_CLEAR_SW` at power-on / reset still enters the bootloader
(checked before the main loop starts). A brief press during normal operation
only clears ramps — it cannot accidentally trigger a bootloader reset.

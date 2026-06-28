# Stutter Effect Unit — Design Outline
**Platform:** Daisy Seed (STM32H750, libDaisy + DaisySP)  
**Target:** Desktop stereo stutter effect with MIDI sync

---

## 1. Hardware Overview

### 1.1 Power
- Power via Daisy Seed USB connector only
- No external power regulation required
- No barrel jack

### 1.2 Audio I/O
- Stereo line level in and out
- Daisy Seed onboard codec (PCM3060, stereo 24-bit)
- Two 1/4" TRS input jacks (L, R)
- Two 1/4" TRS output jacks (L, R)
- DC-blocking capacitors (10µF) in series on all four audio lines, between jacks and Daisy Seed audio pins

### 1.3 Controls

| Control | Type | Purpose |
|---|---|---|
| Loop Length | 5-position rotary switch | Sets stutter buffer length as a musical subdivision |
| Stutter Rate + Trigger | Rotary encoder with push | Turning adjusts playback rate/pitch; pressing and holding triggers stutter |
| Wet/Dry | Potentiometer | Blends stutter output with dry signal during stutter |
| Menu Nav | Rotary encoder + push | Navigates OLED menu; confirms selection on push |

### 1.4 Display
- SSD1306 OLED, 1.3" 128×64
- I2C interface (SDA, SCL)
- Used for menu navigation, parameter display, and stutter status

### 1.5 MIDI
- DIN-5 MIDI In only (for clock sync)
- Standard optocoupler isolation circuit (6N138 or equivalent)
- UART input to Daisy Seed RX pin

### 1.6 Indicator
- Standalone LED indicates stutter active state
- LED driven from a Daisy Seed GPIO via a current-limiting resistor (~220Ω)
- LED on while stutter is active (RECORDING or PLAYING state)
- OLED status line also reflects stutter state

### 1.7 Rate Encoder Behavior
- Modeled on the Synthstrom Deluge stutter implementation
- **Push and hold** the rate encoder to engage stutter; release to stop (momentary trigger)
- **Turn while held** (or at any time) to adjust playback rate in real time
- Rate initializes to 1.0 (unity) on every power-up — no physical position to find
- Rate is a persistent runtime value; not stored to flash (intentional: always starts at unity)
- Rate range: approximately 0.25× to 4.0× (two octaves down to two octaves up)
- Each encoder detent step changes rate by a small fixed increment (tune to taste; ~0.05 suggested)

---

## 2. Pin Assignments

| Daisy Seed Pin | Function |
|---|---|
| Audio In L | Codec left channel in |
| Audio In R | Codec right channel in |
| Audio Out L | Codec left channel out |
| Audio Out R | Codec right channel out |
| ADC 0 (A0 / D15) | Wet/Dry pot (0–3.3V) |
| GPIO D0 (UART5_RX) | MIDI In (via optocoupler) |
| GPIO D1 (digital in) | Menu encoder A |
| GPIO D2 (digital in) | Menu encoder B |
| GPIO D3 (digital in, pull-up) | Menu encoder push (active low) |
| GPIO D4 (digital in) | Loop Length rotary switch — position 1 (1/32) |
| GPIO D5 (digital in) | Loop Length rotary switch — position 2 (1/16) |
| GPIO D6 (digital in) | Loop Length rotary switch — position 3 (1/8) |
| GPIO D7 (digital in) | Loop Length rotary switch — position 4 (1/4) |
| GPIO D8 (digital in) | Loop Length rotary switch — position 5 (1/2) |
| GPIO D9 (digital in) | Rate encoder A |
| GPIO D10 (digital in) | Rate encoder B |
| GPIO D11 (digital in, pull-up) | Rate encoder push = stutter trigger (active low) |
| GPIO D12 (digital out) | Stutter indicator LED |
| I2C D13 | OLED SCL (I2C1, fixed pin) |
| I2C D14 | OLED SDA (I2C1, fixed pin) |

> **Note:** The 5-position rotary switch is wired one-hot: common terminal → GND, each position terminal → one GPIO with internal pull-up. The active position reads LOW; all others read HIGH.

> **Note on D14:** D14 is also capable of UART1_RX. Since D14 is used here for I2C1_SDA, UART1 is unavailable. MIDI therefore uses UART5_RX on D0 instead, which has no conflict.

---

## 3. Firmware Architecture

### 3.1 Top-Level Structure

```
main()
  └── HardwareInit()
  └── LoadConfig()          // read PedalConfig from flash
  └── StartAudioCallback()  // runs at audio rate, highest priority
  └── loop()                // main loop, runs at control rate
        ├── PollControls()
        ├── PollMidi()
        ├── UpdateMenu()
        └── UpdateLed()
```

### 3.2 Audio Callback

Runs at audio rate (48kHz recommended). Must be fast and non-blocking. No heap allocation, no I2C, no flash access.

Responsibilities:
- Read current values from shared state (config, stutter state, smoothed rate)
- If stutter is recording: write incoming samples to stutter buffer
- If stutter is playing: read from stutter buffer at current playback position, output via fixed-point accumulator
- If stutter is inactive: pass dry signal through unchanged
- Apply wet/dry blend during stutter playback
- Advance smoothed rate value via one-pole lowpass filter

### 3.3 Main Loop

Runs at control rate (~1kHz or slower). Handles:
- Rate encoder push: debounced read; sets trigger_active true while held, false on release
- Rate encoder rotation: increment/decrement target_rate by step value each detent
- Menu encoder rotation and push: menu navigation and selection
- ADC read for wet/dry pot
- Rotary switch reads (loop length)
- MIDI byte parsing
- OLED menu rendering
- Writing updated values to shared state

---

## 4. Shared State

All values shared between the main loop and the audio callback must be declared `volatile` or protected against race conditions. On Cortex-M7, simple scalar reads/writes of aligned 32-bit values are atomic; prefer this where possible.

```c
// Stutter machine states
typedef enum {
    STUTTER_IDLE,
    STUTTER_RECORDING,
    STUTTER_PLAYING
} StutterState;

// Persistent configuration (stored in flash)
typedef struct {
    bool     quantize_trigger;   // false = immediate (option 1), true = beat-quantized (option 2)
    uint8_t  midi_sync_enabled;  // 0 = free-running, 1 = MIDI sync
    // add future settings here
} PedalConfig;

// Runtime state (in RAM, shared between main loop and audio callback)
typedef struct {
    volatile StutterState state;
    volatile float        rate;           // current smoothed playback rate (1.0 = unity)
    volatile float        target_rate;    // set by encoder in main loop; smoothed toward in callback
    volatile float        wet;            // current wet/dry blend (0.0–1.0)
    volatile uint32_t     buffer_length;  // in samples, derived from loop length switch + tempo
    volatile bool         trigger_active; // true while rate encoder push is held
} StutterRuntime;
```

> **Note:** `target_rate` is written by the main loop (encoder reads) and read by the audio callback (smoothing filter). `rate` is written and read only by the audio callback. This single-writer pattern is safe without a mutex on Cortex-M7 with aligned 32-bit access.

---

## 5. Stutter Effect Implementation

### 5.1 Buffer Design
- Two-channel (stereo) sample buffer allocated statically in RAM
- Maximum buffer size should cover the largest subdivision at the lowest expected tempo
  - Example: 1/2 note at 60 BPM = 2 seconds = 96,000 samples at 48kHz
  - Allocate ~100,000 samples × 2 channels × 4 bytes = ~800KB
  - Daisy Seed has 512KB internal SRAM + 64MB SDRAM; use SDRAM via `DSY_SDRAM_BSS` attribute
- Buffer is written once per stutter trigger (record phase), then looped (playback phase)

```c
#define STUTTER_BUFFER_MAX_SAMPLES 100000
DSY_SDRAM_BSS float stutter_buf_l[STUTTER_BUFFER_MAX_SAMPLES];
DSY_SDRAM_BSS float stutter_buf_r[STUTTER_BUFFER_MAX_SAMPLES];
```

### 5.2 Record Phase
- Begins on trigger press (rate encoder push held; `STUTTER_IDLE` → `STUTTER_RECORDING`)
- Write incoming L and R samples sequentially to buffer
- Advance `write_pos` each sample
- When `write_pos >= buffer_length`, transition to `STUTTER_PLAYING`, reset `read_pos_accum` to 0

### 5.3 Playback Phase
- Read from buffer using a fixed-point fractional accumulator for variable rate
- Accumulator advances by `rate` each sample
- Use linear interpolation between adjacent samples for smooth output at non-unity rates
- When accumulator reaches `buffer_length`, wrap to 0 (loop)
- On trigger release (rate encoder push released), transition to `STUTTER_IDLE`; output dry signal

```c
// Fixed-point accumulator approach (in audio callback):
read_pos_accum += rate;
if (read_pos_accum >= buffer_length) read_pos_accum -= buffer_length;

uint32_t idx  = (uint32_t)read_pos_accum;
float    frac = read_pos_accum - idx;
uint32_t idx1 = (idx + 1 < buffer_length) ? idx + 1 : 0;

float out_l = stutter_buf_l[idx] + frac * (stutter_buf_l[idx1] - stutter_buf_l[idx]);
float out_r = stutter_buf_r[idx] + frac * (stutter_buf_r[idx1] - stutter_buf_r[idx]);
```

### 5.4 Wet/Dry Blend
- Applied only during `STUTTER_PLAYING`
- During `STUTTER_IDLE` and `STUTTER_RECORDING`, pass dry signal 100% regardless of knob position

```c
if (state == STUTTER_PLAYING) {
    out_l = (dry_l * (1.0f - wet)) + (out_l * wet);
    out_r = (dry_r * (1.0f - wet)) + (out_r * wet);
} else {
    out_l = dry_l;
    out_r = dry_r;
}
```

### 5.5 Rate Smoothing
- Apply a one-pole lowpass filter to the rate value in the audio callback to prevent clicks on encoder steps

```c
// In audio callback, each block:
rate += 0.001f * (target_rate - rate);
```

Tune the coefficient (`0.001f`) to taste — smaller = slower/smoother, larger = more responsive.

### 5.6 Rate Encoder Handling (Main Loop)

```c
// In main loop, read rate encoder:
int32_t rate_enc_increment = rate_encoder.Increment(); // from libDaisy Encoder class
if (rate_enc_increment != 0) {
    runtime.target_rate += rate_enc_increment * RATE_STEP;
    runtime.target_rate = fclamp(runtime.target_rate, RATE_MIN, RATE_MAX);
}

// Rate encoder push = stutter trigger:
runtime.trigger_active = !rate_encoder_push.Read(); // active low
```

Suggested constants:
```c
#define RATE_STEP  0.05f   // per encoder detent; tune to taste
#define RATE_MIN   0.25f   // two octaves down
#define RATE_MAX   4.0f    // two octaves up
```

On boot, initialize `runtime.target_rate = 1.0f` and `runtime.rate = 1.0f`.

---

## 6. MIDI Sync

### 6.1 Messages to Handle

| Message | Byte | Action |
|---|---|---|
| Clock | 0xF8 | Increment pulse counter; measure inter-pulse interval |
| Start | 0xFA | Reset pulse counter to 0 |
| Stop | 0xFC | Pause pulse counter (do not reset) |
| Continue | 0xFB | Resume pulse counter |

### 6.2 Tempo Derivation
- Record timestamp of each incoming 0xF8 clock tick (use `DWT->CYCCNT` or a hardware timer)
- BPM = 60 / (24 × inter-pulse interval in seconds)
- Apply light averaging (e.g., rolling average of last 4 intervals) to smooth jitter
- Derive buffer length in samples:

```c
float beat_duration_sec = 60.0f / bpm;
float subdiv_duration_sec = beat_duration_sec * subdiv_multiplier; // e.g. 0.25 for 1/16
uint32_t buffer_length = (uint32_t)(subdiv_duration_sec * SAMPLE_RATE);
buffer_length = MIN(buffer_length, STUTTER_BUFFER_MAX_SAMPLES);
```

### 6.3 Subdivision Multipliers (Loop Length Rotary Switch)

| Position | Subdivision | Multiplier (relative to quarter note) |
|---|---|---|
| 1 | 1/32 | 0.125 |
| 2 | 1/16 | 0.25 |
| 3 | 1/8 | 0.5 |
| 4 | 1/4 | 1.0 |
| 5 | 1/2 | 2.0 |

### 6.4 Free-Running Fallback
- If no MIDI clock has been received within 2 seconds, derive buffer length from a default BPM (e.g., 120) rather than blocking or erroring
- Display "NO CLOCK" on OLED when in this state if MIDI sync is enabled in config

---

## 7. Menu System

### 7.1 Navigation Model
- Menu encoder rotation scrolls through menu items
- Menu encoder push selects item / confirms value
- Single menu level for now; no nested submenus required initially
- Menu encoder is entirely separate from rate encoder; both are always active

### 7.2 Initial Menu Items

| Item | Type | Values |
|---|---|---|
| MIDI Sync | Toggle | Off / On |
| Quantize Trigger | Toggle | Off / On |

### 7.3 Display Layout (128×64 SSD1306)
- Line 0: Unit status ("IDLE", "RECORDING", "PLAYING", "NO CLOCK")
- Line 1: Current rate value (e.g. "RATE: 1.00x")
- Lines 2–3: Menu items, current item highlighted with cursor or inverted text
- Line 4: Current BPM (if MIDI sync active)

### 7.4 Menu Should Not Block Audio
- OLED writes happen only in the main loop, never in the audio callback
- libDaisy's `OledDisplay` driver handles I2C; call `display.Update()` in main loop only

---

## 8. Configuration Persistence

- `PedalConfig` struct stored in Daisy Seed internal flash (use libDaisy `PersistentStorage` class)
- Load on boot; save only when a value changes (to manage flash write endurance)
- A "dirty" flag pattern is sufficient: set flag when config changes, write flash at end of main loop iteration if dirty, clear flag
- Rate is NOT stored to flash; always initializes to 1.0 on boot (intentional)

---

## 9. Build Environment

- **Framework:** libDaisy + DaisySP
- **Toolchain:** ARM GCC via the Daisy toolchain installer
- **Build system:** CMake (preferred) or Makefile per libDaisy examples
- **Flash tool:** `dfu-util` via Daisy bootloader USB DFU mode
- **Reference examples to study:**
  - `libDaisy/examples/OledDisplay` — I2C OLED setup
  - `libDaisy/examples/Encoder` — encoder handling (use for both menu encoder and rate encoder)
  - `libDaisy/examples/MIDI` — UART MIDI parsing
  - `DaisySP/examples` — audio callback patterns

---

## 10. Implementation Order (Suggested)

1. **Scaffold:** Init Daisy Seed, audio passthrough (dry signal L+R in, L+R out)
2. **Stutter core:** Implement record/play state machine with fixed buffer length, rate encoder push as trigger, unity rate
3. **Rate control:** Add rate encoder rotation, fixed-point accumulator, linear interpolation, rate smoothing
4. **Wet/dry:** Add blend pot, conditional blend in audio callback
5. **Loop length:** Add rotary switch, hardcoded 120 BPM free-running tempo calculation
6. **MIDI sync:** Add MIDI input, clock parsing, live buffer length derivation
7. **Config struct:** Add `PedalConfig`, load/save, wire quantize and sync flags
8. **OLED + menu encoder:** Add menu system, expose config items, show rate and status
9. **LED:** Wire stutter indicator LED to stutter active state
10. **Polish:** Rate smoothing tuning, OLED status display, NO CLOCK fallback

---

## 11. Key Implementation Notes for Coding Agent

- **Never allocate heap in the audio callback.** All buffers must be statically allocated before audio starts.
- **SDRAM requires the `DSY_SDRAM_BSS` attribute** for stutter buffer variables; do not place them in regular BSS.
- **Audio callback and main loop share state.** Use `volatile` on all shared scalars. For the stutter state enum, a volatile uint8_t cast is acceptable; no mutex needed for single-writer single-reader scalar values on Cortex-M7 with aligned access.
- **The stutter is record-then-loop, not a ring buffer.** The buffer is filled once on trigger press, then looped indefinitely until release. This is architecturally distinct from a delay effect.
- **Rate affects pitch.** Rate > 1.0 = faster playback = higher pitch. Rate < 1.0 = slower = lower pitch. This is intentional and desirable.
- **Rate encoder push is the stutter trigger.** There is no separate trigger button. The rate encoder serves dual purpose: rotation = rate, push-and-hold = trigger. This matches Deluge behavior.
- **Rate always initializes to 1.0 on boot.** Do not persist rate to flash. The encoder is relative so there is no physical position to restore anyway.
- **Two encoder instances are needed:** one for the rate/trigger control (D9/D10/D11) and one for menu navigation (D1/D2/D3). Use libDaisy's `Encoder` class for both.
- **libDaisy's MIDI handler** (`MidiUartHandler`) parses MIDI bytes and provides a message queue; use it rather than parsing raw UART bytes manually. Real-time messages (0xF8, 0xFA etc.) are delivered separately from note/CC messages — check the libDaisy API for `GetRealTimeMessage()` or equivalent.
- **SSD1306 driver** is available in libDaisy as `OledDisplay<SSD130x4WireSoftSpiTransport>` (SPI) or I2C variant. Use the I2C variant to preserve SPI pins.
- **D14 / UART1_RX conflict:** D14 is used for I2C1_SDA (OLED). UART1_RX shares this pin and cannot be used simultaneously. MIDI uses UART5_RX on D0 instead. Do not attempt to use UART1 for MIDI.

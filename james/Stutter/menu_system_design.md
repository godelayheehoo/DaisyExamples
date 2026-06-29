# Stutter Effect Unit — Menu System Design
**Replaces / expands Section 7 of the main design outline**

---

## 7. Menu System

### 7.1 Display Hardware & Font

- **Driver:** SSD1306, 128×64 pixels, I2C (use libDaisy `OledDisplay` with I2C transport)
- **Font:** `Font_7x10` — 7px wide, 10px tall, fixed-width
  - Characters per row: **18** (128 ÷ 7 = 18, 2px remainder unused)
  - Rows available: **6** (y = 0, 10, 20, 30, 40, 50; bottom 4px unused)
- **Update rate:** ~20 Hz (every 50ms) in the main loop; do not update faster as it wastes I2C bandwidth

---

### 7.2 Screen Inventory

Four screens exist. Only one is active at a time.

| Screen | When shown |
|--------|-----------|
| `STATUS` | Default — shown on boot and after idle timeout or back-navigation from `BROWSE` |
| `BROWSE` | After short press from `STATUS`; encoder scrolls the item cursor |
| `EDIT` | After short press on a settings menu item in `BROWSE`; encoder changes the item's value |
| `DEBUG` | After short press on the `DEBUG` item in `BROWSE`; displays live hardware states |

---

### 7.3 Menu State Machine

```
              [short press]              [short press]
  STATUS ───────────────────→ BROWSE ───────────────────→ EDIT
    ↑                            │  (if not DEBUG)           │
    │     [5s idle timeout]      ├───────────────────────────┤ [short press = confirm]
    │                            │  (if DEBUG)               │
    │                            ▼                           │
    │                          DEBUG                         │
    │                            │                           │
    │     [long press ≥500ms]    │  [short press / long]     │
    └────────────────────────────┼───────────────────────────┘
                                 │  [press = back]
                                 ▼
                               BROWSE
```

**Transition rules:**

| From | Event | To | Side effect |
|------|-------|----|-------------|
| `STATUS` | Short press | `BROWSE` | Reset `idle_timer_ms` to 0 |
| `BROWSE` | Encoder rotate | `BROWSE` | Move cursor; reset `idle_timer_ms` |
| `BROWSE` | Short press | `EDIT` | Copy current `PedalConfig` into `edit_shadow` (if cursor is not on `DEBUG` item) |
| `BROWSE` | Short press | `DEBUG` | Transition to debug screen (if cursor is on `DEBUG` item) |
| `BROWSE` | Long press ≥500ms | `STATUS` | — |
| `BROWSE` | Idle timeout (5s) | `STATUS` | — |
| `EDIT` | Encoder rotate | `EDIT` | Cycle/toggle value of selected item |
| `EDIT` | Short press | `BROWSE` | Commit value to `PedalConfig`; set `dirty = true` |
| `EDIT` | Long press ≥500ms | `BROWSE` | Restore `edit_shadow` into `PedalConfig`; discard changes |
| `DEBUG` | Short press | `BROWSE` | — |
| `DEBUG` | Long press ≥500ms | `BROWSE` | — |

**Note:** There is no auto-timeout from `EDIT`. The user must explicitly confirm (short press) or cancel (long press).

---

### 7.4 Data Structures

```c
typedef enum {
    MENU_STATE_STATUS = 0,
    MENU_STATE_BROWSE,
    MENU_STATE_EDIT,
    MENU_STATE_DEBUG,
} MenuState;

typedef enum {
    MENU_ITEM_MIDI_SYNC = 0,
    MENU_ITEM_QUANTIZE_TRIGGER,
    MENU_ITEM_DEBUG,
    MENU_ITEM_COUNT   // always last; used for bounds checking and loop limits
} MenuItemId;

typedef struct {
    MenuState   state;
    int         cursor;          // index of highlighted item; 0..(MENU_ITEM_COUNT-1)
    uint32_t    idle_timer_ms;   // ms since last encoder activity; reset on any encoder event
    bool        dirty;           // true when PedalConfig has changed and needs flash write
    bool        needs_redraw;    // set true on any state/value change; cleared after Update()
    PedalConfig edit_shadow;     // snapshot of config at BROWSE→EDIT entry; restored on cancel
} MenuContext;
```

---

### 7.5 Screen Layouts

All screens use `Font_7x10`. Each row is 10px tall. Coordinates are `(x, y)` pixels.  
Row y-positions: 0, 10, 20, 30, 40, 50.

---

#### 7.5.1 STATUS Screen

Operational at-a-glance view. Updated every render cycle from live runtime values.

```
┌──────────────────┐  ← 18 chars wide
│STUTTER      IDLE │  y=0   unit name left; state tag right-aligned
│BPM: 128.0        │  y=10  current BPM (or "BPM: ---" if no clock)
│SUBDIV: 1/8   FREE│  y=20  subdivision label left; sync mode right ("SYNC"/"FREE")
│RATE: 1.00        │  y=30  smoothed rate value, 2 decimal places
│WET:  0.75        │  y=40  wet/dry blend, 2 decimal places
│[>]=MENU          │  y=50  static hint
└──────────────────┘
```

**State tag** (4 chars, right-aligned at x=100):

| `StutterState` | Tag |
|---|---|
| `STUTTER_IDLE` | `"IDLE"` |
| `STUTTER_RECORDING` | `"REC "` |
| `STUTTER_PLAYING` | `"PLAY"` |

**BPM display:** Format to one decimal place (`"BPM: 128.0"`). If no MIDI clock has been received in the last 2 seconds and MIDI sync is enabled, show `"BPM: --- "`.

**Sync mode:** Show `"SYNC"` when MIDI sync is active and clock is live; `"FREE"` when free-running (no clock or sync disabled).

**Rate value:** Read from `StutterRuntime::rate` (the smoothed value). Format as `"RATE: X.XX"`.

**Wet value:** Read from `StutterRuntime::wet`. Format as `"WET:  X.XX"` (two spaces after colon to align decimal points).

---

#### 7.5.2 BROWSE Screen

```
┌──────────────────┐
│-- SETTINGS --    │  y=0   header, static
│> MIDI SYNC    ON │  y=10  item 0 (cursor on this item = entire row inverted)
│  QUANTIZE    OFF │  y=20  item 1
│                  │  y=30  (future item slot)
│                  │  y=40  (future item slot)
│[>]=SEL [H]=BACK  │  y=50  control hints; [H] = long press
└──────────────────┘
```

**Row format for each menu item (18 chars total):**

```
Col: 0         1
     012345678901234567
     C L..........VVVVV
```

| Field | Chars | Content |
|-------|-------|---------|
| C | 1 | `>` if cursor is on this row, otherwise ` ` |
| spacer | 1 | always ` ` |
| Label | 10 | item label, left-aligned, space-padded to exactly 10 chars |
| spacer | 1 | always ` ` |
| Value | 5 | value string, right-padded to exactly 5 chars |

**Item labels and value strings:**

| `MenuItemId` | Label (10 chars) | Value false (5 chars) | Value true (5 chars) |
|---|---|---|---|
| `MENU_ITEM_MIDI_SYNC` | `"MIDI SYNC "` | `"  OFF"` | `"   ON"` |
| `MENU_ITEM_QUANTIZE_TRIGGER` | `"QUANTIZE  "` | `"  OFF"` | `"   ON"` |
| `MENU_ITEM_DEBUG` | `"DEBUG     "` | `"ENTER"` | `"ENTER"` |

**Selected row rendering:** Render the entire 18-char row with `invert = true` for the item where `cursor == item_index`. All other rows use `invert = false`.

Use `display.WriteString(row_buf, Font_7x10, invert)` where `row_buf` is the fully-formatted 18-char string.

---

#### 7.5.3 EDIT Screen

Same layout as BROWSE with two differences:

1. Header row changes to `"-- EDITING -- "` (padded to 18 chars)
2. Only the **value field** (rightmost 5 chars of the selected row) is rendered inverted; the cursor `>` and label are rendered normally

This means you must split the row write into two calls for the selected item:
- Write `"> LABEL     "` (13 chars) at x=0 with `invert = false`
- Write `"VVVVV"` (5 chars) at x=91 with `invert = true`
  - x = 128 − (5 × 7) = 93 pixels

The hint row changes to: `"[>]=OK [H]=CNCL"` (confirm vs. cancel).

```
┌──────────────────┐
│-- EDITING --     │  y=0
│> MIDI SYNC   [ON]│  y=10  label normal; value inverted (shown here as [ON])
│  QUANTIZE    OFF │  y=20  non-selected item, normal
│                  │  y=30
│                  │  y=40
│[>]=OK [H]=CNCL   │  y=50
└──────────────────┘

---

#### 7.5.4 DEBUG Screen

Displays the live hardware states of the potentiometer, both encoders' buttons, and both encoders' A/B pins. This is useful for checking hardware connections and debugging controls.

```
┌──────────────────┐
│-- DEBUG --       │  y=0   header, static
│POT (A0): 0.52    │  y=10  pot value (0.00 to 1.00)
│MENU A:1 B:0 SW:1 │  y=20  menu encoder pins (A, B) and button (SW, active low/high)
│RATE A:0 B:1 SW:0 │  y=30  rate encoder pins (A, B) and button (SW)
│ROT:2 BAK:1 CON:1 │  y=40  rotary switch pos, back button (BAK), confirm button (CON)
│[>]=BACK          │  y=50  hint to exit
└──────────────────┘
```

**Display details:**
- **POT (A0):** Read from the ADC input of the Wet/Dry pot (GPIO D15). Format as `"POT (A0): X.XX"`.
- **MENU A/B/SW:** Read states of menu encoder A pin (GPIO D1), menu encoder B pin (GPIO D2), and menu encoder button (GPIO D3). `1` = High/Released, `0` = Low/Pressed.
- **RATE A/B/SW:** Read states of rate encoder A pin (GPIO D9), rate encoder B pin (GPIO D10), and rate encoder button (GPIO D11). `1` = High/Released, `0` = Low/Pressed.
- **ROT/BAK/CON:** Read the rotary switch index `ROT` (0 to 4), Back button `BAK` pin state (GPIO D25), and Confirm button `CON` pin state (GPIO D26). `1` = High/Released, `0` = Low/Pressed.
- **Hint:** Show `"[>]=BACK"` to indicate that pressing the encoder button will return to the settings menu.
```

---

### 7.6 Controls Interaction

Three encoder events and two hardware button events to detect and dispatch in the main loop:

| Event | Detection |
|-------|-----------|
| **Rotate** | Quadrature increment/decrement; ±1 per detent |
| **Short press / CON** | Encoder button release after hold < 500ms, or Confirm button (CON) press |
| **Long press** | Encoder button held ≥ 500ms; fire on threshold crossing, not on release |
| **BAK button** | Back button (BAK) press |

**Behavior by state:**

| State | Rotate | Short press / CON | Long press | BAK button |
|-------|--------|-------------------|------------|------------|
| `STATUS` | ignored | → `BROWSE` | ignored | ignored |
| `BROWSE` | Move cursor ±1, clamp to [0, `MENU_ITEM_COUNT-1`]; reset `idle_timer_ms` | If `cursor == MENU_ITEM_DEBUG` → `DEBUG`<br>Else → `EDIT`, copy config to `edit_shadow` | → `STATUS` | → `STATUS` |
| `EDIT` | Toggle/cycle value of `cfg[cursor]` (see below) | Commit; set `dirty = true`; → `BROWSE` | Restore `edit_shadow` → `cfg`; → `BROWSE` | Restore `edit_shadow` → `cfg`; → `BROWSE` |
| `DEBUG` | ignored | → `BROWSE` | → `BROWSE` | → `BROWSE` |

**Cursor clamping in BROWSE:** Do not wrap — stop at 0 and `MENU_ITEM_COUNT - 1`. This prevents accidentally skipping past the first or last item.

**Value cycling in EDIT:** For boolean items (the current set), toggle on every encoder step regardless of direction. If future items have more than two values, increment on CW and decrement on CCW, wrapping at the ends.

```c
// Example toggle for boolean items:
case MENU_ITEM_MIDI_SYNC:
    cfg->midi_sync_enabled = !cfg->midi_sync_enabled;
    break;
case MENU_ITEM_QUANTIZE_TRIGGER:
    cfg->quantize_trigger = !cfg->quantize_trigger;
    break;
```

---

### 7.7 Idle Timeout

- Increment `idle_timer_ms` in the main loop by the elapsed ms each iteration (use `System::GetNow()` delta).
- Any encoder rotate or button event resets it to 0.
- While in `BROWSE` state: if `idle_timer_ms >= 5000`, transition to `STATUS`.
- While in `EDIT` state: idle timeout is **not active** — no forced transition.
- While in `DEBUG` state: idle timeout is **not active** — no forced transition.

---

### 7.8 Config Persistence Trigger

- After any confirmed edit (short press in `EDIT` → `BROWSE`), `dirty` is set `true`.
- At the **end** of each main loop iteration: if `dirty == true`, call `PersistentStorage::Save()`, then set `dirty = false`.
- Never write to flash from within `EDIT` state mid-edit, from the audio callback, or from a cancel.

---

### 7.9 Function Signatures

```c
// ── Public API ────────────────────────────────────────────────────────────────

// Call once at startup to initialize the menu context
void MenuInit(MenuContext* ctx);

// Call from main loop every iteration; elapsed_ms = ms since last call
void MenuTick(MenuContext* ctx, PedalConfig* cfg, uint32_t elapsed_ms);

// Call from main loop on encoder events
void MenuHandleRotate(MenuContext* ctx, PedalConfig* cfg, int delta);  // delta: +1 or -1
void MenuHandleShortPress(MenuContext* ctx, PedalConfig* cfg);
void MenuHandleLongPress(MenuContext* ctx, PedalConfig* cfg);

// Call from main loop at ~20Hz to render; pass live runtime for STATUS screen
void MenuRender(OledDisplay<SSD130xI2C128x64Driver>& display,
                const MenuContext* ctx,
                const PedalConfig* cfg,
                const StutterRuntime* rt);

// ── Internal helpers (static, called only from MenuRender) ────────────────────

static void RenderStatusScreen(OledDisplay<...>& display,
                                const PedalConfig* cfg,
                                const StutterRuntime* rt);

static void RenderBrowseScreen(OledDisplay<...>& display,
                                const MenuContext* ctx,
                                const PedalConfig* cfg);

static void RenderEditScreen(OledDisplay<...>& display,
                              const MenuContext* ctx,
                              const PedalConfig* cfg);

static void RenderDebugScreen(OledDisplay<...>& display,
                              const MenuContext* ctx,
                              const PedalConfig* cfg,
                              const StutterRuntime* rt);

static void RenderItemRow(OledDisplay<...>& display,
                           int y,
                           bool cursor_here,
                           const char* label,   // exactly 10 chars
                           const char* value,   // exactly 5 chars
                           bool value_inverted);
```

---

### 7.10 Display Update Sequencing

Every `MenuRender()` call must follow this sequence — no exceptions:

1. `display.Fill(false)` — clear the pixel buffer
2. Write all content for the current screen (all six rows)
3. `display.Update()` — push buffer to SSD1306 over I2C

Do not call `display.Update()` mid-frame. Write all content between `Fill` and `Update`.

**Throttle to 20 Hz:**

```c
static uint32_t last_display_ms = 0;
uint32_t now_ms = System::GetNow();
if (now_ms - last_display_ms >= 50) {
    MenuRender(display, &menu_ctx, &config, &runtime);
    last_display_ms = now_ms;
}
```

---

### 7.11 "NO CLOCK" Status

When `PedalConfig::midi_sync_enabled == true` but no MIDI clock byte (0xF8) has been received in the last 2 seconds:

- STATUS screen row 1 shows: `"BPM: ---         "`
- STATUS screen row 2 sync field shows: `"NCLOCK"` (or `"WAIT  "` — pick one and be consistent)
- The firmware still operates using the free-running fallback BPM (120) during this state

---

### 7.12 Extending the Menu

To add a future menu item:

1. Add a new value to `MenuItemId` before `MENU_ITEM_COUNT`
2. Add a corresponding entry to the label/value string table used by `RenderItemRow()`
3. Add a field to `PedalConfig` for the new setting
4. Add a `case` to `MenuHandleRotate()` for the new item's edit behavior
5. No changes to the state machine, render dispatch, or timeout logic required

The 6-row display supports up to 4 menu items with the current header and hint rows. If more items are needed later, implement scrolling: only render a window of items and track a `scroll_offset` alongside `cursor`.

---

### 7.13 Notes for Coding Agent

- **Do not call `display.Update()` from the audio callback.** I2C is blocking and will cause audio glitches. OLED writes happen only in the main loop.
- **`edit_shadow` prevents live-preview of edits.** Values are written to `PedalConfig` only on confirmed short press. During `EDIT`, the audio callback continues reading from the live `PedalConfig`; changes mid-edit are not heard until confirmed. This is intentional — avoid surprising the user with audio changes while they're still deciding.
- **`needs_redraw` optimization is optional.** If the display is rendered at 20 Hz unconditionally, there is no correctness issue. `needs_redraw` can be added later to skip I2C writes on frames where nothing changed, if power or timing budget requires it.
- **Right-align all numeric values** in their fields. Format floats with `snprintf(buf, sizeof(buf), "%5.2f", val)` to produce exactly 5 chars (e.g., `" 1.00"`, `"12.50"`).
- **libDaisy `System::GetNow()`** returns milliseconds since boot as a `uint32_t`. Use delta timing (`now - last`) for all timers; don't accumulate absolute time.
- **Encoder debounce:** libDaisy's `Encoder` class handles quadrature decoding; call `encoder.Debounce()` in the main loop and use `encoder.Increment()` for rotation and `encoder.RisingEdge()` / `encoder.FallingEdge()` for button events. Track button-down timestamp for long-press detection.

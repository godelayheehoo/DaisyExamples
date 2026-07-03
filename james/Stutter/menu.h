#pragma once
#include "daisy_seed.h"
#include "sh1106_driver.h"


// Stutter machine states
typedef enum
{
    STUTTER_IDLE = 0,
    STUTTER_RECORDING,
    STUTTER_PLAYING
} StutterState;

// Playback rate quantization modes
typedef enum
{
    PRM_OFF = 0, // Continuous (no quantization)
    PRM_LFQ = 1, // Loop-frequency quantization
    PRM_PTQ = 2, // Pitch-detection quantization
    PRM_COUNT
} PlaybackRateMode;

// Persistent configuration (stored in flash)
typedef struct PedalConfig
{
    bool    quantize_trigger;   // false = immediate, true = beat-quantized
    uint8_t midi_sync_enabled;  // 0 = free-running, 1 = MIDI sync
    uint8_t playback_rate_mode; // 0=OFF, 1=LFQ, 2=PTQ

    inline bool operator==(const PedalConfig& other) const
    {
        return quantize_trigger == other.quantize_trigger
               && midi_sync_enabled == other.midi_sync_enabled
               && playback_rate_mode == other.playback_rate_mode;
    }
    inline bool operator!=(const PedalConfig& other) const
    {
        return !(*this == other);
    }
} PedalConfig;

// Runtime state (in RAM, shared between main loop and audio callback)
typedef struct
{
    volatile StutterState state;
    volatile float        rate; // current smoothed playback rate (1.0 = unity)
    volatile float        target_rate;   // set by encoder in main loop
    volatile float        wet;           // current wet/dry blend (0.0-1.0)
    volatile uint32_t     buffer_length; // in samples
    volatile bool     trigger_active;    // true while rate encoder push is held
    volatile float    bpm;               // current tempo in BPM
    volatile bool     has_clock;         // true if MIDI clock is active
    volatile int      subdiv_pos; // current loop length rotary position (0-4)
    volatile uint32_t midi_event_count; // total MIDI events processed
    volatile bool     midi_play_seen; // true if MIDI Start or Continue received
    volatile uint32_t midi_clock_ticks; // counts incoming timing clocks
    volatile bool
        quantize_trigger; // copy of persistent config flag for audio thread
    volatile uint8_t playback_rate_mode; // copy of config for audio/main loop
    volatile float
        loop_frequency_hz; // sampleRate / bufferSize, set per capture
    volatile float   detected_pitch_hz; // from autocorrelation pitch detection
    volatile float   pitch_confidence;  // 0.0–1.0
    volatile bool    pitch_valid;       // true if confidence > 0.7
    volatile bool    pitch_detection_pending; // main loop should run detector
    volatile int32_t semitone_offset;         // current quantized knob position
} StutterRuntime;

typedef enum
{
    MENU_STATE_STATUS = 0,
    MENU_STATE_BROWSE,
    MENU_STATE_EDIT,
    MENU_STATE_DEBUG,
} MenuState;

typedef enum
{
    MENU_ITEM_MIDI_SYNC = 0,
    MENU_ITEM_QUANTIZE_TRIGGER,
    MENU_ITEM_RATE_MODE,
    MENU_ITEM_DEBUG,
    MENU_ITEM_COUNT // always last; used for bounds checking and loop limits
} MenuItemId;

typedef struct
{
    MenuState state;
    int       cursor; // index of highlighted item; 0..(MENU_ITEM_COUNT-1)
    uint32_t
        idle_timer_ms; // ms since last encoder activity; reset on any encoder event
    bool dirty; // true when PedalConfig has changed and needs flash write
    bool needs_redraw; // set true on any state/value change
    PedalConfig
        edit_shadow; // snapshot of config at BROWSE->EDIT entry; restored on cancel
} MenuContext;

// External declarations of raw pins for debug screen
extern daisy::GPIO menu_pin_a;
extern daisy::GPIO menu_pin_b;
extern daisy::GPIO menu_pin_sw;
extern daisy::GPIO menu_pin_bak;
extern daisy::GPIO menu_pin_con;
extern daisy::GPIO rate_pin_a;
extern daisy::GPIO rate_pin_b;
extern daisy::GPIO rate_pin_sw;


// OLED Display Type Definition
typedef daisy::OledDisplay<custom_daisy::SH1106I2c128x64Driver> StutterDisplay;

// ── Public API ────────────────────────────────────────────────────────────────

// Call once at startup to initialize the menu context
void MenuInit(MenuContext* ctx);

// Call from main loop every iteration; elapsed_ms = ms since last call
void MenuTick(MenuContext* ctx, PedalConfig* cfg, uint32_t elapsed_ms);

// Call from main loop on encoder/button events
void MenuHandleRotate(MenuContext* ctx,
                      PedalConfig* cfg,
                      int          delta); // delta: +1 or -1
void MenuHandleShortPress(MenuContext* ctx, PedalConfig* cfg);
void MenuHandleBackPress(MenuContext* ctx, PedalConfig* cfg);

// Call from main loop at ~20Hz to render; pass live runtime for STATUS screen
void MenuRender(StutterDisplay&       display,
                const MenuContext*    ctx,
                const PedalConfig*    cfg,
                const StutterRuntime* rt);

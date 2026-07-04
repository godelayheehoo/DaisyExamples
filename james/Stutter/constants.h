#pragma once
#include <cstdint>

// =============================================================================
// AUDIO / BUFFER CONSTANTS
// =============================================================================
constexpr float    SAMPLE_RATE                = 48000.0f;
constexpr uint32_t AUDIO_BLOCK_SIZE           = 4;
constexpr uint32_t STUTTER_BUFFER_MAX_SAMPLES = 100000;

// =============================================================================
// PLAYBACK RATE CONSTANTS
// =============================================================================
constexpr float RATE_STEP = 0.05f; // change per encoder detent
constexpr float RATE_MIN  = 0.25f; // lower bound (~2 octaves down)
constexpr float RATE_MAX  = 4.0f;  // upper bound (~2 octaves up)
constexpr float RATE_INIT = 1.0f;  // always start at unity on boot

// =============================================================================
// TEMPO / BPM CONSTANTS
// =============================================================================
constexpr float BPM_MIN             = 40.0f;
constexpr float BPM_MAX             = 240.0f;
constexpr float BPM_DEFAULT         = 120.0f;
constexpr float BPM_SMOOTHING_COEFF = 0.05f;

// =============================================================================
// MIDI CLOCK CONSTANTS
// =============================================================================
constexpr float    MIDI_CLOCK_DIVISOR = 2500000.0f;
constexpr float    BPM_FILTER_MIN     = 30.0f;
constexpr float    BPM_FILTER_MAX     = 300.0f;
constexpr uint32_t MIDI_CLOCK_MIN_US  = static_cast<uint32_t>(MIDI_CLOCK_DIVISOR / BPM_FILTER_MAX); // 8333 us (~300 BPM)
constexpr uint32_t MIDI_CLOCK_MAX_US  = static_cast<uint32_t>(MIDI_CLOCK_DIVISOR / BPM_FILTER_MIN); // 83333 us (~30 BPM)

// =============================================================================
// PITCH DETECTION CONSTANTS
// =============================================================================
constexpr float PITCH_MIN_FREQ             = 50.0f;
constexpr float PITCH_MAX_FREQ             = 2000.0f;
constexpr float PITCH_CONFIDENCE_THRESHOLD = 0.7f;

// =============================================================================
// UI / DISPLAY CONSTANTS
// =============================================================================
constexpr uint32_t MENU_IDLE_TIMEOUT_MS     = 5000;
constexpr uint32_t DISPLAY_UPDATE_PERIOD_MS = 50;

#define DEBUG_MODE
#define TEST_WEAR_LEVELING 1
#include "daisy_seed.h"
#include "daisysp.h"
#include "pins.h"
#include "menu.h"
#include "wear_leveling_storage.h"

using namespace daisy;
using namespace daisysp;

#include <cmath>

DaisySeed       hw;
MidiUartHandler midi;

// Controls
Encoder menu_encoder;
Encoder rate_encoder;
int32_t menu_enc_accum = 0;
int32_t rate_enc_accum = 0;

// GPIOs for raw debugging of encoder pins
GPIO menu_pin_a;
GPIO menu_pin_b;
GPIO menu_pin_sw;
GPIO menu_pin_bak;
GPIO menu_pin_con;

GPIO rate_pin_a;
GPIO rate_pin_b;
GPIO rate_pin_sw;

// Dedicated menu buttons
Switch button_bak;
Switch button_con;

// GPIOs for the 5-position rotary switch
GPIO rot_switch_pins[5];

// LED indicator
GPIO stutter_led;

// Stutter buffers in external SDRAM
#define STUTTER_BUFFER_MAX_SAMPLES 100000
float DSY_SDRAM_BSS stutter_buf_l[STUTTER_BUFFER_MAX_SAMPLES];
float DSY_SDRAM_BSS stutter_buf_r[STUTTER_BUFFER_MAX_SAMPLES];

// Initialize ADC configuration
AdcChannelConfig adc_config[NUM_ADC_CHANNELS];

// Display
StutterDisplay display;

// Shared state
MenuContext    menu_ctx;
StutterRuntime runtime;

// Persistent Storage
WearLevelingStorage<PedalConfig, 4> storage(hw.qspi);

// ── Pitch Detection (autocorrelation) ─────────────────────────────────────────
// Runs in main loop, NOT in audio callback.
// Operates on the stutter buffer after a capture completes.
static void RunPitchDetection(uint32_t buffer_length)
{
    const float sample_rate = 48000.0f;
    const float min_freq    = 50.0f;
    const float max_freq    = 2000.0f;

    uint32_t min_lag = static_cast<uint32_t>(sample_rate / max_freq); // ~24
    uint32_t max_lag = static_cast<uint32_t>(sample_rate / min_freq); // ~960

    // Clamp max_lag to half the buffer length for meaningful autocorrelation
    if(max_lag > buffer_length / 2)
    {
        max_lag = buffer_length / 2;
    }
    if(min_lag >= max_lag)
    {
        runtime.pitch_valid      = false;
        runtime.pitch_confidence = 0.0f;
        return;
    }

    // Compute energy of the signal (for normalization)
    float energy = 0.0f;
    for(uint32_t i = 0; i < buffer_length; i++)
    {
        energy += stutter_buf_l[i] * stutter_buf_l[i];
    }

    if(energy < 1e-8f)
    {
        // Signal is essentially silent
        runtime.pitch_valid      = false;
        runtime.pitch_confidence = 0.0f;
        return;
    }

    // Compute normalized autocorrelation and find best lag
    float    best_corr = -1.0f;
    uint32_t best_lag  = min_lag;

    for(uint32_t lag = min_lag; lag <= max_lag; lag++)
    {
        float    sum      = 0.0f;
        float    energy_a = 0.0f;
        float    energy_b = 0.0f;
        uint32_t count    = buffer_length - lag;

        for(uint32_t i = 0; i < count; i++)
        {
            sum += stutter_buf_l[i] * stutter_buf_l[i + lag];
            energy_a += stutter_buf_l[i] * stutter_buf_l[i];
            energy_b += stutter_buf_l[i + lag] * stutter_buf_l[i + lag];
        }

        float norm = sqrtf(energy_a * energy_b);
        float corr = (norm > 1e-8f) ? (sum / norm) : 0.0f;

        if(corr > best_corr)
        {
            best_corr = corr;
            best_lag  = lag;
        }
    }

    runtime.pitch_confidence = best_corr;
    runtime.pitch_valid      = (best_corr > 0.7f);

    if(runtime.pitch_valid)
    {
        float freq = sample_rate / static_cast<float>(best_lag);

        // Quantize to nearest MIDI note
        float midi_note   = 69.0f + 12.0f * log2f(freq / 440.0f);
        float midi_note_q = roundf(midi_note);
        float target_freq = 440.0f * powf(2.0f, (midi_note_q - 69.0f) / 12.0f);

        runtime.detected_pitch_hz = target_freq;
    }
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    static uint32_t write_pos              = 0;
    static float    read_pos_accum         = 0.0f;
    static uint32_t active_buffer_length   = 48000;
    static bool     trigger_pending        = false;
    static uint32_t last_audio_clock_ticks = 0;

    for(size_t i = 0; i < size; i++)
    {
        float in_l = in[0][i];
        float in_r = in[1][i];

        float out_l = in_l;
        float out_r = in_r;

        // Local copies of runtime states (volatile)
        StutterState current_state = runtime.state;
        bool         trigger       = runtime.trigger_active;
        float        target_rate   = runtime.target_rate;
        float        wet           = runtime.wet;
        bool         bypassed      = runtime.bypassed;

        // Smooth rate towards target at all times to keep runtime.rate updated
        runtime.rate += 0.001f * (target_rate - runtime.rate);
        float current_rate = runtime.rate;

        if(bypassed)
        {
            out_l = in_l;
            out_r = in_r;
        }
        // Handle State Transitions
        else if(current_state == STUTTER_IDLE)
        {
            if(trigger_pending)
            {
                if(!trigger)
                {
                    trigger_pending = false;
                }
                else
                {
                    uint32_t prev_ticks    = last_audio_clock_ticks;
                    uint32_t curr_ticks    = runtime.midi_clock_ticks;
                    last_audio_clock_ticks = curr_ticks;

                    uint32_t subdiv_ticks
                        = 24; // Default to 1/4 note (24 ticks)
                    switch(runtime.subdiv_pos)
                    {
                        case 0: subdiv_ticks = 3; break;  // 1/32
                        case 1: subdiv_ticks = 6; break;  // 1/16
                        case 2: subdiv_ticks = 12; break; // 1/8
                        case 3: subdiv_ticks = 24; break; // 1/4
                        case 4: subdiv_ticks = 48; break; // 1/2
                    }

                    if(curr_ticks != prev_ticks)
                    {
                        // Trigger on tick crossing/modulo of subdiv boundary
                        if((prev_ticks / subdiv_ticks)
                               != (curr_ticks / subdiv_ticks)
                           || (curr_ticks % subdiv_ticks == 0))
                        {
                            trigger_pending      = false;
                            current_state        = STUTTER_RECORDING;
                            write_pos            = 0;
                            active_buffer_length = runtime.buffer_length;
                            runtime.state        = STUTTER_RECORDING;
                        }
                    }
                }
            }
            else if(trigger)
            {
                // Trigger transition: check if we should quantize
                if(runtime.quantize_trigger && runtime.midi_play_seen)
                {
                    trigger_pending        = true;
                    last_audio_clock_ticks = runtime.midi_clock_ticks;
                }
                else
                {
                    // Transition immediately
                    current_state        = STUTTER_RECORDING;
                    write_pos            = 0;
                    active_buffer_length = runtime.buffer_length;
                    runtime.state        = STUTTER_RECORDING;
                }
            }
        }
        else if(current_state == STUTTER_RECORDING)
        {
            if(!trigger)
            {
                // Aborted before recording completed
                current_state = STUTTER_IDLE;
                runtime.state = STUTTER_IDLE;
            }
        }
        else if(current_state == STUTTER_PLAYING)
        {
            if(!trigger)
            {
                // Released stutter button
                current_state = STUTTER_IDLE;
                runtime.state = STUTTER_IDLE;
            }
        }

        // Process current state
        if(current_state == STUTTER_IDLE)
        {
            out_l = in_l;
            out_r = in_r;
        }
        else if(current_state == STUTTER_RECORDING)
        {
            // Record current input into buffer
            if(write_pos < STUTTER_BUFFER_MAX_SAMPLES)
            {
                stutter_buf_l[write_pos] = in_l;
                stutter_buf_r[write_pos] = in_r;
            }
            write_pos++;

            // If recording buffer is full, transition to PLAYING
            if(write_pos >= active_buffer_length)
            {
                current_state  = STUTTER_PLAYING;
                runtime.state  = STUTTER_PLAYING;
                read_pos_accum = 0.0f;

                // Compute loop frequency for LFQ/PTQ modes
                runtime.loop_frequency_hz
                    = 48000.0f / static_cast<float>(active_buffer_length);

                // Request async pitch detection for PTQ mode
                if(runtime.playback_rate_mode == PRM_PTQ)
                {
                    runtime.pitch_detection_pending = true;
                }

                // Reset semitone offset on new capture
                runtime.semitone_offset = 0;
            }

            // Output dry signal during recording phase
            out_l = in_l;
            out_r = in_r;
        }
        else if(current_state == STUTTER_PLAYING)
        {
            // Fixed-point interpolation
            uint32_t idx  = static_cast<uint32_t>(read_pos_accum);
            float    frac = read_pos_accum - idx;
            uint32_t idx1 = idx + 1;

            if(idx >= active_buffer_length)
            {
                idx = 0;
            }
            if(idx1 >= active_buffer_length)
            {
                idx1 = 0;
            }

            float play_l = stutter_buf_l[idx]
                           + frac * (stutter_buf_l[idx1] - stutter_buf_l[idx]);
            float play_r = stutter_buf_r[idx]
                           + frac * (stutter_buf_r[idx1] - stutter_buf_r[idx]);

            // Advance playback position
            read_pos_accum += current_rate;
            if(read_pos_accum >= active_buffer_length)
            {
                read_pos_accum -= active_buffer_length;
                if(read_pos_accum >= active_buffer_length)
                {
                    read_pos_accum = 0.0f;
                }
            }

            // Apply wet/dry mix
            out_l = (in_l * (1.0f - wet)) + (play_l * wet);
            out_r = (in_r * (1.0f - wet)) + (play_r * wet);
        }

        out[0][i] = out_l;
        out[1][i] = out_r;
    }
}

void InitControls()
{
    // Initialize encoders
    menu_encoder.Init(StutterPins::MENU_ENC_A,
                      StutterPins::MENU_ENC_B,
                      StutterPins::MENU_ENC_SW);
    rate_encoder.Init(StutterPins::RATE_ENC_A,
                      StutterPins::RATE_ENC_B,
                      StutterPins::RATE_ENC_SW);

    // Initialize raw pins for debugging status (Input with Pull-up)
    menu_pin_a.Init(
        StutterPins::MENU_ENC_A, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    menu_pin_b.Init(
        StutterPins::MENU_ENC_B, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    menu_pin_sw.Init(
        StutterPins::MENU_ENC_SW, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    menu_pin_bak.Init(
        StutterPins::MENU_BAK, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    menu_pin_con.Init(
        StutterPins::MENU_CON, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    rate_pin_a.Init(
        StutterPins::RATE_ENC_A, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    rate_pin_b.Init(
        StutterPins::RATE_ENC_B, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    rate_pin_sw.Init(
        StutterPins::RATE_ENC_SW, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

    // Initialize dedicated menu switches (momentary active low with pull-up)
    button_bak.Init(StutterPins::MENU_BAK, 1000.0f);
    button_con.Init(StutterPins::MENU_CON, 1000.0f);

    // Initialize 5-position rotary switch (Input with Pull-up)
    Pin rot_pins[5] = {StutterPins::ROT_POS_1,
                       StutterPins::ROT_POS_2,
                       StutterPins::ROT_POS_3,
                       StutterPins::ROT_POS_4,
                       StutterPins::ROT_POS_5};
    for(int i = 0; i < 5; i++)
    {
        rot_switch_pins[i].Init(
            rot_pins[i], GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    }

    // Initialize ADC for the Wet/Dry pot
    adc_config[ADC_WET_DRY_POT].InitSingle(StutterPins::WET_DRY_POT);
    hw.adc.Init(adc_config, NUM_ADC_CHANNELS);

    // Initialize stutter indicator LED
    stutter_led.Init(StutterPins::STUTTER_LED, GPIO::Mode::OUTPUT);
}

void InitOled()
{
    StutterDisplay::Config display_cfg;
    display_cfg.driver_config.transport_config.i2c_config.pin_config.scl
        = StutterPins::OLED_SCL;
    display_cfg.driver_config.transport_config.i2c_config.pin_config.sda
        = StutterPins::OLED_SDA;
    display.Init(display_cfg);
}

#ifdef TEST_WEAR_LEVELING
static void TestWearLeveling()
{
    hw.PrintLine("--- WEAR LEVELING SELF TEST START ---");

    // Use a different sector offset, e.g., 1MB in (0x100000)
    uint32_t test_offset = 0x100000;

    struct TestConfig
    {
        uint32_t    a;
        uint32_t    b;
        inline bool operator==(const TestConfig& other) const
        {
            return a == other.a && b == other.b;
        }
        inline bool operator!=(const TestConfig& other) const
        {
            return !(*this == other);
        }
    };

    WearLevelingStorage<TestConfig, 4> test_storage(hw.qspi);

    hw.PrintLine("Erasing test sectors...");
    for(size_t i = 0; i < 4; ++i)
    {
        hw.qspi.EraseSector(test_offset + i * 4096);
    }

    TestConfig defaults = {42, 100};
    hw.PrintLine("Initializing test storage...");
    test_storage.Init(defaults, test_offset);

    TestConfig loaded = test_storage.GetSettings();
    if(loaded.a == 42 && loaded.b == 100)
    {
        hw.PrintLine("Init clean state: SUCCESS");
    }
    else
    {
        hw.PrintLine("Init clean state: FAILED! Loaded a=%d, b=%d",
                     (int)loaded.a,
                     (int)loaded.b);
    }

    loaded.a                   = 99;
    test_storage.GetSettings() = loaded;
    hw.PrintLine("Saving modification 1 (a=99)...");
    test_storage.Save();

    hw.PrintLine("Re-initializing test storage to simulate reboot...");
    test_storage.Init(defaults, test_offset);
    loaded = test_storage.GetSettings();
    if(loaded.a == 99 && loaded.b == 100)
    {
        hw.PrintLine("Re-init check: SUCCESS");
    }
    else
    {
        hw.PrintLine("Re-init check: FAILED! Loaded a=%d, b=%d",
                     (int)loaded.a,
                     (int)loaded.b);
    }

    hw.PrintLine(
        "Writing 300 sequential configurations to verify ring buffer...");
    for(int i = 0; i < 300; ++i)
    {
        loaded.a                   = 200 + i;
        test_storage.GetSettings() = loaded;
        test_storage.Save();
    }

    hw.PrintLine("Re-initializing test storage after 300 writes...");
    test_storage.Init(defaults, test_offset);
    loaded = test_storage.GetSettings();
    if(loaded.a == 200 + 299 && loaded.b == 100)
    {
        hw.PrintLine("Post-wrap check: SUCCESS (retrieved latest config a=%d)",
                     (int)loaded.a);
    }
    else
    {
        hw.PrintLine("Post-wrap check: FAILED! Loaded a=%d, b=%d",
                     (int)loaded.a,
                     (int)loaded.b);
    }

    hw.PrintLine("--- WEAR LEVELING SELF TEST COMPLETE ---");
}
#endif

int main(void)
{
    // Initialize Seed hardware
    hw.Init();

    // Check if BACK and CON are both held at boot to enter bootloader mode
    GPIO boot_bak, boot_con;
    boot_bak.Init(StutterPins::MENU_BAK, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    boot_con.Init(StutterPins::MENU_CON, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    hw.DelayMs(10);

    if(!boot_bak.Read() && !boot_con.Read())
    {
        // Flash built-in LED 5 times as visual confirmation
        for(int i = 0; i < 5; i++)
        {
            hw.SetLed(true);
            hw.DelayMs(100);
            hw.SetLed(false);
            hw.DelayMs(100);
        }
        daisy::System::ResetToBootloader();
    }

    // Load Configuration from QSPI flash
    PedalConfig default_config;
    default_config.quantize_trigger   = false;
    default_config.midi_sync_enabled  = 0;
    default_config.playback_rate_mode = PRM_OFF;
    default_config.midi_channel       = 1; // Default to MIDI Channel 1

    storage.Init(default_config);
    PedalConfig& config = storage.GetSettings();

    // Initialize peripherals and UI
    InitControls();
    InitOled();
    MenuInit(&menu_ctx);

    // Initialize MIDI input
    MidiUartHandler::Config midi_config;
    midi_config.transport_config.periph
        = UartHandler::Config::Peripheral::UART_5;
    midi_config.transport_config.rx = StutterPins::MIDI_RX;
    midi_config.transport_config.tx = Pin(); // input only
    midi.Init(midi_config);
    midi.StartReceive();

    // Initialize Stutter runtime states
    runtime.state                   = STUTTER_IDLE;
    runtime.rate                    = 1.0f;
    runtime.target_rate             = 1.0f;
    runtime.wet                     = 0.5f;
    runtime.buffer_length           = 48000;
    runtime.trigger_active          = false;
    runtime.bpm                     = 120.0f;
    runtime.has_clock               = false;
    runtime.subdiv_pos              = 2; // Default to 1/8 note
    runtime.midi_event_count        = 0;
    runtime.midi_play_seen          = false;
    runtime.midi_clock_ticks        = 0;
    runtime.quantize_trigger        = config.quantize_trigger;
    runtime.playback_rate_mode      = config.playback_rate_mode;
    runtime.loop_frequency_hz       = 0.0f;
    runtime.detected_pitch_hz       = 0.0f;
    runtime.pitch_confidence        = 0.0f;
    runtime.pitch_valid             = false;
    runtime.pitch_detection_pending = false;
    runtime.semitone_offset         = 0;
    runtime.bypassed                = false;

    // Start peripherals
    hw.adc.Start();

    // Start Audio
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAudio(AudioCallback);

#ifdef DEBUG_MODE
    hw.StartLog(
        false); // Start logging over USB serial without waiting for connection
    uint32_t last_print = System::GetNow();
#ifdef TEST_WEAR_LEVELING
    TestWearLeveling();
#endif
#endif

    uint32_t last_tick_time  = System::GetNow();
    uint32_t last_display_ms = System::GetNow();

    // MIDI Timing and Tracking Variables
    uint32_t last_clock_us      = 0;
    uint32_t last_clock_recv_ms = 0;
    float    bpm_smoothed       = 120.0f;
    bool     first_clock        = true;
    // bool     prev_has_clock     = false;

    while(1)
    {
        // Compute delta time in milliseconds
        uint32_t now_ms     = System::GetNow();
        uint32_t elapsed_ms = now_ms - last_tick_time;
        last_tick_time      = now_ms;

        // Poll controls
        menu_encoder.Debounce();
        rate_encoder.Debounce();
        button_bak.Debounce();
        button_con.Debounce();

        menu_enc_accum += menu_encoder.Increment();
        rate_enc_accum += rate_encoder.Increment();

        // Handle rate encoder rotation based on playback rate mode
        int32_t rate_inc = rate_encoder.Increment();
        if(rate_inc != 0)
        {
            uint8_t mode = runtime.playback_rate_mode;
            if(mode == PRM_OFF)
            {
                // Continuous mode: direct rate adjustment
                runtime.target_rate += rate_inc * RATE_STEP;
                runtime.target_rate
                    = fclamp(runtime.target_rate, RATE_MIN, RATE_MAX);
            }
            else
            {
                // Quantized modes (LFQ or PTQ): adjust semitone offset
                runtime.semitone_offset += rate_inc;
                if(runtime.semitone_offset < -24)
                    runtime.semitone_offset = -24;
                if(runtime.semitone_offset > 24)
                    runtime.semitone_offset = 24;

                float rate_from_semitones
                    = powf(2.0f, runtime.semitone_offset / 12.0f);

                if(mode == PRM_PTQ && runtime.pitch_valid
                   && runtime.detected_pitch_hz > 0.0f
                   && runtime.loop_frequency_hz > 0.0f)
                {
                    // PTQ: shift relative to detected pitch
                    float target_freq
                        = runtime.detected_pitch_hz * rate_from_semitones;
                    runtime.target_rate
                        = target_freq / runtime.loop_frequency_hz;
                }
                else
                {
                    // LFQ (or PTQ fallback): shift relative to loop frequency
                    runtime.target_rate = rate_from_semitones;
                }

                // Clamp to safe range
                runtime.target_rate
                    = fclamp(runtime.target_rate, RATE_MIN, RATE_MAX);
            }
        }

        // Handle rate encoder switch as momentary stutter trigger
        runtime.trigger_active = rate_encoder.Pressed();

        // Process MIDI events
        midi.Listen();
        while(midi.HasEvents())
        {
            MidiEvent msg = midi.PopEvent();
            runtime.midi_event_count++;

            // Channel filter for non-SystemRealTime messages
            if(msg.type != SystemRealTime)
            {
                if(config.midi_channel == 0)
                {
                    continue; // MIDI input is disabled (OFF)
                }
                if(msg.channel != (config.midi_channel - 1))
                {
                    continue; // Message is for another MIDI channel
                }
            }

            if(msg.type == SystemRealTime)
            {
                if(msg.srt_type == TimingClock)
                {
                    runtime.midi_clock_ticks++;
                    uint32_t now_us = System::GetUs();
                    if(last_clock_us != 0)
                    {
                        uint32_t elapsed_us = now_us - last_clock_us;
                        // Outlier filter: accept only between 30 and 300 BPM (8,333us to 83,333us)
                        if(elapsed_us >= 8333 && elapsed_us <= 83333)
                        {
                            float raw_bpm = 2500000.0f / elapsed_us;
                            if(first_clock)
                            {
                                bpm_smoothed = raw_bpm;
                                first_clock  = false;
                            }
                            else
                            {
                                bpm_smoothed
                                    += 0.05f * (raw_bpm - bpm_smoothed);
                            }
                            runtime.has_clock  = true;
                            last_clock_recv_ms = now_ms;
                        }
                    }
                    else
                    {
                        first_clock = true;
                    }
                    last_clock_us = now_us;
                }
                else if(msg.srt_type == Start)
                {
                    runtime.midi_play_seen   = true;
                    runtime.midi_clock_ticks = 0;
                    first_clock              = true;
                }
                else if(msg.srt_type == Continue)
                {
                    runtime.midi_play_seen = true;
                    first_clock            = true;
                }
                else if(msg.srt_type == Stop)
                {
                    runtime.midi_play_seen = false;
                }
            }
            else if(msg.type == ControlChange)
            {
                ControlChangeEvent cc   = msg.AsControlChange();
                uint8_t            ctrl = cc.control_number;
                uint8_t            val  = cc.value;

                switch(ctrl)
                {
                    case 20: // Stutter Trigger (Momentary)
                        runtime.trigger_active = (val >= 64);
                        break;
                    case 21: // Stutter Toggle (Latching)
                        runtime.trigger_active = (val >= 64);
                        break;
                    case 22: // Playback Rate (Continuous)
                    {
                        float cc_val = static_cast<float>(val);
                        if(val <= 64)
                        {
                            runtime.target_rate
                                = RATE_MIN
                                  + (cc_val / 64.0f) * (1.0f - RATE_MIN);
                        }
                        else
                        {
                            runtime.target_rate = 1.0f
                                                  + ((cc_val - 64.0f) / 63.0f)
                                                        * (RATE_MAX - 1.0f);
                        }
                    }
                    break;
                    case 23: // Quantized Semitone Offset
                    {
                        float norm = (static_cast<float>(val) - 64.0f) / 63.0f;
                        int semitones = static_cast<int>(roundf(norm * 24.0f));
                        if(semitones < -24)
                            semitones = -24;
                        if(semitones > 24)
                            semitones = 24;
                        runtime.semitone_offset = semitones;

                        // Recalculate target rate if quantized mode is active
                        uint8_t mode = runtime.playback_rate_mode;
                        if(mode != PRM_OFF)
                        {
                            float rate_from_semitones
                                = powf(2.0f, runtime.semitone_offset / 12.0f);
                            if(mode == PRM_PTQ && runtime.pitch_valid
                               && runtime.detected_pitch_hz > 0.0f
                               && runtime.loop_frequency_hz > 0.0f)
                            {
                                float target_freq = runtime.detected_pitch_hz
                                                    * rate_from_semitones;
                                runtime.target_rate
                                    = target_freq / runtime.loop_frequency_hz;
                            }
                            else
                            {
                                runtime.target_rate = rate_from_semitones;
                            }
                            runtime.target_rate = fclamp(
                                runtime.target_rate, RATE_MIN, RATE_MAX);
                        }
                    }
                    break;
                    case 24: // Wet/Dry Mix
                        runtime.wet = val / 127.0f;
                        break;
                    case 25: // Loop Length subdivision
                        if(val <= 25)
                            runtime.subdiv_pos = 0;
                        else if(val <= 50)
                            runtime.subdiv_pos = 1;
                        else if(val <= 76)
                            runtime.subdiv_pos = 2;
                        else if(val <= 102)
                            runtime.subdiv_pos = 3;
                        else
                            runtime.subdiv_pos = 4;
                        break;
                    case 26: // MIDI Sync Enable
                        config.midi_sync_enabled = (val >= 64) ? 1 : 0;
                        menu_ctx.needs_redraw    = true;
                        menu_ctx.dirty           = true;
                        break;
                    case 27: // Quantize Trigger Enable
                        config.quantize_trigger  = (val >= 64);
                        runtime.quantize_trigger = config.quantize_trigger;
                        menu_ctx.needs_redraw    = true;
                        menu_ctx.dirty           = true;
                        break;
                    case 28: // Playback Rate Mode
                        if(val <= 42)
                            config.playback_rate_mode = PRM_OFF;
                        else if(val <= 85)
                            config.playback_rate_mode = PRM_LFQ;
                        else
                            config.playback_rate_mode = PRM_PTQ;
                        runtime.playback_rate_mode = config.playback_rate_mode;
                        menu_ctx.needs_redraw      = true;
                        menu_ctx.dirty             = true;
                        break;
                    case 29: // Manual BPM (Tempo)
                        runtime.bpm = 40.0f + (val / 127.0f) * 200.0f;
                        break;
                    case 30: // Clear Loop / Reset
                        if(val >= 64)
                        {
                            runtime.trigger_active = false;
                            runtime.state          = STUTTER_IDLE;
                        }
                        break;
                    case 31:         // Bypass / Active
                        if(val < 64) // Bypass
                        {
                            runtime.bypassed       = true;
                            runtime.trigger_active = false;
                            runtime.state          = STUTTER_IDLE;
                        }
                        else // Active
                        {
                            runtime.bypassed = false;
                        }
                        break;
                }
            }
            else if(msg.type == NoteOn || msg.type == NoteOff)
            {
                NoteOnEvent note_event = msg.AsNoteOn();
                uint8_t     velocity   = note_event.velocity;
                bool        active     = (msg.type == NoteOn && velocity > 0);

                if(note_event.note == 60) // C4: Momentary Trigger
                {
                    runtime.trigger_active = active;
                }
                else if(note_event.note == 62 && active) // D4: Latching Toggle
                {
                    runtime.trigger_active = !runtime.trigger_active;
                }
                else if(note_event.note == 64 && active) // E4: Clear Loop
                {
                    runtime.trigger_active = false;
                    runtime.state          = STUTTER_IDLE;
                }
            }
        }

        // MIDI clock timeout check (2 seconds)
        if(runtime.has_clock && (now_ms - last_clock_recv_ms > 2000))
        {
            runtime.has_clock      = false;
            first_clock            = true;
            last_clock_us          = 0;
            runtime.midi_play_seen = false;
        }

        // Print log when MIDI clock status transitions
        /*
        if(runtime.has_clock && !prev_has_clock)
        {
            hw.PrintLine("MIDI Clock Active. BPM: %d", (int)bpm_smoothed);
        }
        else if(!runtime.has_clock && prev_has_clock)
        {
            hw.PrintLine("MIDI Clock Timeout. Reverting to internal BPM.");
        }
        */
        // prev_has_clock = runtime.has_clock;

        // Set active BPM based on configuration and clock availability
        if(config.midi_sync_enabled && runtime.has_clock)
        {
            runtime.bpm = bpm_smoothed;
        }
        else
        {
            runtime.bpm = 120.0f;
        }

        // Get pot float value (0.0 to 1.0), inverted so CW increases value
        float wet_dry_val = 1.0f - hw.adc.GetFloat(ADC_WET_DRY_POT);
        int   pot_whole   = static_cast<int>(wet_dry_val);
        int   pot_frac = static_cast<int>((wet_dry_val - pot_whole) * 100.0f);
        if(pot_frac < 0)
            pot_frac = -pot_frac;

        // Update runtime pot value
        runtime.wet = wet_dry_val;

        // Get rotary switch position (0-4, or -1 if in transition)
        int rot_pos = -1;
        for(int i = 0; i < 5; i++)
        {
            if(!rot_switch_pins[i].Read())
            {
                rot_pos = i;
                break;
            }
        }

        // Update runtime rotary subdivision position
        if(rot_pos >= 0 && rot_pos <= 4)
        {
            runtime.subdiv_pos = rot_pos;
        }

        // Calculate dynamic buffer length based on BPM and subdivision position
        const float SUBDIV_MULTIPLIERS[5] = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f};
        float       beat_duration_sec     = 60.0f / runtime.bpm;
        float       subdiv_duration_sec
            = beat_duration_sec * SUBDIV_MULTIPLIERS[runtime.subdiv_pos];
        uint32_t calc_length = (uint32_t)(subdiv_duration_sec * 48000.0f);
        if(calc_length > STUTTER_BUFFER_MAX_SAMPLES)
        {
            calc_length = STUTTER_BUFFER_MAX_SAMPLES;
        }
        else if(calc_length < 1)
        {
            calc_length = 1;
        }
        runtime.buffer_length = calc_length;

        // Drive stutter indicator LED
        stutter_led.Write(runtime.state != STUTTER_IDLE);

        // Tick menu idle timers
        MenuTick(&menu_ctx, &config, elapsed_ms);

        // Dispatch Menu Encoder rotation events
        int32_t menu_inc = menu_encoder.Increment();
        if(menu_inc != 0)
        {
            MenuHandleRotate(&menu_ctx, &config, menu_inc);
        }

        // Dispatch Menu Encoder press events
        if(menu_encoder.FallingEdge())
        {
            MenuHandleShortPress(&menu_ctx, &config);
        }

        // Handle dedicated menu buttons
        if(button_bak.FallingEdge())
        {
            MenuHandleBackPress(&menu_ctx, &config);
        }
        if(button_con.FallingEdge())
        {
            MenuHandleShortPress(&menu_ctx, &config);
        }

        // Handle dirty configuration storage saves
        if(menu_ctx.dirty)
        {
            storage.Save();
            menu_ctx.dirty             = false;
            runtime.quantize_trigger   = config.quantize_trigger;
            runtime.playback_rate_mode = config.playback_rate_mode;
        }

        // Run pitch detection asynchronously in main loop (not audio callback)
        if(runtime.pitch_detection_pending)
        {
            runtime.pitch_detection_pending = false;
            RunPitchDetection(runtime.buffer_length);

            // If PTQ and pitch was detected, recompute target rate at current
            // semitone offset now that we have a valid pitch reference
            if(runtime.playback_rate_mode == PRM_PTQ && runtime.pitch_valid
               && runtime.detected_pitch_hz > 0.0f
               && runtime.loop_frequency_hz > 0.0f)
            {
                float rate_from_semitones
                    = powf(2.0f, runtime.semitone_offset / 12.0f);
                float target_freq
                    = runtime.detected_pitch_hz * rate_from_semitones;
                runtime.target_rate = target_freq / runtime.loop_frequency_hz;
                runtime.target_rate
                    = fclamp(runtime.target_rate, RATE_MIN, RATE_MAX);
            }
        }

        // Render OLED screen at 20 Hz
        if(now_ms - last_display_ms >= 50)
        {
            MenuRender(display, &menu_ctx, &config, &runtime);
            last_display_ms = now_ms;
        }

#ifdef DEBUG_MODE
        if(now_ms - last_print >= 250) // Print every 250ms
        {
            last_print = now_ms;
            hw.PrintLine(
                "POT: %d.%02d | ROT: %d | RATE [ACC:%d] | MIDI [SYNC:%d CLK:%d "
                "BPM:%d.%d]",
                pot_whole,
                pot_frac,
                rot_pos,
                static_cast<int>(rate_enc_accum),
                config.midi_sync_enabled,
                runtime.has_clock,
                (int)runtime.bpm,
                (int)((runtime.bpm - (int)runtime.bpm) * 10.0f));
        }
#endif

        System::Delay(1);
    }
}

#define DEBUG_MODE
#include "daisy_seed.h"
#include "daisysp.h"
#include "pins.h"
#include "menu.h"

using namespace daisy;
using namespace daisysp;

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
PersistentStorage<PedalConfig> storage(hw.qspi);

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    static uint32_t write_pos            = 0;
    static float    read_pos_accum       = 0.0f;
    static uint32_t active_buffer_length = 48000;

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

        // Smooth rate towards target at all times to keep runtime.rate updated
        runtime.rate += 0.001f * (target_rate - runtime.rate);
        float current_rate = runtime.rate;

        // Handle State Transitions
        if(current_state == STUTTER_IDLE)
        {
            if(trigger)
            {
                // Transition: IDLE -> RECORDING
                current_state        = STUTTER_RECORDING;
                write_pos            = 0;
                active_buffer_length = runtime.buffer_length;
                runtime.state        = STUTTER_RECORDING;
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

int main(void)
{
    // Initialize Seed hardware
    hw.Init();

    // Load Configuration from QSPI flash
    PedalConfig default_config;
    default_config.quantize_trigger  = false;
    default_config.midi_sync_enabled = 0;

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
    runtime.state          = STUTTER_IDLE;
    runtime.rate           = 1.0f;
    runtime.target_rate    = 1.0f;
    runtime.wet            = 0.5f;
    runtime.buffer_length  = 48000;
    runtime.trigger_active = false;
    runtime.bpm            = 120.0f;
    runtime.has_clock      = false;
    runtime.subdiv_pos     = 2; // Default to 1/8 note

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
#endif

    uint32_t last_tick_time         = System::GetNow();
    uint32_t last_display_ms        = System::GetNow();
    bool     menu_button_held_fired = false;

    // MIDI Timing and Tracking Variables
    uint32_t last_clock_us      = 0;
    uint32_t last_clock_recv_ms = 0;
    float    bpm_smoothed       = 120.0f;
    bool     first_clock        = true;
    bool     prev_has_clock     = false;

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

        // Handle rate encoder rotation to adjust target playback rate
        int32_t rate_inc = rate_encoder.Increment();
        if(rate_inc != 0)
        {
            runtime.target_rate += rate_inc * RATE_STEP;
            runtime.target_rate
                = fclamp(runtime.target_rate, RATE_MIN, RATE_MAX);
        }

        // Handle rate encoder switch as momentary stutter trigger
        runtime.trigger_active = rate_encoder.Pressed();

        // Process MIDI events
        midi.Listen();
        while(midi.HasEvents())
        {
            MidiEvent msg = midi.PopEvent();
            if(msg.type == SystemRealTime)
            {
                if(msg.srt_type == TimingClock)
                {
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
                else if(msg.srt_type == Start || msg.srt_type == Continue)
                {
                    first_clock = true;
                }
            }
        }

        // MIDI clock timeout check (2 seconds)
        if(runtime.has_clock && (now_ms - last_clock_recv_ms > 2000))
        {
            runtime.has_clock = false;
            first_clock       = true;
            last_clock_us     = 0;
        }

        // Print log when MIDI clock status transitions
        if(runtime.has_clock && !prev_has_clock)
        {
            hw.PrintLine("MIDI Clock Active. BPM: %d", (int)bpm_smoothed);
        }
        else if(!runtime.has_clock && prev_has_clock)
        {
            hw.PrintLine("MIDI Clock Timeout. Reverting to internal BPM.");
        }
        prev_has_clock = runtime.has_clock;

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

        // Dispatch Menu Encoder press events (Short vs Long press detection)
        if(menu_encoder.Pressed())
        {
            if(!menu_button_held_fired && menu_encoder.TimeHeldMs() >= 500.0f)
            {
                MenuHandleLongPress(&menu_ctx, &config);
                menu_button_held_fired = true;
            }
        }
        else
        {
            if(menu_encoder.FallingEdge())
            {
                if(!menu_button_held_fired)
                {
                    MenuHandleShortPress(&menu_ctx, &config);
                }
            }
            menu_button_held_fired = false;
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
            menu_ctx.dirty = false;
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

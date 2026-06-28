#define DEBUG_MODE
#include "daisy_seed.h"
#include "daisysp.h"
#include "pins.h"
#include "menu.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

// Controls
Encoder menu_encoder;
Encoder rate_encoder;
int32_t menu_enc_accum = 0;
int32_t rate_enc_accum = 0;

// GPIOs for raw debugging of encoder pins
GPIO menu_pin_a;
GPIO menu_pin_b;
GPIO menu_pin_sw;

GPIO rate_pin_a;
GPIO rate_pin_b;
GPIO rate_pin_sw;

// GPIOs for the 5-position rotary switch
GPIO rot_switch_pins[5];

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
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
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

    rate_pin_a.Init(
        StutterPins::RATE_ENC_A, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    rate_pin_b.Init(
        StutterPins::RATE_ENC_B, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
    rate_pin_sw.Init(
        StutterPins::RATE_ENC_SW, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);

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

    while(1)
    {
        // Compute delta time in milliseconds
        uint32_t now_ms     = System::GetNow();
        uint32_t elapsed_ms = now_ms - last_tick_time;
        last_tick_time      = now_ms;

        // Poll controls
        menu_encoder.Debounce();
        rate_encoder.Debounce();

        menu_enc_accum += menu_encoder.Increment();
        rate_enc_accum += rate_encoder.Increment();

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
                "POT: %d.%02d | ROT: %d | MENU [A:%d B:%d SW:%d ACC:%d] | RATE "
                "[A:%d B:%d SW:%d ACC:%d]",
                pot_whole,
                pot_frac,
                rot_pos,
                menu_pin_a.Read(),
                menu_pin_b.Read(),
                menu_pin_sw.Read(),
                static_cast<int>(menu_enc_accum),
                rate_pin_a.Read(),
                rate_pin_b.Read(),
                rate_pin_sw.Read(),
                static_cast<int>(rate_enc_accum));
        }
#endif

        System::Delay(1);
    }
}

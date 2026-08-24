#include "daisy_seed.h"
#include "daisysp.h"
#include "SidechainCompressor.h"
#include "constants.h"
#include "pins.h"
#include "ramp_control.h"

using namespace daisy;

// Uncomment the line below to enable serial logging
// #define DEBUG_LOG 1

// Uncomment to disable pots/switches and use fixed values for SC input testing
// #define DISABLE_POTS 1

// ─────────────────────────────────────────────────────────────────────────────
// Hardcoded parameter values — swap these for pot readings once wired up.
// Set for dramatic test behaviour: instant clamp to silence on sidechain hit.
// ─────────────────────────────────────────────────────────────────────────────

// Saturation mode state

SidechainCompressor::SatMode current_sat_mode
    = SidechainCompressor::SatMode::kSoft;

enum class FilterMode
{
    kLPF,
    kBPF,
    kHPF
};
volatile FilterMode current_filter_mode = FilterMode::kLPF;
volatile bool       effect_engaged      = true;

volatile float current_threshold  = -20.0f;
volatile float debug_sc_pre_peak  = 0.0f;
volatile float debug_sc_post_peak = 0.0f;

// LED fires when envelope follower exceeds this — tune if needed

// ─────────────────────────────────────────────────────────────────────────────
// Pin assignments
//   Sidechain input : A10 / D25  (board pin 32, via TL072 conditioning circuit)
//   Main stereo in  : Audio In  L/R  (built-in codec)
//   Main stereo out : Audio Out L/R  (built-in codec)
//   LED             : D30
//   Bootloader entry: D8 (board pin 9, pull LOW at boot to enter bootloader)
// ─────────────────────────────────────────────────────────────────────────────

DaisySeed           hw;
SidechainCompressor comp;
dsy_gpio            led;
dsy_gpio            footswitch;
dsy_gpio            led_footswitch;
dsy_gpio            unused_button;
dsy_gpio            unused_button_led;
dsy_gpio            clear_sw;
dsy_gpio            sw_sat1, sw_sat2;
dsy_gpio            filter_sw1, filter_sw2;

daisysp::Svf sc_filter;

enum AdcChannel
{
    kScInput = 0,
    kFilterCutoffPot,
    kThresholdPot,
    kRatioPot,
    kAttackPot,
    kReleasePot,
    kMixPot,
    kDrivePot,
    kWidthPot,
    kOutputPot,
    kUnusedPot,
    kNumAdcChannels
};

// State variables
static float sample_rate = 48000.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Ramp / LFO state
// ─────────────────────────────────────────────────────────────────────────────

RampState ramp_state[kNumRampableControls];

LearnPhase learn_phase  = LearnPhase::kIdle;
int        learn_target = -1;
float      learn_snapshot[kNumRampableControls];
float      learn_ramp_pot_snapshot = 0.0f;
float      learn_committed_high    = 0.0f;


// Non-blocking triple-flash state
static int      flash_remaining      = 0;
static bool     flash_led_on         = false;
static uint32_t flash_next_toggle_ms = 0;

void TriggerTripleFlash()
{
    flash_remaining      = RAMP_FLASH_COUNT * 2; // on+off half-cycles
    flash_next_toggle_ms = daisy::System::GetNow();
}

void UpdateFlash(uint32_t now_ms)
{
    if(flash_remaining <= 0)
        return;
    if(now_ms >= flash_next_toggle_ms)
    {
        flash_led_on = !flash_led_on;
        dsy_gpio_write(&unused_button_led, flash_led_on ? 1 : 0);
        flash_next_toggle_ms
            = now_ms + (flash_led_on ? RAMP_FLASH_ON_MS : RAMP_FLASH_OFF_MS);
        flash_remaining--;
        if(flash_remaining == 0)
            dsy_gpio_write(&unused_button_led, 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio callback
// ─────────────────────────────────────────────────────────────────────────────

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Bypass: pass through audio unchanged when effect is disengaged
    if(!effect_engaged)
    {
        for(size_t i = 0; i < size; i++)
        {
            out[0][i] = in[0][i];
            out[1][i] = in[1][i];
        }
        dsy_gpio_write(&led, 0);
        return;
    }

    static float last_sc_in_raw = 0.0f;
    float        sc_target      = hw.adc.GetFloat(kScInput);
    float        sc_start       = last_sc_in_raw;
    float        inv_size       = 1.0f / (float)size;

    for(size_t i = 0; i < size; i++)
    {
        // Linearly interpolate the sidechain ADC input across the block
        // to prevent digital stair-stepping/zipper noise artifacts.
        float t         = (float)(i + 1) * inv_size;
        float sc_in_pre = sc_start + t * (sc_target - sc_start);

        // Track pre-filter peak
        float abs_pre = fabsf(sc_in_pre);
        if(abs_pre > debug_sc_pre_peak)
        {
            debug_sc_pre_peak = abs_pre;
        }

        // Apply sidechain filter at sample rate
        sc_filter.Process(sc_in_pre);
        float sc_in_post = sc_in_pre;
        switch(current_filter_mode)
        {
            case FilterMode::kLPF: sc_in_post = sc_filter.Low(); break;
            case FilterMode::kBPF: sc_in_post = sc_filter.Band(); break;
            case FilterMode::kHPF: sc_in_post = sc_filter.High(); break;
        }

        // Track post-filter peak
        float abs_post = fabsf(sc_in_post);
        if(abs_post > debug_sc_post_peak)
        {
            debug_sc_post_peak = abs_post;
        }

        float left  = in[0][i];
        float right = in[1][i];

        comp.Process(sc_in_post, &left, &right);

        out[0][i] = left;
        out[1][i] = right;
    }

    // Option B: Update LED if envelope exceeds threshold
    dsy_gpio_write(&led, comp.GetEnvelopeDB() > current_threshold ? 1 : 0);

    last_sc_in_raw = sc_target;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    hw.Init();

    // ── Bootloader Entry ─────────────────────────────────────────────────────
    // If D8 (Pin 9) is held LOW (button to GND) during startup, jump to bootloader.
    clear_sw.pin  = hw.GetPin(PIN_CLEAR_SW);
    clear_sw.mode = DSY_GPIO_MODE_INPUT;
    clear_sw.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&clear_sw);
    hw.DelayMs(STARTUP_DELAY_MS);
    if(!dsy_gpio_read(&clear_sw))
    {
        System::Delay(BOOTLOADER_RESET_DELAY_MS);
        daisy::System::ResetToBootloader();
    }

    // Flash built-in LED 4 times as soon as possible
    for(int i = 0; i < BOOT_FLASH_COUNT; i++)
    {
        hw.SetLed(true);
        hw.DelayMs(BOOT_FLASH_DELAY_MS);
        hw.SetLed(false);
        hw.DelayMs(BOOT_FLASH_DELAY_MS);
    }

    hw.SetAudioBlockSize(
        AUDIO_BLOCK_SIZE); // samples per callback; 4 is low-latency default

    sample_rate = hw.AudioSampleRate();

    // ── Sidechain ADC init ────────────────────────────────────────────────────
    // ── ADC init (Sidechain + Cutoff + 8 Pots) ───────────────────────────────
    // NOTE: Pin assignments for Cutoff, Width, and Output have been swapped
    // to match physical wiring based on diagnostics.
    AdcChannelConfig adc_cfg[kNumAdcChannels];
    adc_cfg[kScInput].InitSingle(
        hw.GetPin(PIN_SC_INPUT)); // A10 / D25 = sidechain input
    adc_cfg[kFilterCutoffPot].InitSingle(
        hw.GetPin(PIN_FILTER_CUTOFF)); // A6  / D21
    adc_cfg[kThresholdPot].InitSingle(
        hw.GetPin(PIN_THRESHOLD_POT));                           // A0  / D15
    adc_cfg[kRatioPot].InitSingle(hw.GetPin(PIN_RATIO_POT));     // A1  / D16
    adc_cfg[kAttackPot].InitSingle(hw.GetPin(PIN_ATTACK_POT));   // A2  / D17
    adc_cfg[kReleasePot].InitSingle(hw.GetPin(PIN_RELEASE_POT)); // A3  / D18
    adc_cfg[kMixPot].InitSingle(hw.GetPin(PIN_MIX_POT));         // A4  / D19
    adc_cfg[kDrivePot].InitSingle(hw.GetPin(PIN_DRIVE_POT));     // A5  / D20
    adc_cfg[kWidthPot].InitSingle(hw.GetPin(PIN_WIDTH_POT));     // A7  / D22
    adc_cfg[kOutputPot].InitSingle(hw.GetPin(PIN_OUTPUT_POT));   // A8  / D23
    adc_cfg[kUnusedPot].InitSingle(hw.GetPin(PIN_UNUSED_POT));   // A9  / D24

    hw.adc.Init(adc_cfg, kNumAdcChannels);
    hw.adc.Start();
    //
    // ── Switch init ──────────────────────────────────────────────────────────
    sw_sat1.pin  = hw.GetPin(PIN_SAT_SW1);
    sw_sat1.mode = DSY_GPIO_MODE_INPUT;
    sw_sat1.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&sw_sat1);

    sw_sat2.pin  = hw.GetPin(PIN_SAT_SW2);
    sw_sat2.mode = DSY_GPIO_MODE_INPUT;
    sw_sat2.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&sw_sat2);

    // ── Filter switch init ───────────────────────────────────────────────────
    filter_sw1.pin  = hw.GetPin(PIN_FILTER_SW1);
    filter_sw1.mode = DSY_GPIO_MODE_INPUT;
    filter_sw1.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&filter_sw1);

    filter_sw2.pin  = hw.GetPin(PIN_FILTER_SW2);
    filter_sw2.mode = DSY_GPIO_MODE_INPUT;
    filter_sw2.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&filter_sw2);


    // ── Compressor init ────────────────────────────────────
    comp.Init(sample_rate);
    comp.SetInputGain(
        COMP_INPUT_GAIN_DB); // +6dB trim to bring line-level signals into a better range
    comp.SetInputSaturation(
        COMP_INPUT_SATURATION); // subtle input "color" saturation before compression

    // ── Filter init ────────────────────────────────────────
    sc_filter.Init(sample_rate);
    sc_filter.SetFreq(SC_FILTER_INIT_FREQ);
    sc_filter.SetRes(SC_FILTER_INIT_RES);

#ifdef DEBUG_LOG
    hw.StartLog(true);
    hw.PrintLine("Sidechain Compressor Test");
#endif

    // ── LED init — flash 3x on boot to confirm hardware is alive ─────────────
    led.pin  = hw.GetPin(PIN_LED);
    led.mode = DSY_GPIO_MODE_OUTPUT_PP;
    led.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&led);

    // ── Footswitch init (latching, wired to GND — active-LOW with pull-up) ──
    footswitch.pin  = hw.GetPin(PIN_FOOTSWITCH);
    footswitch.mode = DSY_GPIO_MODE_INPUT;
    footswitch.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&footswitch);

    // ── Footswitch LED init ─────────────────────────────────────────────────
    led_footswitch.pin  = hw.GetPin(PIN_LED_FOOTSWITCH);
    led_footswitch.mode = DSY_GPIO_MODE_OUTPUT_PP;
    led_footswitch.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&led_footswitch);

    // ── Unused button init (momentary, active-LOW with pull-up) ────────────
    unused_button.pin  = hw.GetPin(PIN_UNUSED_BUTTON);
    unused_button.mode = DSY_GPIO_MODE_INPUT;
    unused_button.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&unused_button);

    // ── Ramp button LED init (ramp LED — starts OFF) ──────────────────────
    unused_button_led.pin  = hw.GetPin(PIN_UNUSED_BUTTON_LED);
    unused_button_led.mode = DSY_GPIO_MODE_OUTPUT_PP;
    unused_button_led.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&unused_button_led);
    dsy_gpio_write(&unused_button_led, 0); // ramp LED starts OFF

    for(int i = 0; i < INIT_FLASH_COUNT; i++)
    {
        dsy_gpio_write(&led, 1);
        hw.DelayMs(INIT_FLASH_DELAY_MS);
        dsy_gpio_write(&led, 0);
        hw.DelayMs(INIT_FLASH_DELAY_MS);
    }


    // ── Start audio ──────────────────────────────────────────────────────────
    hw.StartAudio(AudioCallback);

    while(true)
    {
        // =====================================================================
        // Timing — measure dt for LFO phase accumulation
        // =====================================================================
        static uint32_t last_loop_ms = 0;
        uint32_t        now_ms       = daisy::System::GetNow();
        float           dt
            = (last_loop_ms == 0) ? 0.0f : (now_ms - last_loop_ms) * 0.001f;
        last_loop_ms = now_ms;

        // =====================================================================
        // Advance active LFO phases
        // =====================================================================
        for(int i = 0; i < kNumRampableControls; i++)
        {
            if(!ramp_state[i].active)
                continue;
            ramp_state[i].phase += ramp_state[i].rate_hz * dt;
            if(ramp_state[i].phase >= 1.0f)
                ramp_state[i].phase -= floorf(ramp_state[i].phase);
        }

#ifndef DISABLE_POTS
        // =====================================================================
        // Helper lambdas — read raw (inverted) pot value for a control index
        // =====================================================================
        auto GetRawPotValue = [&](int idx) -> float
        {
            switch(idx)
            {
                case kRampThreshold:
                    return 1.0f - hw.adc.GetFloat(kThresholdPot);
                case kRampRatio: return 1.0f - hw.adc.GetFloat(kRatioPot);
                case kRampAttack: return 1.0f - hw.adc.GetFloat(kAttackPot);
                case kRampRelease: return 1.0f - hw.adc.GetFloat(kReleasePot);
                case kRampMix: return 1.0f - hw.adc.GetFloat(kMixPot);
                case kRampDrive: return 1.0f - hw.adc.GetFloat(kDrivePot);
                case kRampWidth: return 1.0f - hw.adc.GetFloat(kWidthPot);
                case kRampOutput: return 1.0f - hw.adc.GetFloat(kOutputPot);
                case kRampCutoff:
                    return 1.0f - hw.adc.GetFloat(kFilterCutoffPot);
                default: return 0.0f;
            }
        };

        auto GetEffectiveRawValue = [&](int idx) -> float
        {
            RampState &rs = ramp_state[idx];
            if(!rs.active)
                return GetRawPotValue(idx);
            float lfo = EvaluateLfo(rs.phase, rs.shape); // 0..1
            return rs.low + lfo * (rs.high - rs.low);
        };

        // Preview LFO phase used while kSettingRate to blink the LED at the dialed rate
        static float preview_phase = 0.0f;
#endif // DISABLE_POTS

        // =====================================================================
        // Hardware control reads
        // =====================================================================

        float raw_ramp_pot_value = 1.0f - hw.adc.GetFloat(kUnusedPot);

        // 0. Read footswitch (latching) and clear switch
#ifdef DISABLE_POTS
        effect_engaged = true; // Force engaged for SC testing
#else
        bool footswitch_raw
            = !dsy_gpio_read(&footswitch); // active-LOW: true = closed/engaged
        effect_engaged = footswitch_raw;

        bool ramp_btn_held = !dsy_gpio_read(&unused_button); // active-LOW

        // PIN_CLEAR_SW: press clears all active ramp states
        static bool clear_sw_prev = false;
        bool        clear_sw_raw  = !dsy_gpio_read(&clear_sw); // active-LOW
        if(clear_sw_raw && !clear_sw_prev)
        {
            for(int i = 0; i < kNumRampableControls; i++)
                ramp_state[i].active = false;
        }
        clear_sw_prev = clear_sw_raw;
#endif
        dsy_gpio_write(&led_footswitch, effect_engaged ? 1 : 0);

        // =====================================================================
        // Learn-gesture state machine
        // =====================================================================
#ifndef DISABLE_POTS
        switch(learn_phase)
        {
            case LearnPhase::kIdle:
                if(ramp_btn_held)
                {
                    for(int i = 0; i < kNumRampableControls; i++)
                        learn_snapshot[i] = GetRawPotValue(i);
                    learn_ramp_pot_snapshot = raw_ramp_pot_value;
                    learn_target            = -1;
                    learn_phase             = LearnPhase::kWaitingForTarget;
                }
                break;

            case LearnPhase::kWaitingForTarget:
                if(!ramp_btn_held)
                {
                    // Released before finding a target — cancel
                    learn_phase = LearnPhase::kIdle;
                    break;
                }
                for(int i = 0; i < kNumRampableControls; i++)
                {
                    if(fabsf(GetRawPotValue(i) - learn_snapshot[i])
                       > RAMP_MOVE_THRESHOLD)
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
                    // Released before rate was set — cancel
                    learn_phase = LearnPhase::kIdle;
                    dsy_gpio_write(&unused_button_led, 0);
                    break;
                }
                learn_committed_high = GetRawPotValue(learn_target);
                if(fabsf(raw_ramp_pot_value - learn_ramp_pot_snapshot)
                   > RAMP_MOVE_THRESHOLD)
                {
                    preview_phase = 0.0f;
                    learn_phase   = LearnPhase::kSettingRate;
                }
                break;

            case LearnPhase::kSettingRate:
                if(!ramp_btn_held)
                {
                    // Commit the ramp
                    RampState &rs = ramp_state[learn_target];
                    rs.active     = true;
                    rs.low        = learn_snapshot[learn_target];
                    rs.high       = learn_committed_high;
                    rs.rate_hz    = RAMP_RATE_MIN_HZ
                                 * powf(RAMP_RATE_MAX_HZ / RAMP_RATE_MIN_HZ,
                                        raw_ramp_pot_value);
                    rs.phase = 0.0f;
                    rs.shape = LfoShape::kTriangle;

                    learn_phase = LearnPhase::kIdle;
                    dsy_gpio_write(&unused_button_led, 0);
                    break;
                }
                {
                    // Live preview: pulse LED at the currently-dialed rate
                    float preview_rate
                        = RAMP_RATE_MIN_HZ
                          * powf(RAMP_RATE_MAX_HZ / RAMP_RATE_MIN_HZ,
                                 raw_ramp_pot_value);
                    preview_phase += preview_rate * dt;
                    if(preview_phase >= 1.0f)
                        preview_phase -= floorf(preview_phase);
                    // Square wave: on when phase < 0.5
                    dsy_gpio_write(&unused_button_led,
                                   preview_phase < 0.5f ? 1 : 0);
                }
                break;
        }

        // =====================================================================
        // Triple-flash when a mapped pot is physically touched (kIdle only)
        // =====================================================================
        if(learn_phase == LearnPhase::kIdle)
        {
            static float last_seen_raw[kNumRampableControls] = {0.0f};
            for(int i = 0; i < kNumRampableControls; i++)
            {
                if(!ramp_state[i].active)
                    continue;
                float live = GetRawPotValue(i);
                if(fabsf(live - last_seen_raw[i]) > RAMP_MOVE_THRESHOLD)
                    TriggerTripleFlash();
                last_seen_raw[i] = live;
            }
            UpdateFlash(now_ms);
        }
#endif // DISABLE_POTS

#ifdef DISABLE_POTS
        // ── Fixed values for sidechain input testing (kick drum) ─────────
        // Filter: LPF at 100 Hz to isolate kick fundamental
        sc_filter.SetFreq(100.0f);
        current_filter_mode = FilterMode::kLPF;

        // Compressor: sensitive, fast attack, moderate release
        current_threshold = -30.0f;
        comp.SetThreshold(current_threshold);
        comp.SetRatio(8.0f);
        comp.SetAttack(0.5f);      // 0.5 ms — catch the transient
        comp.SetRelease(150.0f);   // 150 ms — breathe with the kick
        comp.SetMix(1.0f);         // 100% wet
        comp.SetSaturation(0.0f);  // clean — no drive
        comp.SetStereoWidth(1.0f); // normal stereo
        comp.SetMakeupGain(0.0f);  // unity output

        current_sat_mode = SidechainCompressor::SatMode::kSoft;
        comp.SetSatMode(current_sat_mode);

        // Provide values for debug logging below
        float cutoff_hz  = 100.0f;
        float ratio      = 8.0f;
        float attack_ms  = 0.5f;
        float release_ms = 150.0f;
        float mix_val    = 1.0f;
        float drive_val  = 0.0f;
        float width_val  = 1.0f;
        float makeup_db  = 0.0f;
        bool  s1         = true;
        bool  s2         = false;
#else
        // NOTE: all raw ADC readings are inverted (1.0f - val) to compensate
        // for reversed GND/3.3V wiring on the potentiometers.
        float cutoff_raw = GetEffectiveRawValue(kRampCutoff);
        float cutoff_hz  = CUTOFF_MIN_HZ * powf(CUTOFF_MAX_SCALE, cutoff_raw);
        sc_filter.SetFreq(cutoff_hz);

        bool f1 = !dsy_gpio_read(&filter_sw1); // true if grounded
        bool f2 = !dsy_gpio_read(&filter_sw2);

        if(f1 && !f2)
            current_filter_mode = FilterMode::kLPF;
        else if(!f1 && f2)
            current_filter_mode = FilterMode::kHPF;
        else
            current_filter_mode = FilterMode::kBPF;

        // 2. Read Pots and Update Parameters
        float thresh_val  = GetEffectiveRawValue(kRampThreshold);
        current_threshold = THRESHOLD_MIN_DB
                            + (thresh_val * THRESHOLD_RANGE_DB); // -60 to 0 dB
        comp.SetThreshold(current_threshold);

        float ratio_val = GetEffectiveRawValue(kRampRatio);
        float ratio;
        // Exponential mapping: 1:1 at min → 100:1 at max (at kInfinityCutoff)
        // Past kInfinityCutoff, the ratio becomes infinite (1:inf)
        if(ratio_val <= INFINITY_CUTOFF)
        {
            float normalized_ratio = ratio_val / INFINITY_CUTOFF;
            ratio                  = powf(RATIO_MAX_EXP, normalized_ratio);
        }
        else
        {
            ratio = INFINITY;
        }
        comp.SetRatio(ratio);

        float atk_val = GetEffectiveRawValue(kRampAttack);
        float attack_ms
            = ATTACK_MIN_MS + (atk_val * ATTACK_RANGE_MS); // 0.2ms to 150ms
        comp.SetAttack(attack_ms);

        float rel_val = GetEffectiveRawValue(kRampRelease);
        float release_ms
            = RELEASE_MIN_MS + (rel_val * RELEASE_RANGE_MS); // 0.2ms to 500ms
        comp.SetRelease(release_ms);

        float mix_val = GetEffectiveRawValue(kRampMix);
        comp.SetMix(mix_val); // 0 to 1

        float drive_val = GetEffectiveRawValue(kRampDrive);
        comp.SetSaturation(drive_val); // 0 to 1

        float width_val = GetEffectiveRawValue(kRampWidth);
        comp.SetStereoWidth(width_val * STEREO_WIDTH_MAX); // 0 to 2.0 (200%)

        float output_val = GetEffectiveRawValue(kRampOutput);
        float makeup_db  = output_val * MAKEUP_GAIN_MAX_DB; // 0 to 24 dB makeup
        comp.SetMakeupGain(makeup_db);

        // 3. Read Switch and Update Saturation Mode
        // Logic for 3-pin On-Off-On toggle (Common to GND):
        bool s1 = !dsy_gpio_read(&sw_sat1); // Position 1 (D26 Low)
        bool s2 = !dsy_gpio_read(&sw_sat2); // Position 2 (D27 Low)

        if(s1 && !s2)
            current_sat_mode = SidechainCompressor::SatMode::kSoft;
        else if(!s1 && s2)
            current_sat_mode = SidechainCompressor::SatMode::kDucker;
        else
            current_sat_mode = SidechainCompressor::SatMode::
                kFold; // Center position (Both High)

        comp.SetSatMode(current_sat_mode);
#endif // DISABLE_POTS

#ifdef DEBUG_LOG
        float pre_db
            = 20.0f
              * log10f(debug_sc_pre_peak > 1e-9f ? debug_sc_pre_peak : 1e-9f);
        float post_db
            = 20.0f
              * log10f(debug_sc_post_peak > 1e-9f ? debug_sc_post_peak : 1e-9f);
        debug_sc_pre_peak  = 0.0f;
        debug_sc_post_peak = 0.0f;

        const char *filt_str = (current_filter_mode == FilterMode::kLPF) ? "LPF"
                               : (current_filter_mode == FilterMode::kHPF)
                                   ? "HPF"
                                   : "BPF";

        const char *sat_str
            = (current_sat_mode == SidechainCompressor::SatMode::kSoft) ? "Soft"
              : (current_sat_mode == SidechainCompressor::SatMode::kDucker)
                  ? "Ducker"
                  : "Fold";

        char ratio_buf[16];
        if(std::isinf(ratio))
        {
            snprintf(ratio_buf, sizeof(ratio_buf), "inf");
        }
        else
        {
            int ratio_int  = (int)ratio;
            int ratio_frac = (int)((ratio - ratio_int) * 1000.f);
            if(ratio_frac < 0)
                ratio_frac = -ratio_frac;
            snprintf(
                ratio_buf, sizeof(ratio_buf), "%d.%03d", ratio_int, ratio_frac);
        }

        hw.PrintLine("T:" FLT_FMT3 " R:%s A:" FLT_FMT3 " Rel:" FLT_FMT3
                     " M:" FLT_FMT3 " D:" FLT_FMT3 " W:" FLT_FMT3 " O:" FLT_FMT3
                     " Rate:" FLT_FMT3,
                     FLT_VAR3(current_threshold),
                     ratio_buf,
                     FLT_VAR3(attack_ms),
                     FLT_VAR3(release_ms),
                     FLT_VAR3(mix_val),
                     FLT_VAR3(drive_val),
                     FLT_VAR3(width_val),
                     FLT_VAR3(makeup_db),
                     FLT_VAR3(raw_ramp_pot_value));
        hw.PrintLine("Pre:" FLT_FMT3 " Post:" FLT_FMT3 " C:" FLT_FMT3
                     " F:%s M:%s Byp:%s RampBtn:%d ClrSw:%d",
                     FLT_VAR3(pre_db),
                     FLT_VAR3(post_db),
                     FLT_VAR3(cutoff_hz),
                     filt_str,
                     sat_str,
                     effect_engaged ? "OFF" : "ON",
                     !dsy_gpio_read(&unused_button) ? 1 : 0,
                     !dsy_gpio_read(&clear_sw) ? 1 : 0);
        hw.DelayMs(250);
#endif
    }
}
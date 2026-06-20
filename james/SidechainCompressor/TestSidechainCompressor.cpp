#include "daisy_seed.h"
#include "daisysp.h"
#include "SidechainCompressor.h"

using namespace daisy;

// Uncomment the line below to enable serial logging
// #define DEBUG_LOG 1

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

volatile float current_threshold  = -20.0f;
volatile float debug_sc_pre_peak  = 0.0f;
volatile float debug_sc_post_peak = 0.0f;

// LED fires when envelope follower exceeds this — tune if needed
static constexpr float kLedThreshold = 0.15f;

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
    kNumAdcChannels
};

// State variables
static float sample_rate = 48000.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Audio callback
// ─────────────────────────────────────────────────────────────────────────────

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float sc_in_pre
            = hw.adc.GetFloat(kScInput); // A10 / D25, conditioned 0..1

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
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    hw.Init();

    // ── Bootloader Entry ─────────────────────────────────────────────────────
    // If D8 (Pin 9) is held LOW (button to GND) during startup, jump to bootloader.
    dsy_gpio boot_sw;
    boot_sw.pin  = hw.GetPin(8);
    boot_sw.mode = DSY_GPIO_MODE_INPUT;
    boot_sw.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&boot_sw);
    hw.DelayMs(10);
    //temp: remove
    if(!dsy_gpio_read(&boot_sw))
    {
        System::Delay(500);
        daisy::System::ResetToBootloader();
    }

    // Flash built-in LED 4 times as soon as possible
    for(int i = 0; i < 4; i++)
    {
        hw.SetLed(true);
        hw.DelayMs(100);
        hw.SetLed(false);
        hw.DelayMs(100);
    }

    hw.SetAudioBlockSize(4); // samples per callback; 4 is low-latency default

    sample_rate = hw.AudioSampleRate();

    // ── Sidechain ADC init ────────────────────────────────────────────────────
    // ── ADC init (Sidechain + Cutoff + 8 Pots) ───────────────────────────────
    // NOTE: Pin assignments for Cutoff, Width, and Output have been swapped
    // to match physical wiring based on diagnostics.
    AdcChannelConfig adc_cfg[kNumAdcChannels];
    adc_cfg[kScInput].InitSingle(hw.GetPin(25)); // A10 / D25 = sidechain input
    adc_cfg[kFilterCutoffPot].InitSingle(hw.GetPin(21)); // A6  / D21
    adc_cfg[kThresholdPot].InitSingle(hw.GetPin(15));    // A0  / D15
    adc_cfg[kRatioPot].InitSingle(hw.GetPin(16));        // A1  / D16
    adc_cfg[kAttackPot].InitSingle(hw.GetPin(17));       // A2  / D17
    adc_cfg[kReleasePot].InitSingle(hw.GetPin(18));      // A3  / D18
    adc_cfg[kMixPot].InitSingle(hw.GetPin(19));          // A4  / D19
    adc_cfg[kDrivePot].InitSingle(hw.GetPin(20));        // A5  / D20
    adc_cfg[kWidthPot].InitSingle(hw.GetPin(22));        // A7  / D22
    adc_cfg[kOutputPot].InitSingle(hw.GetPin(23));       // A8  / D23

    hw.adc.Init(adc_cfg, kNumAdcChannels);
    hw.adc.Start();

    // ── Switch init ──────────────────────────────────────────────────────────
    sw_sat1.pin  = hw.GetPin(26);
    sw_sat1.mode = DSY_GPIO_MODE_INPUT;
    sw_sat1.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&sw_sat1);

    sw_sat2.pin  = hw.GetPin(27);
    sw_sat2.mode = DSY_GPIO_MODE_INPUT;
    sw_sat2.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&sw_sat2);

    // ── Filter switch init ───────────────────────────────────────────────────
    filter_sw1.pin  = hw.GetPin(11);
    filter_sw1.mode = DSY_GPIO_MODE_INPUT;
    filter_sw1.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&filter_sw1);

    filter_sw2.pin  = hw.GetPin(12);
    filter_sw2.mode = DSY_GPIO_MODE_INPUT;
    filter_sw2.pull = DSY_GPIO_PULLUP;
    dsy_gpio_init(&filter_sw2);


    // ── Compressor init ────────────────────────────────────
    comp.Init(sample_rate);
    comp.SetInputGain(
        6.0f); // +6dB trim to bring line-level signals into a better range
    comp.SetInputSaturation(
        0.15f); // subtle input "color" saturation before compression

    // ── Filter init ────────────────────────────────────────
    sc_filter.Init(sample_rate);
    sc_filter.SetFreq(1000.f);
    sc_filter.SetRes(0.5f);

#ifdef DEBUG_LOG
    hw.StartLog(true);
    hw.PrintLine("Sidechain Compressor Test");
#endif

    // ── LED init — flash 3x on boot to confirm hardware is alive ─────────────
    led.pin  = hw.GetPin(30);
    led.mode = DSY_GPIO_MODE_OUTPUT_PP;
    led.pull = DSY_GPIO_NOPULL;
    dsy_gpio_init(&led);

    for(int i = 0; i < 3; i++)
    {
        dsy_gpio_write(&led, 1);
        hw.DelayMs(100);
        dsy_gpio_write(&led, 0);
        hw.DelayMs(100);
    }


    // ── Start audio ──────────────────────────────────────────────────────────
    hw.StartAudio(AudioCallback);

    while(true)
    {
        // 1. Read Filter Controls
        // NOTE: all raw ADC readings are inverted (1.0f - val) to compensate
        // for reversed GND/3.3V wiring on the potentiometers.
        float cutoff_raw = 1.0f - hw.adc.GetFloat(kFilterCutoffPot);
        float cutoff_hz  = 20.f * powf(1000.f, cutoff_raw);
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
        float thresh_val  = 1.0f - hw.adc.GetFloat(kThresholdPot);
        current_threshold = -60.0f + (thresh_val * 60.0f); // -60 to 0 dB
        comp.SetThreshold(current_threshold);

        float ratio_val = 1.0f - hw.adc.GetFloat(kRatioPot);
        // Exponential mapping: 1:1 at min → 100:1 at max
        // Mid-pot (~0.5) gives ~10:1, top gives near-limiting at 100:1
        float ratio = powf(100.0f, ratio_val);
        comp.SetRatio(ratio);

        float atk_val   = 1.0f - hw.adc.GetFloat(kAttackPot);
        float attack_ms = 1.0f + (atk_val * 299.0f); // 1ms to 300ms
        comp.SetAttack(attack_ms);

        float rel_val    = 1.0f - hw.adc.GetFloat(kReleasePot);
        float release_ms = 10.0f + (rel_val * 990.0f); // 10ms to 1000ms
        comp.SetRelease(release_ms);

        float mix_val = 1.0f - hw.adc.GetFloat(kMixPot);
        comp.SetMix(mix_val); // 0 to 1

        float drive_val = 1.0f - hw.adc.GetFloat(kDrivePot);
        comp.SetSaturation(drive_val); // 0 to 1

        float width_val = 1.0f - hw.adc.GetFloat(kWidthPot);
        comp.SetStereoWidth(width_val * 2.0f); // 0 to 2.0 (200%)

        float output_val = 1.0f - hw.adc.GetFloat(kOutputPot);
        float makeup_db  = output_val * 24.0f; // 0 to 24 dB makeup
        comp.SetMakeupGain(makeup_db);

        // 2. Read Switch and Update Saturation Mode
        // Logic for 3-pin On-Off-On toggle (Common to GND):
        bool s1 = !dsy_gpio_read(&sw_sat1); // Position 1 (D26 Low)
        bool s2 = !dsy_gpio_read(&sw_sat2); // Position 2 (D27 Low)

        if(s1 && !s2)
            current_sat_mode = SidechainCompressor::SatMode::kSoft;
        else if(!s1 && s2)
            current_sat_mode = SidechainCompressor::SatMode::kHard;
        else
            current_sat_mode = SidechainCompressor::SatMode::
                kFold; // Center position (Both High)

        comp.SetSatMode(current_sat_mode);
#ifdef DEBUG_LOG
        float pre_db
            = 20.0f
              * log10f(debug_sc_pre_peak > 1e-9f ? debug_sc_pre_peak : 1e-9f);
        float post_db
            = 20.0f
              * log10f(debug_sc_post_peak > 1e-9f ? debug_sc_post_peak : 1e-9f);
        debug_sc_pre_peak  = 0.0f;
        debug_sc_post_peak = 0.0f;

        const char* filt_str = (current_filter_mode == FilterMode::kLPF) ? "LPF"
                               : (current_filter_mode == FilterMode::kHPF)
                                   ? "HPF"
                                   : "BPF";

        hw.PrintLine("T:" FLT_FMT3 " Pre:" FLT_FMT3 " Post:" FLT_FMT3
                     " R:" FLT_FMT3 " O:" FLT_FMT3 " C:" FLT_FMT3 " F:%s",
                     FLT_VAR3(current_threshold),
                     FLT_VAR3(pre_db),
                     FLT_VAR3(post_db),
                     FLT_VAR3(ratio),
                     FLT_VAR3(makeup_db),
                     FLT_VAR3(cutoff_hz),
                     filt_str);
        hw.DelayMs(250);
#endif
    }
}
#include "daisy_seed.h"
#include "daisysp.h"
#include "compressor.h"

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

// LED fires when envelope follower exceeds this — tune if needed
static constexpr float kLedThreshold = 0.15f;

// ─────────────────────────────────────────────────────────────────────────────
// Pin assignments
//   Sidechain input : A10 / D25  (board pin 32, via TL072 conditioning circuit)
//   Main stereo in  : Audio In  L/R  (built-in codec)
//   Main stereo out : Audio Out L/R  (built-in codec)
//   LED             : D30
// ─────────────────────────────────────────────────────────────────────────────

DaisySeed           hw;
SidechainCompressor comp;
dsy_gpio            led;
dsy_gpio            sw_sat1, sw_sat2;

enum AdcChannel
{
    kScInput = 0,
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

// Envelope follower state and coefficients — persist across callbacks
static float sc_env          = 0.0f;
static float sc_attack_coef  = 0.0f;
static float sc_release_coef = 0.0f;
static float sample_rate     = 48000.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Audio callback
// ─────────────────────────────────────────────────────────────────────────────

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    float sc_in = hw.adc.GetFloat(0); // A10 / D25, conditioned 0..1
    // float sc_in = 0.0f;

    for(size_t i = 0; i < size; i++)
    {
        float left  = in[0][i];
        float right = in[1][i];

        // Envelope follower — tracks amplitude of sidechain signal
        float rect = fabsf(sc_in);
        float coef = rect > sc_env ? sc_attack_coef : sc_release_coef;
        sc_env     = sc_env + coef * (rect - sc_env);

        comp.Process(sc_env, &left, &right);

        out[0][i] = left;
        out[1][i] = right;
    }

    // Update LED once per block — fast enough to look instantaneous
    dsy_gpio_write(&led, sc_env > kLedThreshold ? 1 : 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    hw.Init();

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
    // ── ADC init (Sidechain + 8 Pots) ────────────────────────────────────────
    AdcChannelConfig adc_cfg[kNumAdcChannels];
    adc_cfg[kScInput].InitSingle(hw.GetPin(25)); // A10 / D25 = sidechain input
    adc_cfg[kThresholdPot].InitSingle(hw.GetPin(15)); // A0  / D15
    adc_cfg[kRatioPot].InitSingle(hw.GetPin(16));     // A1  / D16
    adc_cfg[kAttackPot].InitSingle(hw.GetPin(17));    // A2  / D17
    adc_cfg[kReleasePot].InitSingle(hw.GetPin(18));   // A3  / D18
    adc_cfg[kMixPot].InitSingle(hw.GetPin(19));       // A4  / D19
    adc_cfg[kDrivePot].InitSingle(hw.GetPin(20));     // A5  / D20
    adc_cfg[kWidthPot].InitSingle(hw.GetPin(21));     // A6  / D21
    adc_cfg[kOutputPot].InitSingle(hw.GetPin(22));    // A7  / D22

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

    // ── Compressor init ────────────────────────────────────
    comp.Init(sample_rate);

#ifdef DEBUG_LOG
    hw.StartLog(false);
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
        // 1. Read Pots and Update Parameters
        float thresh_val = hw.adc.GetFloat(kThresholdPot);
        float threshold  = -60.0f + (thresh_val * 60.0f); // -60 to 0 dB
        comp.SetThreshold(threshold);

        float ratio_val = hw.adc.GetFloat(kRatioPot);
        float ratio     = 1.0f + (ratio_val * 19.0f); // 1:1 to 20:1
        comp.SetRatio(ratio);

        float atk_val   = hw.adc.GetFloat(kAttackPot);
        float attack_ms = 1.0f + (atk_val * 299.0f); // 1ms to 300ms
        comp.SetAttack(attack_ms);
        sc_attack_coef
            = 1.0f - expf(-1.0f / (attack_ms * 0.001f * sample_rate));

        float rel_val    = hw.adc.GetFloat(kReleasePot);
        float release_ms = 10.0f + (rel_val * 990.0f); // 10ms to 1000ms
        comp.SetRelease(release_ms);
        sc_release_coef
            = 1.0f - expf(-1.0f / (release_ms * 0.001f * sample_rate));

        float mix_val = hw.adc.GetFloat(kMixPot);
        comp.SetMix(mix_val); // 0 to 1

        float drive_val = hw.adc.GetFloat(kDrivePot);
        comp.SetSaturation(drive_val); // 0 to 1

        float width_val = hw.adc.GetFloat(kWidthPot);
        comp.SetStereoWidth(width_val * 2.0f); // 0 to 2.0 (200%)

        float output_val = hw.adc.GetFloat(kOutputPot);
        float makeup_db  = output_val * 24.0f; // 0 to 24 dB makeup
        comp.SetMakeupGain(makeup_db);

        // 2. Read Switch and Update Saturation Mode
        // Based on build reference logic:
        // - D26 low, D27 high -> Soft
        // - D26 high, D27 low -> Hard
        // - Both low -> Fold (Position 3)
        bool s1 = !dsy_gpio_read(&sw_sat1); // true if pin is low
        bool s2 = !dsy_gpio_read(&sw_sat2); // true if pin is low

        if(s1 && s2)
        {
            current_sat_mode = SidechainCompressor::SatMode::kFold;
        }
        else if(s2)
        {
            current_sat_mode = SidechainCompressor::SatMode::kHard;
        }
        else
        {
            current_sat_mode = SidechainCompressor::SatMode::kSoft;
        }
        comp.SetSatMode(current_sat_mode);

#ifdef DEBUG_LOG
        if(sc_env > 0.01f) // only print when there's sidechain activity
        {
            hw.PrintLine("Thr: " FLT_FMT3 " | Rat: " FLT_FMT3
                         " | Atk: " FLT_FMT3 " | Rel: " FLT_FMT3,
                         FLT_VAR3(threshold),
                         FLT_VAR3(ratio),
                         FLT_VAR3(attack_ms),
                         FLT_VAR3(release_ms));
            hw.DelayMs(50);
        }
#endif
    }
}
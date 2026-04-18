#include "daisy_seed.h"
#include "daisysp.h"
#include "compressor.h"

using namespace daisy;

// ─────────────────────────────────────────────────────────────────────────────
// Hardcoded parameter values — swap these for pot readings once wired up
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float kThresholdDB  = -20.0f;  // dB,  range: -60..0
static constexpr float kRatio        =   4.0f;  //      range: 1..20
static constexpr float kAttackMs     =  10.0f;  // ms,  range: 0.1..100
static constexpr float kReleaseMs    = 150.0f;  // ms,  range: 10..500
static constexpr float kMix          =   1.0f;  // 0=dry, 1=wet
static constexpr float kMakeupDB     =   6.0f;  // dB,  range: -12..+12
static constexpr float kSatAmount    =   0.0f;  // 0..1  (0 = bypassed)
static constexpr float kStereoWidth  =   1.0f;  // 0=mono, 1=normal, 2=hyper-wide

// Saturation mode: kSoft | kHard | kFold
static constexpr SidechainCompressor::SatMode kSatMode = SidechainCompressor::SatMode::kSoft;

// ─────────────────────────────────────────────────────────────────────────────
// Pin assignments
//   Sidechain input : ADC input 0  (Daisy Seed pin 15 = ADC0)
//   Main stereo in  : Audio In  L/R  (built-in codec)
//   Main stereo out : Audio Out L/R  (built-in codec)
// ─────────────────────────────────────────────────────────────────────────────

DaisySeed              hw;
SidechainCompressor    comp;

// ─────────────────────────────────────────────────────────────────────────────
// Audio callback
// ─────────────────────────────────────────────────────────────────────────────

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Sidechain not wired yet — feed silence so compressor passes audio through
    float sc_in = 0.0f;

    for (size_t i = 0; i < size; i++)
    {
        float left  = in[0][i];
        float right = in[1][i];

        comp.Process(sc_in, &left, &right);

        out[0][i] = left;
        out[1][i] = right;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);   // samples per callback; 4 is low-latency default

    float sample_rate = hw.AudioSampleRate();

    // ── Compressor init & parameter setup ────────────────────────────────────
    comp.Init(sample_rate);
    comp.SetThreshold(kThresholdDB);
    comp.SetRatio(kRatio);
    comp.SetAttack(kAttackMs);
    comp.SetRelease(kReleaseMs);
    comp.SetMix(kMix);
    comp.SetMakeupGain(kMakeupDB);
    comp.SetSaturation(kSatAmount);
    comp.SetSatMode(kSatMode);
    comp.SetStereoWidth(kStereoWidth);

    // ── Start audio ──────────────────────────────────────────────────────────
    hw.StartAudio(AudioCallback);

    // Loop forever — all work happens in the audio callback
    while (true) {}
}
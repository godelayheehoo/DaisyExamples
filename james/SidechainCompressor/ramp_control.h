#ifndef RAMP_CONTROL_H
#define RAMP_CONTROL_H

#include <cmath>
#include "constants.h"

// ─────────────────────────────────────────────────────────────────────────────
// LFO shape
// ─────────────────────────────────────────────────────────────────────────────

enum class LfoShape
{
    kTriangle,
    // future: kSine, kSquare, kSaw
};

// Returns a 0..1 unipolar value for the given 0..1 phase.
// phase = 0 -> 0 (low), phase = 0.5 -> 1 (high), phase = 1 -> 0.
inline float EvaluateLfo(float phase, LfoShape shape)
{
    switch(shape)
    {
        case LfoShape::kTriangle: return 1.0f - fabsf(2.0f * phase - 1.0f);
        default: return 0.5f * (1.0f - cosf(2.0f * (float)M_PI * phase));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Control index enum — maps each of the 9 main pots to a ramp slot
// ─────────────────────────────────────────────────────────────────────────────

enum RampableControl
{
    kRampThreshold = 0,
    kRampRatio,
    kRampAttack,
    kRampRelease,
    kRampMix,
    kRampDrive,
    kRampWidth,
    kRampOutput,
    kRampCutoff,
    kNumRampableControls
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-control ramp state (stored in raw pot-space [0,1])
// ─────────────────────────────────────────────────────────────────────────────

struct RampState
{
    bool     active  = false;
    float    low     = 0.0f; // raw pot-space [0,1], post-inversion
    float    high    = 0.0f; // raw pot-space [0,1], post-inversion
    float    rate_hz = 0.5f;
    float    phase   = 0.0f; // 0..1, wraps
    LfoShape shape   = LfoShape::kTriangle;
};

// ─────────────────────────────────────────────────────────────────────────────
// Learn-gesture state machine phases
// ─────────────────────────────────────────────────────────────────────────────

enum class LearnPhase
{
    kIdle,
    kWaitingForTarget, // button held, watching for a pot to move
    kSettingHigh,      // target found, tracking its value + watching ramp pot
    kSettingRate // ramp pot moving, locking rate; releasing button commits
};

#endif // RAMP_CONTROL_H

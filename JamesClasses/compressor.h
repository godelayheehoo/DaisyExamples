#ifndef JAMES_COMPRESSOR_H
#define JAMES_COMPRESSOR_H

#include <cmath>

/**
 * SidechainCompressor
 *
 * Stereo compressor with external sidechain input, designed for Daisy Seed.
 * RMS envelope follower on the sidechain signal drives gain reduction on the
 * main stereo bus. Soft-knee gain computer, independent attack/release
 * smoothing, wet/dry mix, and optional saturation on the output stage.
 *
 * Usage:
 *   SidechainCompressor comp;
 *   comp.Init(sample_rate);
 *
 *   // In your audio callback:
 *   comp.SetThreshold(threshold_db);   // e.g. -20.0f
 *   comp.SetRatio(ratio);              // e.g. 4.0f
 *   comp.SetAttack(attack_ms);         // e.g. 10.0f
 *   comp.SetRelease(release_ms);       // e.g. 150.0f
 *   comp.SetMix(mix);                  // 0.0 = dry, 1.0 = fully compressed
 *   comp.SetMakeupGain(gain_db);       // output level trim
 *   comp.SetSaturation(amount);        // 0.0–1.0
 *   comp.SetSatMode(SatMode::kSoft);
 *   comp.SetStereoWidth(width);        // 0.0 = mono, 1.0 = full stereo, >1.0 = hyper-wide
 *
 *   comp.Process(sc_in, &main_l, &main_r);
 */

class SidechainCompressor {
 public:
  enum class SatMode { kSoft, kHard, kFold };




  void Init(float sample_rate) {
    // Assign sample_rate_ first — UpdateCoeffs() depends on it
    sample_rate_   = sample_rate;

    // Defaults
    threshold_db_  = -20.0f;
    ratio_         = 4.0f;
    knee_db_       = 6.0f;   // soft knee half-width
    attack_ms_     = 10.0f;
    release_ms_    = 150.0f;
    mix_           = 1.0f;
    makeup_db_     = 0.0f;
    sat_amount_    = 0.0f;
    sat_mode_      = SatMode::kSoft;
    stereo_width_  = 1.0f;

    // State
    env_           = 0.0f;
    gain_smooth_   = 1.0f;

    UpdateCoeffs();
  }

  // ── Setters ────────────────────────────────────────────────────────────────

  void SetThreshold(float db)      { threshold_db_ = db; }
  void SetRatio(float ratio)       { ratio_ = fmaxf(1.0f, ratio); }
  void SetAttack(float ms)         { attack_ms_ = fmaxf(0.01f, ms); UpdateCoeffs(); }
  void SetRelease(float ms)        { release_ms_ = fmaxf(1.0f, ms);  UpdateCoeffs(); }
  void SetMix(float mix)           { mix_ = fclamp(mix, 0.0f, 1.0f); }
  void SetMakeupGain(float db)     { makeup_db_ = db; }
  void SetSaturation(float amount) { sat_amount_ = fclamp(amount, 0.0f, 1.0f); }
  void SetSatMode(SatMode mode)    { sat_mode_ = mode; }
  void SetStereoWidth(float width) { stereo_width_ = fmaxf(0.0f, width); }
  void SetKnee(float db)           { knee_db_ = fmaxf(0.0f, db); }

  // ── Per-sample process ─────────────────────────────────────────────────────
  // sc_in   : mono sidechain sample (e.g. kick drum input after input stage)
  // main_l/r: in-place stereo main bus

  void Process(float sc_in, float* main_l, float* main_r) {
    // --- 1. RMS envelope follower on sidechain ---
    float sc_sq = sc_in * sc_in;
    // One-pole filter: squared signal → squared envelope
    if (sc_sq > env_) {
      env_ = att_coeff_ * env_ + (1.0f - att_coeff_) * sc_sq;
    } else {
      env_ = rel_coeff_ * env_ + (1.0f - rel_coeff_) * sc_sq;
    }
    float level_db = LinToDB(sqrtf(fmaxf(env_, 1e-18f)));

    // --- 2. Gain computer (soft knee) ---
    float gain_reduction_db = GainComputer(level_db);

    // --- 3. Smooth the gain reduction signal ---
    // gain_reduction_db is negative (or zero). Convert to linear for smoothing.
    float target_gain = DBToLin(gain_reduction_db);
    if (target_gain < gain_smooth_) {
      // Gain is going down → use attack
      gain_smooth_ = att_coeff_ * gain_smooth_ + (1.0f - att_coeff_) * target_gain;
    } else {
      // Gain is recovering → use release
      gain_smooth_ = rel_coeff_ * gain_smooth_ + (1.0f - rel_coeff_) * target_gain;
    }

    // --- 4. Apply gain + makeup to main bus ---
    float makeup_lin = DBToLin(makeup_db_);
    float wet_l = *main_l * gain_smooth_ * makeup_lin;
    float wet_r = *main_r * gain_smooth_ * makeup_lin;

    // --- 5. Stereo width (mid/side) ---
    if (stereo_width_ != 1.0f) {
      float mid  = 0.5f * (wet_l + wet_r);
      float side = 0.5f * (wet_l - wet_r) * stereo_width_;
      wet_l = mid + side;
      wet_r = mid - side;
    }

    // --- 6. Saturation ---
    if (sat_amount_ > 0.0f) {
      wet_l = Saturate(wet_l);
      wet_r = Saturate(wet_r);
    }

    // --- 7. Wet/dry mix ---
    *main_l = mix_ * wet_l + (1.0f - mix_) * (*main_l);
    *main_r = mix_ * wet_r + (1.0f - mix_) * (*main_r);
  }

  // Returns current gain reduction in dB (useful for a GR meter)
  float GetGainReductionDB() const { return LinToDB(gain_smooth_); }

 private:
  // ── Gain computer with soft knee ──────────────────────────────────────────
  float GainComputer(float level_db) const {
    float diff = level_db - threshold_db_;
    float half_knee = knee_db_ * 0.5f;

    if (diff < -half_knee) {
      // Below knee: unity gain (0 dB reduction)
      return 0.0f;
    } else if (diff < half_knee) {
      // Inside knee: interpolate smoothly
      float x = (diff + half_knee) / knee_db_;  // 0..1
      return (1.0f / ratio_ - 1.0f) * (x * x) * half_knee;
    } else {
      // Above knee: full compression
      return diff * (1.0f / ratio_ - 1.0f);
    }
  }

  // ── Saturation modes ──────────────────────────────────────────────────────
  float Saturate(float x) const {
    float driven = x * (1.0f + sat_amount_ * 4.0f);  // pre-gain into sat stage
    float y;
    switch (sat_mode_) {
      case SatMode::kSoft:
        // Tanh soft clip — smooth, tube-like
        y = tanhf(driven);
        break;
      case SatMode::kHard:
        // Hard clip at ±1
        y = fclamp(driven, -1.0f, 1.0f);
        break;
      case SatMode::kFold:
        // Wavefolding — folds signal back on itself
        y = Wavefold(driven);
        break;
      default:
        y = driven;
    }
    // Compensate for pre-gain so output level stays roughly consistent
    return y / (1.0f + sat_amount_ * 4.0f);
  }

  float Wavefold(float x) const {
    // Triangle-wave fold: wraps signal into [-1, 1]
    // One full fold period = 4 units
    x = x * 0.5f + 0.5f;          // shift to [0,1] range for easier folding math
    x = x - floorf(x);             // fractional part → [0, 1)
    x = (x < 0.5f) ? 2.0f * x : 2.0f * (1.0f - x);  // triangle wave [0,1]
    return x * 2.0f - 1.0f;       // back to [-1, 1]
  }

  // ── Time constant helpers ─────────────────────────────────────────────────
  void UpdateCoeffs() {
    // One-pole IIR coefficient from time constant in ms
    // tau = -1 / (fs * ln(1 - coeff))  →  coeff = 1 - exp(-1 / (fs * tau_s))
    att_coeff_ = TimeToCoeff(attack_ms_);
    rel_coeff_ = TimeToCoeff(release_ms_);
  }

  float TimeToCoeff(float ms) const {
    return expf(-1.0f / (sample_rate_ * ms * 0.001f));
  }

  // ── dB conversion ─────────────────────────────────────────────────────────
  static float LinToDB(float lin) {
    return 20.0f * log10f(fmaxf(lin, 1e-9f));
  }

  static float DBToLin(float db) {
    return powf(10.0f, db / 20.0f);
  }

  // ── Simple clamp (no std::clamp dependency needed) ────────────────────────
  static float fclamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
  }

  // ── Parameters ────────────────────────────────────────────────────────────
  float sample_rate_ = 48000.0f;
  float threshold_db_;
  float ratio_;
  float knee_db_;
  float attack_ms_;
  float release_ms_;
  float mix_;
  float makeup_db_;
  float sat_amount_;
  SatMode sat_mode_;
  float stereo_width_;

  // ── State ─────────────────────────────────────────────────────────────────
  float env_;          // squared RMS envelope state
  float gain_smooth_;  // smoothed linear gain (attack/release applied)
  float att_coeff_;
  float rel_coeff_;
};

#endif
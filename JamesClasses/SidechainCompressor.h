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
  enum class SatMode { kSoft, kFold, kDucker };




  void Init(float sample_rate) {
    // Assign sample_rate_ first — UpdateCoeffs() depends on it
    sample_rate_   = sample_rate;

    // Defaults
    input_gain_db_ = 0.0f;
    threshold_db_  = -20.0f;
    ratio_         = 4.0f;
    knee_db_       = 6.0f;   // soft knee half-width
    attack_ms_     = 10.0f;
    release_ms_    = 150.0f;
    mix_           = 1.0f;
    makeup_db_     = 0.0f;
    sat_amount_    = 0.0f;
    input_sat_amount_ = 0.0f; // Default to clean
    sat_mode_      = SatMode::kSoft;
    stereo_width_  = 1.0f;

    // State
    env_           = 0.0f;
    gr_smooth_db_  = 0.0f;

    UpdateCoeffs();
  }

  // ── Setters ────────────────────────────────────────────────────────────────

  void SetInputGain(float db)      { input_gain_db_ = db; }
  void SetThreshold(float db)      { threshold_db_ = db; }
  void SetRatio(float ratio)       { ratio_ = fmaxf(1.0f, ratio); }
  void SetAttack(float ms)         { attack_ms_ = fmaxf(0.01f, ms); UpdateCoeffs(); }
  void SetRelease(float ms)        { release_ms_ = fmaxf(1.0f, ms);  UpdateCoeffs(); }
  void SetMix(float mix)           { mix_ = fclamp(mix, 0.0f, 1.0f); }
  void SetMakeupGain(float db)     { makeup_db_ = db; }
  void SetSaturation(float amount) { sat_amount_ = fclamp(amount, 0.0f, 1.0f); }
  void SetInputSaturation(float amount) { input_sat_amount_ = fclamp(amount, 0.0f, 1.0f); }
  void SetSatMode(SatMode mode)    { sat_mode_ = mode; }
  void SetStereoWidth(float width) { stereo_width_ = fmaxf(0.0f, width); }
  void SetKnee(float db)           { knee_db_ = fmaxf(0.0f, db); }

  // ── Per-sample process ─────────────────────────────────────────────────────
  // sc_in   : mono sidechain sample (e.g. kick drum input after input stage)
  // main_l/r: in-place stereo main bus

  void Process(float sc_in, float* main_l, float* main_r) {
    // --- 0. Input gain staging ---
    float in_gain_lin = DBToLin(input_gain_db_);
    float dry_l = *main_l * in_gain_lin;
    float dry_r = *main_r * in_gain_lin;

    // --- 0.5 Input Saturation (subtle color) ---
    float staged_l = dry_l;
    float staged_r = dry_r;
    if (input_sat_amount_ > 0.0f) {
        staged_l = InputSaturate(dry_l);
        staged_r = InputSaturate(dry_r);
    }

    // --- 1. Peak envelope follower on sidechain ---
    // Smooth the linear amplitude first to avoid log(0) issues and spikes.
    float rect = fabsf(sc_in);
    if (rect > env_) {
      env_ = att_coeff_ * env_ + (1.0f - att_coeff_) * rect;
    } else {
      env_ = rel_coeff_ * env_ + (1.0f - rel_coeff_) * rect;
    }
    
    // Now convert the smoothed linear envelope to dB for the gain computer.
    // This ensures we are comparing dB to dB (threshold is in dB).
    float level_db = LinToDB(env_);

    // --- 2. Gain computer (dB) ---
    float gain_reduction_db;
    if (sat_mode_ == SatMode::kDucker) {
      // Repurpose sat_amount_ (0..1) to control ducking depth (e.g. 0 to -100 dB)
      float duck_depth_db = -sat_amount_ * 100.0f;
      gain_reduction_db = (level_db > threshold_db_) ? duck_depth_db : 0.0f;
    } else {
      gain_reduction_db = GainComputer(level_db);
    }

    // --- 3. Smooth the gain reduction signal in the dB domain ---
    // This makes the gain reduction logarithmic (linear in dB)
    if (gain_reduction_db < gr_smooth_db_) {
      // Gain is going down (attack)
      gr_smooth_db_ = att_coeff_ * gr_smooth_db_ + (1.0f - att_coeff_) * gain_reduction_db;
    } else {
      // Gain is recovering (release)
      gr_smooth_db_ = rel_coeff_ * gr_smooth_db_ + (1.0f - rel_coeff_) * gain_reduction_db;
    }

    // --- 4. Apply gain + makeup to main bus ---
    float total_gain_db = gr_smooth_db_ + makeup_db_;
    float gain_lin      = DBToLin(total_gain_db);
    float wet_l         = staged_l * gain_lin;
    float wet_r         = staged_r * gain_lin;

    // --- 5. Stereo width (mid/side) ---
    // Use an epsilon to bypass the matrix when width is near 100% (unity).
    // This prevents ADC jitter from causing tiny crosstalk/smearing near the center.
    if(fabsf(stereo_width_ - 1.0f) > 0.02f)
    {
        float mid  = wet_l + wet_r;
        float side = (wet_l - wet_r) * stereo_width_;
        // Apply the 0.5f scaling factor on the decode side for standard normalization
        wet_l = 0.5f * (mid + side);
        wet_r = 0.5f * (mid - side);
    }

    // --- 6. Saturation (Output stage) ---
    if (sat_amount_ > 0.0f) {
      wet_l = Saturate(wet_l);
      wet_r = Saturate(wet_r);
    }

    // --- 7. Wet/dry mix ---
    *main_l = mix_ * wet_l + (1.0f - mix_) * dry_l;
    *main_r = mix_ * wet_r + (1.0f - mix_) * dry_r;
  }

  // Returns current gain reduction in dB (useful for a GR meter)
  float GetGainReductionDB() const { return gr_smooth_db_; }

  // Returns the current sidechain envelope level (linear 0..1)
  float GetEnvelope() const { return env_; }

  // Returns the current sidechain envelope level (dB)
  float GetEnvelopeDB() const { return LinToDB(env_); }

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
  float InputSaturate(float x) const {
    // Subtle soft clip: x - x^3 / 6 (first part of sin(x))
    // We scale the input to bite a bit more if input_sat_amount_ is high.
    float drive = 1.0f + input_sat_amount_ * 0.5f;
    float x_driven = fclamp(x * drive, -1.5f, 1.5f);
    float x2 = x_driven * x_driven;
    float y = x_driven * (1.0f - 0.16666667f * x2);
    // Compensate for drive
    return y / drive;
  }

  float Saturate(float x) const {
    if (sat_mode_ == SatMode::kDucker) {
      return x; // Bypass output stage processing completely
    }
    float driven = x * (1.0f + sat_amount_ * 4.0f);  // pre-gain into sat stage
    float y;
    switch (sat_mode_) {
      case SatMode::kSoft:
        // Tanh soft clip — smooth, tube-like
        y = tanhf(driven);
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
  float input_gain_db_;
  float threshold_db_;
  float ratio_;
  float knee_db_;
  float attack_ms_;
  float release_ms_;
  float mix_;
  float makeup_db_;
  float sat_amount_;
  float input_sat_amount_;
  SatMode sat_mode_;
  float stereo_width_;

  // ── State ─────────────────────────────────────────────────────────────────
  float env_;            // squared RMS envelope state
  float gr_smooth_db_;   // smoothed gain reduction in dB
  float att_coeff_;
  float rel_coeff_;
};

#endif
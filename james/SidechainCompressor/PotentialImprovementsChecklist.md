1. Envelope follower is too coarse / aliased
What it is: The envelope follower runs at audio rate but is only updated from the ADC reading once per callback block. If the sidechain rectification is jerky or the smoothing coefficients are mismatched to the signal, the gain reduction modulation itself will have artifacts — clicks, zipper noise, or a mechanical quality.
Technically: The current code reads hw.adc.GetFloat(0) once per block and then applies the same value to all samples in that block. That means gain changes can step rather than interpolate. Smoothing inside the loop helps, but the single-ADC-read-per-block is a ceiling.
Fix sketch: Move the sidechain ADC read inside the sample loop (the Daisy ADC is continuously sampling, the read is just a register fetch so it's cheap), or interpolate the envelope value across the block.
Difficulty: Low. Effort: Small but requires careful listening to confirm improvement.

2. [subjective] Attack/release curves feel wrong for musical pumping
What it is: Linear-in-time attack/release on the gain reduction doesn't match how analog compressors actually behave. Classic pumping compressors have non-linear curves — attack can be near-instantaneous, release often has a two-stage shape (fast early, then slower tail). The current code uses simple one-pole IIR, which is correct mathematically but may sound "digital" or abrupt.
Fix sketch: A couple of options — log-domain gain reduction (compute everything in dB, smooth the dB value rather than the linear envelope), or a dual-stage release (fast coefficient for the first portion of release, then a slower one). The latter is what gives classic pumps their musical "breathe."
Difficulty: Medium. Effort: Medium — requires experimentation to dial in the curves, there's no single right answer.

3. [DONE] Gain reduction math is linear, should be logarithmic
What it is: Compressor arithmetic done in the linear domain (multiplying the signal by a gain factor) rather than the log domain (operating in dB) sounds harder and less musical.
Technically: Fixed in JamesClasses/SidechainCompressor.h by smoothing gain reduction in the dB domain and using a Peak detector for level sensing. This ensures the attack/release curves are linear in dB.
Difficulty: Low-Medium. Effort: Low once you know whether it's already handled.

4. [subjective] Saturation is happening post-makeup-gain, wrong order
What it is: If the signal hits the saturation stage at the wrong level — either too hot (clipping hard every hit) or too cold (saturation does nothing) — the character of the saturation sounds wrong or invisible. Also, applying saturation after the dry/wet mix means the dry signal gets saturated, which may not be what you want.
Fix sketch: Experiment with saturation before vs. after the mix blend, and before vs. after makeup gain. For synths at line level, saturation probably wants to be post-compression but pre-makeup, so you're colouring the compressed signal before boosting it up.
Difficulty: Low. Effort: Low, just reordering operations.

5. Soft/hard clip implementation is too simple
What it is: A basic tanh or hard clip is not the same as the non-linear behaviour of analog tape or transformers. At moderate drive settings it can sound like nothing, and at high settings it can sound harsh rather than "thick." The wavefold mode especially can produce aliasing artifacts at line level.
Fix sketch: For soft clip, a polynomial waveshaper (e.g. x - x³/3) has a more gradual knee than tanh. For the wavefold, oversampling (2x or 4x) then downsampling reduces aliasing — the Daisy has enough headroom to run 2x oversampling on a stereo signal. DaisySP has an Overdrive class that may sound more musical than a raw clip function.
Difficulty: Medium (oversampling), Low (waveshaper swap). Effort: Medium — waveshaper swap is quick; oversampling takes more plumbing.

6. [DONE] No input/output gain staging
What it is: Line level synths are nominally around −10 dBu (consumer) to +4 dBu (semi-pro). The Daisy Seed's codec expects a certain input range to use the full ADC resolution. If you're running in too hot or too cold, you're either clipping the codec or losing bit depth, both of which sound bad independently of the compressor.
Fix sketch: Add a fixed or adjustable input trim before the compressor, separate from the output trim. This is also the place to add a tiny bit of "input saturation" for colour before compression.
Difficulty: Low. Effort: Low — it's a multiply, but requires measurement or listening to set correctly.

7. [DONE] Stereo width processing introduces phase artifacts
What it is: The mid/side width processing (assuming it's rotating between M/S) can cause comb filtering or mono-incompatibility issues if not done carefully. On synths this usually shows up as the stereo image sounding "smeared" or the signal disappearing when summed to mono.
Technically: Fixed in JamesClasses/SidechainCompressor.h by implementing a 2% dead-zone around the center point (1.0) and moving the 0.5 normalization factor to the decode side of the matrix. This ensures the matrix is completely bypassed when the pot is near center, eliminating smearing from ADC noise.
Difficulty: Low. Effort: Low.

8. [Probably skipping] Pumping isn't musical — timing feels off
What it is: Even with everything working correctly, if the attack and release times don't feel locked to the tempo, the pumping can sound random rather than musical. This is a parameter problem, not a code problem, but it's worth naming.
Fix sketch: No code change needed — calculate attack and release times from BPM. For 120 BPM (500ms quarter note), a release of ~200–300ms creates tight French house pumping; ~600–800ms creates the slower breathe of classic trance. Attack should be fast enough to let the kick transient through (1–5ms) before clamping.
Difficulty: N/A. Effort: Experimentation only.

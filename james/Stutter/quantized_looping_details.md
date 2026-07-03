# Feature: Quantized Playback Rate Modes for Microloop/Stutter Engine
## Overview

Add optional pitch-quantized control of playback rate for a stutter/microloop effect. The system already captures a fixed-length audio buffer when the stutter trigger is pressed (e.g., 1/16 note worth of audio) and immediately loops it. This feature adds three modes for how the playback rate knob behaves:

- No quantization (continuous rate control)
- Loop-frequency quantization
- Pitch-detection-based quantization

## 1. Mode Selection (UI / Config)

Add a new setting to the existing config/menu system:

Playback Rate Mode (PRM):

- OFF (default): continuous playback rate
- LFO or LFQ (abbrev allowed): loop-frequency quantization
- PIT or PTQ: pitch-detection quantization

This setting affects how the rate knob is interpreted after a stutter buffer is captured.

No changes are required to the stutter trigger behavior.

## 2. Core Data Model Additions

Extend stutter buffer state with:

```cpp
struct StutterState {
    float sampleRate;
    int bufferSize;
    float detectedPitchHz;      // valid only for PTQ
    float pitchConfidence;      // 0.0–1.0
    bool pitchValid;

    float loopFrequencyHz;      // derived from buffer size
};
```

## 3. Mode 1: OFF (No Quantization)

Behavior:

- Knob directly maps to playback rate multiplier
- No pitch or frequency analysis used

Formula:

playbackRate = knobValueMappedDirectly;

This preserves current behavior.

## 4. Mode 2: Loop-Frequency Quantization (LFQ)

### Concept

Treat the loop itself as an oscillator. Pitch is derived from loop length, not content.

### Computation (run once per stutter capture)

When buffer is filled:

```cpp
loopFrequencyHz = sampleRate / bufferSize;
```

Convert knob position into semitone offset:

```cpp
desiredFreq = loopFrequencyHz * pow(2.0f, semitoneOffset / 12.0f);
playbackRate = desiredFreq / loopFrequencyHz;
```

Simplifies to:

```cpp
playbackRate = pow(2.0f, semitoneOffset / 12.0f);
```

### Interpretation

- Knob controls semitone offset relative to loop's inherent pitch
- No audio analysis required
- Extremely stable and deterministic

## 5. Mode 3: Pitch Detection Quantization (PTQ)
### Concept

Detect fundamental frequency of captured audio and use it as reference pitch.

### When to run analysis
- Run once per stutter capture
- Must NOT run in audio callback
- Run asynchronously (main thread or low-priority task)
- Algorithm options (implement at least autocorrelation; YIN optional later)

### Autocorrelation approach:

- Define search range:
    - e.g. 50 Hz – 2000 Hz

- Convert to lag range:

```cpp
minLag = sampleRate / maxFreq;
maxLag = sampleRate / minFreq;
```

- Compute normalized autocorrelation over buffer
- Find lag with maximum correlation peak

Convert lag → frequency:

```cpp
freq = sampleRate / bestLag;
```

- Convert frequency to musical note:

```cpp
midiNote = 69 + 12 * log2(freq / 440.0f);
```

- Quantize:

```cpp
midiNoteQ = round(midiNote);
```

- Convert back:

```cpp
targetFreq = 440.0f * pow(2.0f, (midiNoteQ - 69) / 12.0f);
```

- Playback rate calculation:

```cpp
Relative to detected pitch:

playbackRate = targetFreq / detectedPitchHz;
```

- Confidence handling:

```cpp
Return:

pitchValid = (confidence > 0.7f);

```

- If invalid:
    - fall back to LFQ mode automatically OR bypass quantization for that buffer

## 6. UI / Runtime Behavior

When a new stutter capture occurs:

- Buffer is immediately looped (no waiting)
- Loop-frequency is computed instantly
- Pitch detection runs asynchronously
- Once pitch result is ready:
    - store result in StutterState
    - mark valid/invalid

### Knob behavior depends on mode:

- OFF
    - continuous rate
- LFQ
    - rate snaps to semitone grid relative to loop frequency
- PTQ
    - rate snaps to semitone grid relative to detected pitch
    - if pitch invalid → fallback to LFQ behavior

## 7. Optional Smoothing (recommended)

When switching between quantized values:

- apply short linear or exponential slew (10–50 ms)
- prevents stepping artifacts when detector completes

## 8. Performance Notes

- Pitch detection runs only once per stutter event
- CPU cost is negligible on STM32H750
- No need for real-time analysis
- Autocorrelation implementation should operate on fixed buffer size

## 9. Expected Behavior Summary

- LFQ: musically quantized playback tied to loop length (granular / oscillator-like behavior)
- PTQ: musically quantized playback tied to detected pitch content (instrument-aware transposition)
- OFF: traditional tape-style stutter pitch shifting
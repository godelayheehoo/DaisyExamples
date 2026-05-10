# StereoDirt

## Author

<!-- Insert Your Name Here -->

## Description

<!-- Describe your example here -->
# Stereo Fuzz Bender

Stereo Fuzz Bender is a real-time stereo audio processing system focused on nonlinear distortion, harmonic transformation, and controlled instability. It is designed to treat distortion not as a single effect, but as a set of interacting behaviors that reshape audio differently across the stereo field.

Rather than applying uniform processing to both channels, the system intentionally allows divergence between left and right paths. This creates movement, phase complexity, and evolving spatial character that feels less like a static effect and more like an active system responding to input energy.

## Design Goals
- Stereo as a first-class dimension
Left and right channels can be processed differently, creating width and motion through contrast rather than simple widening.
- Multiple nonlinear “dirt” behaviors
  - Distortion is expressed through distinct operating modes, each with a different response curve and failure behavior:
  - Hard Clip Mode: abrupt amplitude limiting that produces sharp-edged harmonic content and aggressive saturation.
  - Soft Clip Mode: smoother, rounded saturation with more gradual harmonic buildup and musical compression behavior.
  - Foldover Mode: nonlinear folding distortion that introduces dense, chaotic harmonic structures and unstable tonal motion.
- Controlled instability
The system is designed to push into chaotic or near-chaotic regimes without immediately collapsing into unusable noise, unless deliberately driven there.
- Real-time performance focus
Processing is optimized for low latency and responsive behavior suitable for live interaction.
Implementation Target

The initial implementation is built for the Daisy Pod embedded audio platform. This provides a constrained but capable real-time DSP environment that encourages efficient signal design while still supporting complex stereo processing behavior.

The Daisy Pod serves as the primary development and performance target for early iterations of the system.

## Architecture Overview

The processing pipeline is organized into modular stages operating on stereo buffers. Each stage is responsible for a specific type of transformation, and stages can introduce asymmetry between channels where appropriate.

Key structural principles include:

- deterministic audio flow
- minimal per-stage state complexity
- clear separation between signal transformation and system control logic
- extensibility for additional nonlinear and modulation behaviors
## Current Status

The system is in early development. Core stereo routing and the initial set of distortion modes are being established, with ongoing refinement of how each mode behaves under dynamic input conditions.

The long-term direction is a flexible stereo distortion environment capable of moving between subtle coloration and extreme nonlinear breakdown, with distinct “dirt modes” forming the foundational palette for sonic behavior.

## Instructions for Use (Daisy Pod)

The current firmware maps the Daisy Pod controls as follows:

### Knobs
- **Knob 1 (Drive)**: Controls the overall intensity of the distortion.
- **Knob 2 (Volume)**: Controls the master output volume, allowing you to compensate for loud distortion signals.

### Shift Functions (Hold Encoder)
While holding down the Encoder button, the knobs take on secondary functions. The main Daisy LED will illuminate while shift is engaged.
- **Shift + Knob 1 (M/S Drive Balance)**: Adjusts the balance of drive between Mid and Side channels. *(Note: This only affects the sound when in **MS-ISH** stereo mode)*. `0` = 100% Mid drive, `1` = 100% Side drive.
- **Shift + Knob 2 (Mono Bass / Side HPF)**: Sets the cutoff frequency of a high-pass filter on the side channel, ranging from 20Hz to 2000Hz. At `0`, it is fully bypassed. Turning this up forces the low end into the center image (Mono Bass). Works across all stereo modes!

*Note: The shift controls feature "catch-up" (pass-through) logic. When you release the encoder, the primary parameters won't abruptly snap to the knob's physical position. You must turn the knob back through their last known value for them to "catch" and begin moving again.*

### Buttons & Encoder
- **Encoder Click**: Held down to engage the **Shift** functions.
- **Button 1 (Distortion Mode)**: Cycles through the available distortion modes. **LED 1** indicates the active mode:
  - 🔴 **Red**: Hard Clip
  - 🟢 **Green**: Soft Clip (Overdrive)
  - 🔵 **Blue**: Foldover (Wavefolder)
  - 🟣 **Purple**: Decimate (Bitcrush/Downsample)
  - ⚫ **Off**: Distortion Bypassed
- **Button 2 (Stereo Mode)**: Cycles through stereo widening modes. **LED 2** indicates the active mode:
  - 🧊 **Cyan**: Normal (Independent L/R)
  - 🌺 **Magenta**: Width (Exaggerated side channel)
  - 🟠 **Orange**: MS-ISH (Mid/Side distortion processing)
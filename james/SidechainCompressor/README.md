# Stereo Sidechain Compressor

A desktop stereo compressor with sidechain input, built around an Electrosmith Daisy Seed. A signal plugged into the sidechain input (e.g., a kick drum) drives the compression envelope, causing the stereo audio to "duck."

## Hardware Setup

### Audio Connections
- **Main Stereo In**: Left/Right Audio Input pins.
- **Main Stereo Out**: Left/Right Audio Output pins.
- **Sidechain In**: Pin **D25 (A10)**. *Requires conditioning circuit (see build_reference.html).*

### Controls
| Control | Daisy Pin | Range |
| --- | --- | --- |
| **Release** | **D18 (A3)** | 10ms to 1000ms |
| Threshold | D15 (A0) | -60 to 0 dB (Hardcoded for now) |
| Ratio | D16 (A1) | 1:1 to 20:1 (Hardcoded for now) |
| Attack | D17 (A2) | 1ms to 300ms (Hardcoded for now) |
| Mix | D19 (A4) | 0 to 100% (Hardcoded for now) |
| Drive | D20 (A5) | 0 to 100% (Hardcoded for now) |
| Width | D21 (A6) | 0 to 200% (Hardcoded for now) |
| Output | D22 (A7) | 0 to 100% (Hardcoded for now) |

### Visual Feedback
- **Sidechain LED**: Pin **D30**. Pulses with the sidechain envelope.

## Build Reference
For complete wiring diagrams and conditioning circuit details, see [build_reference.html](./build_reference.html).

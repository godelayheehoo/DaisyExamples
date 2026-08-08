# Stereo Sidechain Compressor

A desktop stereo compressor with sidechain input, built around an Electrosmith Daisy Seed. A signal plugged into the sidechain input (e.g., a kick drum) drives the compression envelope, causing the stereo audio to "duck."

## Hardware Setup

### Audio Connections
- **Main Stereo In**: Left/Right Audio Input pins.
- **Main Stereo Out**: Left/Right Audio Output pins.
- **Sidechain In**: Pin **D25 (A10)**. *Requires conditioning circuit (see build_reference.html).*

### Controls
> [!IMPORTANT]
> **Wiring Note**: Due to a hardware wiring reversal (GND and 3V3_A swapped on the pots), all potentiometer readings are inverted in code (`1.0f - reading`). The table below reflects the corrected pin assignments and expected behavior.

| Control | Daisy Pin | Range |
| --- | --- | --- |
| **Release** | **D18 (A3)** | 10ms to 1000ms |
| Threshold | D15 (A0) | -60 to 0 dB |
| Ratio | D16 (A1) | 1:1 to 100:1, then 1:inf past 90% |
| Attack | D17 (A2) | 1ms to 300ms |
| Mix | D19 (A4) | 0 to 100% |
| Drive | D20 (A5) | 0 to 100% | Saturation Amount (or Ducking Depth in Ducker mode) |
| Sat Switch | D26 + D27 | 3 Positions | Soft Saturation / Ducker Mode (top position) / Wavefolding |
| Width | D22 (A7) | 0 to 200% (Stereo Width) |
| Output | D23 (A8) | 0 to +24 dB (Makeup Gain) |
| **Cutoff** | **D21 (A6)** | 20Hz to 20kHz (SC Filter) |
| **Footswitch** | **D13** | Latching bypass. Engaged = effect on, disengaged = clean pass-through. |
| Footswitch LED | D14 | Solid on when engaged, off when bypassed. |
| **Ramp Rate** | **D24 (A9)** | LFO rate: ~0.02 Hz (50s) to 8 Hz |
| **Ramp Button** | **D10** | Momentary. Used for LFO learn-gesture (see Ramp section below). |
| Ramp LED | D9 | Learn-mode indicator (see Ramp section below). |

### Visual Feedback
- **Footswitch LED**: Pin **D14**. Solid on when effect is engaged, off when bypassed.
- **Sidechain LED**: Pin **D30**. Pulses with the sidechain envelope (only active when effect is engaged).
- **Ramp LED**: Pin **D9**. Multi-mode indicator used during the LFO learn-gesture (see Ramp section below).

### Utility
- **Bootloader Button**: Pin **D8 (Board Pin 9)**. 
  - **Wiring**: One side of a momentary button to **D8**, the other side to **GND**.
  - **Function**: Hold this button while powering on or hitting reset to enter bootloader mode for firmware updates.

## Ramp / LFO Auto-Mapping

Any of the 9 main pots can be mapped to an internal triangle-wave LFO that sweeps the parameter between two user-set values at a user-set rate. Multiple parameters can be ramped simultaneously. State is RAM-only — power cycle clears all ramps (by design).

### How to assign a ramp (learn gesture)

1. **Hold** the Ramp Button (D10).
2. While holding, **move the target pot** (e.g. Threshold) from its desired *low* value toward its desired *high* value. The Ramp LED lights solid when the target is detected.
3. Still holding, **turn the Rate pot** (D24) to set the LFO speed. The Ramp LED starts blinking at the dialed rate as a live preview.
4. **Release** the button to commit. The LFO starts running immediately.

> [!NOTE]
> Releasing the button before finding a target (step 2) or before moving the Rate pot (step 3) cancels the gesture — nothing is committed.

### How to clear a ramp

Hold the Ramp Button and **press the Footswitch** once. This clears the most recently mapped ramp. Repeat to clear additional ramps in reverse order of assignment.

### Touching a ramped pot

If you physically move a pot that currently has an active ramp, the Ramp LED **triple-flashes** as a reminder that the pot is being overridden by the LFO. The LFO remains active — the physical pot position does not break the ramp; it must be explicitly cleared.

## Build Reference
For complete wiring diagrams and conditioning circuit details, see [build_reference.html](./build_reference.html).


# BootLoader Mode
To enter bootloader mode, hold the boot button (small blue button on front) while plugging in the usb cable.  You *may* need to unplug the 9v power supply, not sure.

On windows, use PuTTY to connect to the Daisy COM port.  

On mac, do:

```
ls /dev/cu.usbmodem*
```
then:
```
screen /dev/cu.usbmodem\<the number>
```


## Code updates:
`make`
and then 
`make program-dfu`

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

### Visual Feedback
- **Sidechain LED**: Pin **D30**. Pulses with the sidechain envelope.

### Utility
- **Bootloader Button**: Pin **D8 (Board Pin 9)**. 
  - **Wiring**: One side of a momentary button to **D8**, the other side to **GND**.
  - **Function**: Hold this button while powering on or hitting reset to enter bootloader mode for firmware updates.

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


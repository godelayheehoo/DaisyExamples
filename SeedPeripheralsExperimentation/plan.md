# Project Plan: Seed Peripherals Experimentation

## Agent Guidelines
- **Always update README.md** whenever making substantive changes to the code or hardware configuration.
- **Wiring Instructions**: When documenting wiring, only use the Daisy pin names (e.g., `A0`, `D29`, `3V3`, `AGND`). You should include voltage and ground labels (e.g., "3.3V", "Analog Ground") for clarity, but do NOT attempt to provide physical pin numbers (e.g., Pin 15, Pin 30) to avoid errors.

## Current Goal
Successfully read from a potentiometer and a 3-position toggle switch on the Daisy Seed and output their states over USB serial.

## Wiring Setup
To achieve this, the hardware needs to be wired as follows:

### Potentiometer
- **Outer leg 1**: `3V3`
- **Center leg (Wiper)**: `A0`
- **Outer leg 2**: `AGND`

### 3-Position Switch (ON-OFF-ON)
- **Outer leg 1**: `D29`
- **Center leg**: `DGND`
- **Outer leg 2**: `D30`

## Next Steps
- [x] Write code to configure `AdcChannelConfig` for the potentiometer.
- [x] Write code to configure `Switch3` for the 3-position switch.
- [x] Set up USB logging in the main loop to print the read values.
- [ ] Connect the hardware according to the wiring instructions above.
- [ ] Flash the Seed and verify the serial output.
- [ ] Integrate these inputs with DSP parameters (e.g., controlling frequency, volume, or selecting DSP modes).

# SeedPeripheralsExperimentation

## Description

This project experiments with reading hardware peripherals on the Daisy Seed, specifically a potentiometer and a 3-position toggle switch, and printing their values over USB serial.

## Wiring Instructions

### Potentiometer
- **Outer leg 1**: `3V3` (3.3V Power)
- **Center leg (Wiper)**: `A0`
- **Outer leg 2**: `AGND` (Analog Ground)

### 3-Position Switch (SPDT ON-OFF-ON)
- **Outer leg 1**: `D29`
- **Center leg**: `DGND` (Digital Ground)
- **Outer leg 2**: `D30`

*Note: The `Switch3` configuration uses internal pull-up resistors, which is why the center leg connects directly to ground.*

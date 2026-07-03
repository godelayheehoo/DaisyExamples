# Stutter Unit - Daisy Seed Glitch & Pitch Pedal

The Stutter Unit is an advanced audio glitch/stutter and pitch-shifting hardware pedal built using the Daisy Seed microcontroller. It captures line-level stereo audio into external SDRAM buffer memory and loops it at variable speeds adjusted in real time, blending it dynamically with the dry signal.

---

## Project Overview

The Stutter Unit provides a performance-oriented stutter looping effect, similar in control style to the Synthstrom Deluge. The pedal continuously monitors input tempo (via MIDI clock) or falls back to a default tempo, calculating loop lengths based on a physical 5-position rotary switch.

When the performer engages the trigger, the pedal records a stereo audio snippet matching the selected musical subdivision. It then immediately switches to looping that captured audio. Turning the rate encoder alters the loop's playback rate and pitch in real time, allowing for pitch sweeps, tape-stop effects, and high-speed glitching.

---

## Hardware Architecture & Controls

The hardware is comprised of the following key interfaces, mapped in pins.h:

1. **Rate Encoder:**
   * **Rotation:** Adjusts the loop playback rate relative to unity pitch (range: 0.25x to 4.0x).
   * **Push and Hold:** Engages the stutter effect. Recording begins immediately on press and loops continuously until released, passing the dry signal on release.
2. **Menu Encoder:**
   * **Rotation:** Scrolls through menu lists or cycles configuration values.
   * **Push:** Confirms selection, enters editing state, or commits settings (acts as Confirm/Enter).
3. **5-Position Rotary Switch:**
   * Selects the stutter loop subdivision: 1/32 note, 1/16 note, 1/8 note, 1/4 note, or 1/2 note.
4. **Wet/Dry Potentiometer:**
   * Controls the analog mix of input (dry) and stutter loop (wet) audio during active stuttering. Dry signal is fully passed when the stutter is idle or recording.
5. **Confirm (CON) Button:**
   * A standalone push button that acts as a redundant alternative to the menu encoder push.
6. **Back (BAK) Button:**
   * A standalone push button used to exit menus, cancel setting modifications, or exit the debug page.
7. **Stutter LED:**
   * A standalone indicator LED that illuminates whenever the stutter is active (recording or playing back).
8. **OLED Display:**
   * A 128x64 SH1106-driven screen communicating via I2C to show pedal status, configuration parameters, and hardware debugging lines.

---

## Operating Instructions

### Status Screen
This is the default view. It displays:
* Active play state (IDLE, REC, or PLAY).
* Live playback rate (e.g. 1.00x).
* Selected loop subdivision (e.g. 1/8 note).
* MIDI Clock status (CLK or NO CLK) and active BPM.

### Settings Menu
Press the Menu Encoder or the CON button on the Status Screen to enter BROWSE mode. The Settings Menu contains:
* **MIDI SYNC:** Set to ON (sync buffer length to incoming MIDI clock beats) or OFF (use internal free-running tempo).
* **QUANTIZE:** Enables/disables beat-quantized stutter triggers (see TODOs).
* **DEBUG:** Enters the Debug screen.

To adjust a setting, scroll to it, press the Menu Encoder or CON, rotate the encoder to change the value, and press CON/Encoder to save. Pressing BAK cancels the change and restores the previous value.
The system automatically times out and returns to the Status Screen after 5 seconds of inactivity in the browse menu.

### Debug Screen
Select DEBUG from the menu list to display raw values of all connected physical controls (potentiometer voltage, pin states of encoders, switch inputs, rotary dial position, and CON/BAK buttons). Use this screen to verify wiring. Press the BAK button, CON button, or click the menu encoder to return to the Settings Menu.

---

## Build and Compilation

The project uses the standard libDaisy toolchain.

### Prerequisites
* Arm GNU Toolchain (GCC Arm Embedded).
* Daisy Examples environment configured with libDaisy and DaisySP submodules.

### Compilation
Navigate to the project subdirectory and run:
```bash
make
```

### Flashing
Connect the Daisy Seed via USB, put it in DFU bootloader mode (hold BOOT, press RESET, release BOOT), and run:
```bash
make program-dfu
```

---

## Project Documentation

For detailed technical designs, refer to the following local markdown files:
* [hardware_build_guide.md](file:///Users/james/projects/DaisyExamples/james/Stutter/hardware_build_guide.md) - Electrical wiring schematics, pull-ups, ground routing, and checklist.
* [stutter_design_outline.md](file:///Users/james/projects/DaisyExamples/james/Stutter/stutter_design_outline.md) - Code logic layout, audio callbacks, DSP variables, and state variables.
* [menu_system_design.md](file:///Users/james/projects/DaisyExamples/james/Stutter/menu_system_design.md) - State diagrams, layout specifications, and controls mapping.

---

## TODO and Future Implementations

The following features and optimizations remain open for implementation:

1. **Buffer Fade-In/Out windowing:**
   * Implement a micro-fade envelope (5-10ms) on loop boundaries and loop initialization to eliminate clicks or pops when repeating short subdivisions. (may not want this, it sounds fine right now)
2. **MIDI Clock Out-of-Range Guarding:**
   * Clamp smoothed BPM calculations to a reasonable range (e.g. 40 to 240 BPM) to handle noisy clock sequences gracefully.
3. **Settings Flash Wear Leveling:**
   * Replace simple raw sector writes with a basic wear-leveling wrapper if settings are frequently saved to flash during performance.
4. **MIDI Controls**
   * MIDI CC controls/triggers
5. **Smart rates**
   * Set modes for rates to match either the dominant pitch in the microloop or the apparent pitch caused by the loop itself and then move in half steps when turning the rate knob.
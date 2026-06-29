# Stutter Unit — Hardware Build Guide

> **Before you start:** Download and print the official Daisy Seed pinout diagram.
> You'll be referring to it constantly.
> **https://daisy.audio/hardware/Seed/#pinout**
>
> The Daisy Seed has two rows of pins, one on each long edge of the board.
> Pins are labeled on the board itself (D0, D1, A0, A1, etc.).
> All pin names in this guide match those labels exactly.

---

## A few things to understand first

**3V3A vs DGND/3V3D:** The Daisy Seed has two ground pins and two 3.3V supply pins.
- **AGND** and **3V3A** are "analog" power — use these for potentiometers and audio-adjacent circuits. They're cleaner and quieter.
- **DGND** and **3V3D** are "digital" power — use these for the OLED, encoders, LED, and rotary switch.
- **Always connect AGND and DGND together at one point** (a common ground bus on your board). They must share the same reference, just kept physically separate to reduce noise.

**Pull-up resistors:** Several inputs (encoders, rotary switch) use the Daisy's built-in internal pull-up resistors. This means you do NOT need to add external resistors for those signals — the firmware handles it. The only external resistors you need are for the LED (1×) and the MIDI optocoupler circuit.

**Pin names vs physical numbers:** This guide uses Daisy's logical pin names (D0, A0, etc.) because those are printed on the board and used in the code. Refer to the pinout diagram for their physical location.

---

## Power

**Powering the unit:**
- Connect a USB cable to the Daisy Seed's USB connector.
- This is your only power input. No barrel jack or external regulator needed.
- The Daisy Seed generates its own 3V3A and 3V3D internally from USB 5V.

**Ground bus:**
- Run a wire from **AGND** to **DGND** to create a shared ground reference.
- Everything in this guide that connects to "GND" connects to this common ground bus.

---

## Audio I/O

The Daisy Seed has dedicated audio pins that connect directly to the onboard codec. **Do not use these pins for anything else.**

### DC-blocking capacitor (required on all 4 audio lines)

Line-level audio signals can carry a small DC offset that can damage equipment or cause a thump on power-up. A capacitor in series blocks this DC while passing audio.

**For each of the four audio lines (In L, In R, Out L, Out R), wire as follows:**

```
1/4" TRS Jack (tip)  →  [10µF electrolytic cap, + side toward jack]  →  Daisy audio pin
```

> If using electrolytic (polarized) capacitors, the **+** (positive) leg goes toward your jack, the **–** leg toward the Daisy pin. If you use film/ceramic capacitors, polarity doesn't matter.

| Connection | Daisy Pin |
|---|---|
| Audio In Left (from jack) | Audio In L |
| Audio In Right (from jack) | Audio In R |
| Audio Out Left (to jack) | Audio Out L |
| Audio Out Right (to jack) | Audio Out R |

**Jack sleeve (ground):** Connect the sleeve terminal of all 1/4" jacks to **AGND** (not DGND).

---

## Wet/Dry Potentiometer

The wet/dry pot has three legs:
- **Leg 1** (one end of the resistive track) → **AGND**
- **Leg 3** (other end of the resistive track) → **3V3A**
- **Leg 2** (middle leg / wiper) → **A0** (also labeled D15 on the board)

It doesn't matter which physical leg is which as long as the two "end" legs go to GND and 3V3A — if your knob turns the wrong way, just swap legs 1 and 3.

| Control | Daisy Pin (wiper) |
|---|---|
| Wet/Dry | **A0** (also labeled D15) |

---

## 5-Position Rotary Switch (Loop Length)

A rotary switch has one **common** terminal and one terminal per position.

**Wiring:**
- **Common terminal** → **GND**
- **Position 1 terminal** → **D4**
- **Position 2 terminal** → **D5**
- **Position 3 terminal** → **D6**
- **Position 4 terminal** → **D7**
- **Position 5 terminal** → **D8**

How it works: when the knob is at position 1, it connects D4 to GND. The firmware reads D4 as LOW and knows that's position 1. All other pins float HIGH thanks to internal pull-up resistors. No external resistors needed.

| Position | Daisy Pin | Subdivision |
|---|---|---|
| 1 | D4 | 1/32 note |
| 2 | D5 | 1/16 note |
| 3 | D6 | 1/8 note |
| 4 | D7 | 1/4 note |
| 5 | D8 | 1/2 note |

---

## Rate Encoder (Stutter Rate + Trigger)

This is the primary performance control. It does two things:
- **Turning it** adjusts the stutter playback rate (and therefore pitch). Rate starts at 1.0× on every power-up. Turning clockwise increases rate (higher pitch); counterclockwise decreases rate (lower pitch).
- **Pressing and holding it** engages the stutter — exactly like the Synthstrom Deluge stutter knob. Release to stop.

A rotary encoder has 5 pins: A, B, C (common/ground), and two for the push button (SW, GND).

### Rotation contacts
- **A** → **D9**
- **B** → **D10**
- **C** (common, often labeled GND on the encoder) → **GND**

### Push button contacts
- **SW** → **D11**
- **Other SW terminal** → **GND**

No external resistors needed. All three signal pins (D9, D10, D11) use internal pull-ups.

| Signal | Daisy Pin | Notes |
|---|---|---|
| Rate encoder A | **D9** | Quadrature input |
| Rate encoder B | **D10** | Quadrature input |
| Rate encoder push (= stutter trigger) | **D11** | Active LOW; hold to stutter |

---

## Menu Encoder (Menu Navigation)

A second rotary encoder, used only for navigating the OLED menu. Same wiring pattern as the rate encoder.

### Rotation contacts
- **A** → **D1**
- **B** → **D2**
- **C** (common) → **GND**

### Push button contacts
- **SW** → **D3**
- **Other SW terminal** → **GND**

| Signal | Daisy Pin |
|---|---|
| Menu encoder A | **D1** |
| Menu encoder B | **D2** |
| Menu encoder push | **D3** |

---

## Confirm (CON) and Back (BAK) Buttons

Two standalone push buttons used for direct navigation and editing functions:
*   **CON (Confirm)** button: Behaves identically to pushing the menu encoder button. Used to enter menus or save edited values.
*   **BAK (Back)** button: Used to exit screens, cancel edits, or go back to the previous screen.

### Wiring:
For each button, connect one terminal to the Daisy Seed pin and the other terminal to the digital ground (**GND**). No external pull-up resistors are required as the firmware enables internal pull-ups on these pins.

| Button | Daisy Pin | Notes |
|---|---|---|
| Back (BAK) | **D25** | Active LOW; exits/cancels |
| Confirm (CON) | **D26** | Active LOW; enters/saves |

---

## Stutter Indicator LED

A standalone LED (not in a button) that lights up whenever the stutter is active.

```
D12  →  [220Ω resistor]  →  LED Anode (+)
                             LED Cathode (–)  →  GND
```

> The 220Ω resistor limits current through the LED. Without it you risk burning out the LED or the Daisy's GPIO pin. If the LED seems dim, try 100Ω; if too bright, try 470Ω.

| Signal | Daisy Pin |
|---|---|
| Stutter LED | **D12** — HIGH = on |

---

## OLED Display (SSD1306, I2C, 1.3")

Most SSD1306 breakout boards have 4 pins: VCC, GND, SCL, SDA.

| Display Pin | Connects To |
|---|---|
| VCC | **3V3D** |
| GND | **DGND** |
| SCL | **D13** |
| SDA | **D14** |

> The I2C pins D13 and D14 are the only pins on the Daisy Seed capable of I2C1. They are fixed — you cannot use different pins for I2C1.

> **Pull-up resistors:** Most SSD1306 breakout boards include these onboard. Check your specific board; if it has surface-mount resistors near the SDA/SCL pads, you're covered. If not, add 4.7kΩ resistors from SDA and SCL to 3V3D.

---

## MIDI Input (DIN-5)

MIDI uses an optocoupler for electrical isolation. The standard part is a **6N138** or **PC900V**. Wire as follows:

### MIDI DIN-5 socket
A DIN-5 socket has 5 pins. For MIDI In, only pins 2, 4, and 5 are used:
- **Pin 2** (shield) → **GND**
- **Pin 4** → via **220Ω resistor** → **optocoupler pin 3** (anode of LED)
- **Pin 5** → **optocoupler pin 2** (cathode of LED)

### Optocoupler circuit

```
                        ┌─────────────────────┐
DIN pin 4 ──[220Ω]──── │ Pin 3 (anode)        │
DIN pin 5 ───────────── │ Pin 2 (cathode)      │  6N138
                        │                     │
3V3D ──[10kΩ]────────── │ Pin 6 (output)       │──────── D0 (Daisy MIDI RX)
                        │ Pin 5 (GND)          │────────── DGND
                        └─────────────────────┘
```

**More plainly:**
1. DIN socket pin 4 → 220Ω resistor → optocoupler pin 3
2. DIN socket pin 5 → optocoupler pin 2
3. Optocoupler pin 5 → DGND
4. Optocoupler pin 6 → 10kΩ resistor → 3V3D (pull-up)
5. Optocoupler pin 6 → **D0** (MIDI signal to Daisy)

> **D0 carries UART5_RX**, which is the UART the firmware uses for MIDI. Do not use D0 for anything else.

> If using a **6N138** specifically, also connect a 1N4148 diode across the input LED pins (cathode toward pin 3, anode toward pin 2) to protect against reverse voltage. PC900V does not require this diode.

| MIDI Signal | Daisy Pin |
|---|---|
| MIDI RX | **D0** (UART5_RX) |

---

## Complete Pin Assignment Summary

| Function | Daisy Pin | Direction | Notes |
|---|---|---|---|
| MIDI In | D0 | Input | Via optocoupler; UART5_RX |
| Menu encoder A | D1 | Input | Internal pull-up |
| Menu encoder B | D2 | Input | Internal pull-up |
| Menu encoder push | D3 | Input | Internal pull-up; active LOW |
| Rotary switch pos 1 (1/32) | D4 | Input | Internal pull-up; active LOW |
| Rotary switch pos 2 (1/16) | D5 | Input | Internal pull-up; active LOW |
| Rotary switch pos 3 (1/8) | D6 | Input | Internal pull-up; active LOW |
| Rotary switch pos 4 (1/4) | D7 | Input | Internal pull-up; active LOW |
| Rotary switch pos 5 (1/2) | D8 | Input | Internal pull-up; active LOW |
| Rate encoder A | D9 | Input | Internal pull-up |
| Rate encoder B | D10 | Input | Internal pull-up |
| Rate encoder push (stutter trigger) | D11 | Input | Internal pull-up; active LOW; hold to stutter |
| Stutter indicator LED | D12 | Output | HIGH = on; 220Ω series resistor to LED anode |
| OLED SCL | D13 | I2C | Fixed I2C1 SCL pin |
| OLED SDA | D14 | I2C | Fixed I2C1 SDA pin |
| Wet/Dry pot | A0 / D15 | Analog in | Wiper of pot; ends to AGND and 3V3A |
| Back button (BAK) | D25 | Input | Internal pull-up; active LOW |
| Confirm button (CON) | D26 | Input | Internal pull-up; active LOW |
| Audio In L | Audio In L | Analog in | Via 10µF DC-block cap; jack sleeve to AGND |
| Audio In R | Audio In R | Analog in | Via 10µF DC-block cap; jack sleeve to AGND |
| Audio Out L | Audio Out L | Analog out | Via 10µF DC-block cap; jack sleeve to AGND |
| Audio Out R | Audio Out R | Analog out | Via 10µF DC-block cap; jack sleeve to AGND |

---

## Checklist Before Powering On

- [ ] AGND and DGND are bridged together at one point
- [ ] Wet/dry pot end-legs go to AGND / 3V3A (not DGND / 3V3D)
- [ ] All audio jack sleeves go to AGND
- [ ] DC-blocking caps are in series on all 4 audio lines; polarized caps oriented correctly
- [ ] LED has a series resistor (220Ω) between D12 and the anode
- [ ] Optocoupler circuit wired correctly; 220Ω resistor on MIDI DIN pin 4
- [ ] OLED VCC goes to 3V3D, not 3V3A
- [ ] No I2C pins (D13/D14) used for anything else
- [ ] D0 is reserved for MIDI only
- [ ] Rotary switch common goes to GND (not 3V3)
- [ ] Both encoder common/C pins go to GND (not 3V3)
- [ ] Rate encoder push (D11) is the stutter trigger — no separate trigger button
- [ ] Confirm and Back button ground terminals are connected to GND

---

## Corresponding Firmware File

All pin assignments above are mirrored in `pins.h`. If you change any wire, update `pins.h` to match. The rest of the firmware reads only from `pins.h` — you should never need to edit pin numbers anywhere else.

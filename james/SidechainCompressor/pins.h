#ifndef PINS_H
#define PINS_H

// Bootloader button pin, also will be used for clearing ramping
constexpr int PIN_CLEAR_SW = 8; // D8 (board pin 9)

// ADC Potentiometers & Inputs
constexpr int PIN_SC_INPUT      = 25; // A10 / D25 (board pin 32)
constexpr int PIN_FILTER_CUTOFF = 21; // A6  / D21 (board pin 28)
constexpr int PIN_THRESHOLD_POT = 15; // A0  / D15 (board pin 22)
constexpr int PIN_RATIO_POT     = 16; // A1  / D16 (board pin 23)
constexpr int PIN_ATTACK_POT    = 17; // A2  / D17 (board pin 24)
constexpr int PIN_RELEASE_POT   = 18; // A3  / D18 (board pin 25)
constexpr int PIN_MIX_POT       = 19; // A4  / D19 (board pin 26)
constexpr int PIN_DRIVE_POT     = 20; // A5  / D20 (board pin 27)
constexpr int PIN_WIDTH_POT     = 22; // A7  / D22 (board pin 29)
constexpr int PIN_OUTPUT_POT    = 23; // A8  / D23 (board pin 30)
// Unused Potentiometer
constexpr int PIN_UNUSED_POT = 24; // A9  / D24 (board pin 31)

// Unused Latching Button
constexpr int PIN_UNUSED_BUTTON_LED = 9;  // D9 (board pin 10)
constexpr int PIN_UNUSED_BUTTON     = 10; // D10 (board pin 11)

// Saturation Mode Toggle Switch
constexpr int PIN_SAT_SW1 = 26; // D26 (board pin 33)
constexpr int PIN_SAT_SW2 = 27; // D27 (board pin 34)

// Filter Mode Toggle Switch
constexpr int PIN_FILTER_SW1 = 11; // D11 (board pin 12)
constexpr int PIN_FILTER_SW2 = 12; // D12 (board pin 13)

// Status LED
constexpr int PIN_LED = 30; // D30 (board pin 37)

// Enable/Disable
constexpr int PIN_FOOTSWITCH     = 13; // D13 (board pin 14)
constexpr int PIN_LED_FOOTSWITCH = 14; // D14 (board pin 15)

#endif // PINS_H

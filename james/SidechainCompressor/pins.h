#ifndef PINS_H
#define PINS_H

// Bootloader button pin
constexpr int PIN_BOOTLOADER_SW = 8; // D8 (board pin 9)

// ADC Potentiometers & Inputs
constexpr int PIN_SC_INPUT      = 25; // A10 / D25
constexpr int PIN_FILTER_CUTOFF = 21; // A6  / D21
constexpr int PIN_THRESHOLD_POT = 15; // A0  / D15
constexpr int PIN_RATIO_POT     = 16; // A1  / D16
constexpr int PIN_ATTACK_POT    = 17; // A2  / D17
constexpr int PIN_RELEASE_POT   = 18; // A3  / D18
constexpr int PIN_MIX_POT       = 19; // A4  / D19
constexpr int PIN_DRIVE_POT     = 20; // A5  / D20
constexpr int PIN_WIDTH_POT     = 22; // A7  / D22
constexpr int PIN_OUTPUT_POT    = 23; // A8  / D23
constexpr int PIN_UNUSED_POT    = 24; // A9  / D24

// Saturation Mode Toggle Switch
constexpr int PIN_SAT_SW1 = 26; // D26
constexpr int PIN_SAT_SW2 = 27; // D27

// Filter Mode Toggle Switch
constexpr int PIN_FILTER_SW1 = 11; // D11
constexpr int PIN_FILTER_SW2 = 12; // D12

// Status LED
constexpr int PIN_LED = 30; // D30

// Enable/Disable
constexpr int PIN_FOOTSWITCH     = 13; // D13
constexpr int PIN_LED_FOOTSWITCH = 14; // D14

#endif // PINS_H

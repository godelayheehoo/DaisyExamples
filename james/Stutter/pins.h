#pragma once
#include "daisy_seed.h"

// =============================================================================
// pins.h — Stutter Unit Hardware Pin Definitions
// =============================================================================
// All hardware connections are defined here. If you change a physical wire,
// change it here only. The rest of the firmware reads from these definitions.
//
// Pin names use the daisy::seed:: namespace (e.g. daisy::seed::D0).
// These match the labels printed on the Daisy Seed board.
//
// REFERENCE: https://daisy.audio/hardware/Seed/#pinout
// =============================================================================

namespace StutterPins
{

// -------------------------------------------------------------------------
// MIDI INPUT (DIN-5, UART5)
// D0 carries UART5_RX. This pin is used for MIDI input.
// NOTE: D14 also carries UART1_RX but is used for I2C1_SDA (OLED).
//       UART1 cannot be used. UART5 on D0 has no such conflict.
// -------------------------------------------------------------------------
constexpr daisy::Pin MIDI_RX = daisy::seed::D0; // UART5_RX

// -------------------------------------------------------------------------
// MENU ENCODER (rotary encoder with push — menu navigation only)
// ENC_A and ENC_B: configure as INPUT with PULLUP.
// ENC_SW: configure as INPUT with PULLUP. Reads LOW when pressed.
// -------------------------------------------------------------------------
constexpr daisy::Pin MENU_ENC_A  = daisy::seed::D1;
constexpr daisy::Pin MENU_ENC_B  = daisy::seed::D2;
constexpr daisy::Pin MENU_ENC_SW = daisy::seed::D3; // active LOW

// -------------------------------------------------------------------------
// ROTARY SWITCH (5-position loop length selector)
// Wired one-hot: common → GND, each position terminal → one GPIO.
// Configure each pin as INPUT with PULLUP.
// The active position reads LOW; all others read HIGH.
// -------------------------------------------------------------------------
constexpr daisy::Pin ROT_POS_1 = daisy::seed::D4; // 1/32 note
constexpr daisy::Pin ROT_POS_2 = daisy::seed::D5; // 1/16 note
constexpr daisy::Pin ROT_POS_3 = daisy::seed::D6; // 1/8  note
constexpr daisy::Pin ROT_POS_4 = daisy::seed::D7; // 1/4  note
constexpr daisy::Pin ROT_POS_5 = daisy::seed::D8; // 1/2  note

// -------------------------------------------------------------------------
// RATE ENCODER (rotary encoder with push — rate control AND stutter trigger)
// Rotation: adjusts playback rate (relative, initializes to 1.0 on boot).
// Push and hold: engages stutter (trigger active while held; active LOW).
// This is the same dual-purpose design as the Synthstrom Deluge stutter knob.
// -------------------------------------------------------------------------
constexpr daisy::Pin RATE_ENC_A = daisy::seed::D9;
constexpr daisy::Pin RATE_ENC_B = daisy::seed::D10;
constexpr daisy::Pin RATE_ENC_SW
    = daisy::seed::D11; // push = stutter trigger; active LOW

// -------------------------------------------------------------------------
// STUTTER INDICATOR LED (standalone LED, not a button)
// Configure as OUTPUT. Write HIGH to illuminate, LOW to turn off.
// Wire: Daisy pin → 220Ω resistor → LED anode. LED cathode → GND.
// LED is ON while stutter is active (RECORDING or PLAYING state).
// -------------------------------------------------------------------------
constexpr daisy::Pin STUTTER_LED = daisy::seed::D12;

// -------------------------------------------------------------------------
// OLED DISPLAY (SSD1306, I2C1)
// D13 and D14 are the only pins capable of I2C1 on the Daisy Seed.
// They are not configurable — use them as-is.
// Most SSD1306 breakout boards include pull-up resistors.
// If yours does not, add 4.7kΩ from each pin to 3V3D.
// -------------------------------------------------------------------------
constexpr daisy::Pin OLED_SCL = daisy::seed::D13; // I2C1 SCL (fixed)
constexpr daisy::Pin OLED_SDA = daisy::seed::D14; // I2C1 SDA (fixed)

// -------------------------------------------------------------------------
// ANALOG INPUT (potentiometer)
// Only one ADC channel is needed: wet/dry blend.
// Rate is now controlled by the rate encoder (relative, not analog).
// -------------------------------------------------------------------------
constexpr daisy::Pin WET_DRY_POT = daisy::seed::A0; // D15 — Wet/Dry knob wiper

} // namespace StutterPins


// =============================================================================
// RATE ENCODER CONSTANTS
// =============================================================================
constexpr float RATE_STEP = 0.05f; // change per encoder detent; tune to taste
constexpr float RATE_MIN  = 0.25f; // lower bound (~2 octaves down)
constexpr float RATE_MAX  = 4.0f;  // upper bound (~2 octaves up)
constexpr float RATE_INIT = 1.0f;  // always start at unity on boot


// =============================================================================
// ADC CHANNEL INDEX ENUM
// The order here must match the order you call adc_config[n].InitSingle(pin)
// in your hardware initialization code.
// =============================================================================
enum AdcChannel
{
    ADC_WET_DRY_POT = 0,
    NUM_ADC_CHANNELS
};

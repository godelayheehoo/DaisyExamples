# Stutter Unit — Beta Tester Notes & Feedback Guide

Welcome to the beta test for the **Stutter Unit**! This guide outlines newly implemented control features, explains how to test them, and collects feedback on parameter tuning and hardware ergonomics.

---

## 1. Overview of New Hardware Controls

We recently updated the front panel control scheme to provide faster live performance gestures and redundant navigation options:

1. **Menu Encoder Push Button (`MENU_ENC_SW`) Re-enabled:**
   - The push switch inside the Menu rotary encoder now works as a redundant **Confirm / Enter** key alongside the standalone **CON** button.
   - A rotation filter has been added so scrolling through menu items will not accidentally trigger clicks or push you into edit mode.

2. **Back (`BAK`) Button Dual-Functionality on the Status Screen:**
   - **Instant Rate Reset (Tap):** When on the main Status screen, simply tapping (pressing and releasing) the Back button snaps the stutter loop playback rate immediately back to unity (**1.00x** speed, or **0 semitones** in quantized rate modes).
   - **Quantized Mix Snapping (Hold + Turn Mix Knob):** When on the main Status screen, holding the Back button while turning the **Wet/Dry Mix** knob snaps the mix to discrete **25% steps** (0%, 25%, 50%, 75%, 100% wet) rather than continuous floating percentages.
   - **In Menus:** When browsing or editing in the Settings Menu, the Back button continues to act strictly as **Back / Cancel** without affecting the playback rate or mix.

---

## 2. Test Workflows for Beta Testers

### Test A: Stutter Rate Quick Reset (Tap Back)
1. Turn on the unit and ensure you are on the default **STATUS** screen.
2. Turn the **Rate Encoder** to sweep the loop playback speed (e.g., up to 2.50x or pitch-shifted up several semitones).
3. Tap the **BAK** button once (press and quickly release without turning the Mix knob).
4. **Expected Result:** The playback rate instantly jumps back to `RATE: 1.00x` (or `+0st`). The OLED updates immediately.

### Test B: Quantized Mix Steps (Hold Back + Turn Mix)
1. On the **STATUS** screen, hold down the **BAK** button.
2. While keeping **BAK** held, turn the **WET/DRY** mix knob.
3. Observe the `WET:` readout on the bottom row of the OLED.
4. **Expected Result:** The mix reading steps cleanly in 25% increments: `0.00`, `0.25`, `0.50`, `0.75`, `1.00`.
5. Release the **BAK** button. The mix should remain locked at that exact step.
6. Now turn the **WET/DRY** knob again without holding **BAK**. Continuous mix control should resume smoothly.

### Test C: Menu Encoder Push vs. Rotation Guard
1. Scroll through the Settings Menu quickly by turning the **Menu Encoder** clockwise and counterclockwise.
2. **Expected Result:** The cursor moves smoothly across menu items without accidentally selecting an item or jumping into edit mode during turns.
3. Stop on an item (e.g., `QUANTIZE`) and push down firmly on the Menu Encoder.
4. **Expected Result:** It enters edit mode just like pressing the **CON** button.

### Test D: Safety Isolation in Settings Menus
1. From the Status screen, press **CON** to enter the Settings Menu (`BROWSE` state).
2. Tap the **BAK** button.
3. **Expected Result:** You return to the Status screen. The playback rate should **not** reset if it was previously set to an altered rate.

---

## 3. Questions

Please provide your impressions and feedback on the following questions:

### A. Mix Knob Movement Threshold (Tuning Constant: `0.5`)
> *In plain English: When you hold down the Back button, the pedal requires you to turn the Mix knob across at least half (50%) of its physical rotation before it decides you are intentionally adjusting the mix (to distinguish a deliberate adjustment from knob jitter or accidentally bumping the knob while pressing the button).*

1. **Does requiring 50% knob travel feel too wide or difficult to activate?**
   - For example, if your knob is already set at 50% wet, did you find it awkward to have to sweep all the way to 0% or 100% just to activate the quantized snapping?
2. **What would feel more natural?**
   - Would a much smaller threshold—such as 10% or 15% of the dial's rotation—feel significantly more responsive while still preventing accidental mix shifts when you just want to tap the button?
3. **Did you ever experience accidental mix changes when you only intended to tap the Back button to reset the rate?**

### B. Mix Quantization Step Size (Tuning Constant: `0.25`)
> *In plain English: Quantized mix mode divides the knob into 5 fixed positions: 0% (100% dry), 25%, 50% (equal blend), 75%, and 100% (100% wet loop).*

1. **Do 25% steps feel musically useful for live playing?**
   - Do you find yourself using the 25%, 50%, and 75% blend points, or would you prefer a different step size (for instance, 20% steps [0, 20, 40, 60, 80, 100%] or 33% steps [0, 33, 66, 100%])?
2. **Is it easy to land on your intended percentage without overshooting?**

### C. Rate Reset Ergonomics
1. **How does the rate reset tap feel during active play?**
   - When glitching audio and performing live pitch sweeps, does tapping the Back button feel fast and responsive?
   - Did you ever have a tap fail to register, or notice any unwanted delay?

### D. Parameter Latching / "Unlatching" Behavior
1. **When you release the Back button after snapping to a mix value (e.g., 50%), the pedal holds that value until you turn the knob again.**
   - Does this latching behavior feel intuitive?
   - When you turn the knob again to return to continuous control, did the transition feel smooth, or did you notice any jarring audio jump?

### E. Navigation Buttons & Physical Layout
1. **Now that both the Menu knob push and the dedicated green CON button confirm selections, which one do you naturally find yourself using?**
   - Does having both options feel useful, or do you strongly prefer one over the other?
2. **Ergonomics of the two-finger gesture:**
   - Holding the red **BAK** button with one finger while turning the **WET/DRY** knob with your other fingers—does this feel comfortable on the pedal enclosure, or does your hand feel crowded?
3. **Menu Encoder click feel:**
   - Does the Menu Encoder push feel solid and distinct when you intentionally click it? Did you experience any false triggers while rotating the knob?

### F. General Feedback & Bug Reports
1. Did you notice any audio clicks, pops, or glitching when resetting rate or snapping mix values?
2. Are there any other shortcut gestures or button combinations you would find helpful in live performance?

#include "daisy_pod.h"
#include "daisy_seed.h"

#include <stdio.h>

#define WRITE_USB 1
#include <string.h>
#include <cmath>   // for fabs
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
static Parameter p_knob1, p_knob2;

// New boolean for both buttons pressed
bool both_pressed = false;

// Reverb effect
#include "Effects/reverbsc.h"
ReverbSc reverb;
bool reverb_on = false;

// Encoder stutter offset
int stutter_offset = 0;

// --- Stutter buffer globals ---
constexpr size_t STUTTER_BUF_LEN = 8000; // 0.5s at 16kHz, ~64KB for stereo
float stutter_buf[2][STUTTER_BUF_LEN]; // stereo buffer
size_t stutter_write_pos = 0;
size_t stutter_play_pos = 0;
bool stutter_recording = true;
bool stutter_active = false;
Metro metro_tick;
bool trip = false;
bool play_reverse = false;
float knob_1_val = 0.5f; // Default knob value
float knob_2_val = 0.5f; // Default knob2 value


float r = 0.0, g = 0.0, b = 0.9;
volatile int encoder_total = 0;
int main_tick = 0;
bool metro_val = false;
char buff[128];

// Global sample rate for use in AudioCallback and main
float sample_rate = 48000.0f;


static void AudioCallback(AudioHandle::InputBuffer in,
                         AudioHandle::OutputBuffer out,
                         size_t size)
{
    hw.ProcessAllControls(); // Update all knobs, encoders, and buttons
    // hw.encoder.Debounce(); // may not be needed with ProcessAllControls

    int inc = hw.encoder.Increment();
    encoder_total += inc;
    encoder_total = encoder_total % 360; // Wrap around to keep within 0-359 degrees

    // Toggle play direction on encoder button press (RisingEdge)
    static bool last_encoder_pressed = false;
    bool encoder_pressed = hw.encoder.Pressed();
    if(encoder_pressed && !last_encoder_pressed) {
        play_reverse = !play_reverse;
    }
    last_encoder_pressed = encoder_pressed;


    bool button1_pressed = hw.button1.Pressed();
    bool button2_pressed = hw.button2.Pressed();

    // Detect rising edge for both buttons pressed simultaneously
    static bool last_both_pressed = false;
    bool now_both_pressed = button1_pressed && button2_pressed;
    if(now_both_pressed && !last_both_pressed) {
        both_pressed = !both_pressed;
    }
    last_both_pressed = now_both_pressed;

    // Detect rising edge for button2 to toggle reverb
    static bool last_button2_pressed = false;
    if(button2_pressed && !last_button2_pressed) {
        reverb_on = !reverb_on;
    }
    last_button2_pressed = button2_pressed;

    // Only allow stutter offset when stuttering (button1 pressed)
    if(button1_pressed && inc != 0) {
        // Each encoder tick rotates by 0.05s (0.05 * 48000 = 2400 samples)
        stutter_offset += 2400 * inc;
        // Wrap offset to buffer size
        while(stutter_offset < 0) stutter_offset += STUTTER_BUF_LEN;
        while(stutter_offset >= (int)STUTTER_BUF_LEN) stutter_offset -= STUTTER_BUF_LEN;
    }

    // ...existing code...
    static size_t stutter_play_pos = 0;

    for(size_t i = 0; i < size; i++)
    {
        knob_1_val = p_knob1.Process(); // Update knob1 value
        knob_2_val = p_knob2.Process(); // Update knob2 value

        // Calculate stutter length (linear mapping between 0.01s and 0.5s)
        float min_s = 0.01f, max_s = 0.5f;
        float stutter_len_s = min_s + (max_s - min_s) * knob_2_val;
        size_t stutter_len = (size_t)(stutter_len_s * sample_rate);
        if(stutter_len < 1) stutter_len = 1;
        if(stutter_len > STUTTER_BUF_LEN) stutter_len = STUTTER_BUF_LEN;

        float dry_l, dry_r;
        if(!button1_pressed) {
            // Record rolling buffer and passthrough
            stutter_buf[0][stutter_write_pos] = in[0][i];
            stutter_buf[1][stutter_write_pos] = in[1][i];
            dry_l = in[0][i];
            dry_r = in[1][i];
            stutter_write_pos++;
            if(stutter_write_pos >= STUTTER_BUF_LEN) stutter_write_pos = 0;
            // Reset play position to the start of the most recent buffer
            stutter_play_pos = stutter_write_pos;
            stutter_offset = 0; // Reset offset when not stuttering
            hw.seed.SetLed(false);
        } else {
            // Mix stuttered and live input, blend set by knob 1
            // Wrap play position and offset within stutter_len
            size_t stutter_start = (stutter_write_pos + STUTTER_BUF_LEN - stutter_len) % STUTTER_BUF_LEN;
            size_t rel_play_pos = (stutter_play_pos - stutter_start + stutter_len) % stutter_len;
            size_t play_idx = (stutter_start + (rel_play_pos + stutter_offset) % stutter_len) % STUTTER_BUF_LEN;
            dry_l = knob_1_val * stutter_buf[0][play_idx] + (1.0f - knob_1_val) * in[0][i];
            dry_r = knob_1_val * stutter_buf[1][play_idx] + (1.0f - knob_1_val) * in[1][i];
            if(play_reverse) {
                if(stutter_play_pos == 0) stutter_play_pos = STUTTER_BUF_LEN - 1;
                else stutter_play_pos--;
            } else {
                stutter_play_pos++;
                if(stutter_play_pos >= STUTTER_BUF_LEN) stutter_play_pos = 0;
            }
            hw.seed.SetLed(true);
        }

        float wet_l = dry_l, wet_r = dry_r;
        if(reverb_on) {
            reverb.Process(dry_l, dry_r, &wet_l, &wet_r);
        }
        out[0][i] = wet_l;
        out[1][i] = wet_r;

        //diagnostic stuff.
        r = knob_1_val; // Always update knob1 value
        metro_val = metro_tick.Process();
        #if WRITE_USB
        if(metro_val){
            trip = true;
            main_tick += 1;
            sprintf(buff, "Encoder:\t%d\tKnob1:\t%d\tStutterLen: %d\tReverse: %d\tBothPressed: %d\tMainTick: %d\r\n",
                encoder_total, (int)(r*1000), (int)stutter_len, (int)play_reverse, (int)both_pressed, main_tick);
            hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
        }
        #endif
    }
    hw.led1.Set(r, 1-r, b); // Update LED1 color based on knob value (once per block)
    hw.led2.Set(fabs(encoder_total/360.0f), fabs(encoder_total/360.0f), fabs(encoder_total/360.0f)); // Update LED2 based on encoder position (once per block)
    hw.UpdateLeds();
}

int main(void)
{
    hw.Init();
#if WRITE_USB
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
#endif

    // Set sample rate to 16kHz for longer buffer
    sample_rate = 16000.0f;
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_16KHZ);
    metro_tick.Init(2.0f, sample_rate);

    // Init reverb
    reverb.Init(sample_rate);
    reverb.SetFeedback(0.85f); // moderate feedback
    reverb.SetLpFreq(18000.0f); // bright

    //blink the LED on startup to show the program is running.
    hw.led1.Set(0.0f, 0.0f, 1.0f); // Set LED 1 
    hw.led2.Set(1.0f, 0.0f, 0.0f); // Set LED 2
    System::Delay(500);
    hw.UpdateLeds();
    System::Delay(2000);
    hw.led1.Set(0.0f, 1.0f, 1.0f); // Set LED 1 
    hw.UpdateLeds();
    System::Delay(500);
    hw.led1.Set(0.0f, 1.0f, 0.0f); // Set LED 1     
    hw.UpdateLeds();

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);

    hw.StartAudio(AudioCallback);
    
    hw.StartAdc();

    while(1)
    {
        // Main loop intentionally left empty to avoid blocking audio callback
    }
}

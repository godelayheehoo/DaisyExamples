#include "daisy_pod.h"
#include "daisy_seed.h"

#include <stdio.h>
#include <string.h>
#include <cmath>   // for fabs
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
static Parameter p_knob1, p_knob2;
Metro metro_tick;
bool trip = false;

float r = 0.0, g = 0.0, b = 0.9;
volatile int encoder_total = 0;
int main_tick = 0;
bool metro_val = false;
char buff[128];


static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                         AudioHandle::InterleavingOutputBuffer out,
                         size_t size)
{
    hw.encoder.Debounce();
    int inc = hw.encoder.Increment();
    encoder_total += inc;
    encoder_total = encoder_total % 360; // Wrap around to keep within 0-359 degrees
    hw.seed.SetLed(true); // Set the built-in LED to indicate activity
    for(size_t i = 0; i < size; i++)
    {
        r = p_knob1.Process(); // Always update knob1 value
        metro_val = metro_tick.Process();
        if(metro_val){
            trip = true;
            main_tick += 1;
            sprintf(buff, "Encoder:\t%d\tKnob1:\t%d\tMainTick: %d\tTrip: %d\r\n", encoder_total, (int)(r*1000), main_tick, (int)trip);
            hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
        }
    }
    hw.led1.Set(r, 1-r, b); // Update LED1 color based on knob value (once per block)
    hw.led2.Set(fabs(encoder_total/360.0f), fabs(encoder_total/360.0f), fabs(encoder_total/360.0f)); // Update LED2 based on encoder position (once per block)
    hw.UpdateLeds();
}

int main(void)
{
    hw.Init();
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);

    float sample_rate = hw.AudioSampleRate();
    metro_tick.Init(2.0f, sample_rate);

    //blink the LED on startup to show the program is running.
    hw.led1.Set(0.0f, 0.0f, 1.0f); // Set LED 1 
    hw.led2.Set(1.0f, 0.0f, 0.0f); // Set LED 2
    System::Delay(2000);
    hw.UpdateLeds();
    System::Delay(2000);
    hw.led1.Set(0.0f, 1.0f, 1.0f); // Set LED 1 
    hw.UpdateLeds();
    System::Delay(500);
    hw.led1.Set(0.0f, 1.0f, 0.0f); // Set LED 1     
    hw.UpdateLeds();


    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);

    hw.StartAudio(AudioCallback);
    
    hw.StartAdc();


    while(1)
    {
        // Main loop intentionally left empty to avoid blocking audio callback
    }
}



/////////////////////// end  of main.cpp /////////////////


// #include "daisy_pod.h"
// // #include "daisysp.h"
// // #include "../../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.h"
// // #include "Effects/reverbsc.h"

// using namespace daisy;
// // using namespace daisysp;


// DaisyPod  hw;
// // Parameter p_knob1, p_knob2;


// // Glitch buffer settings (commented out for minimal encoder test)
// // constexpr size_t GLITCH_BUF_SIZE = 48000; // 1 second at 48kHz
// // float glitch_buf[GLITCH_BUF_SIZE];
// // size_t glitch_write_pos = 0;
// // size_t glitch_play_pos = 0;
// // bool glitch_active = false;
// // size_t glitch_len = 2400; // default 50ms at 48kHz
// // int glitch_offset = 0; // Offset for rotating buffer
// // float led2_r = 0.3f, led2_g = 0.7f, led2_b = 1.0f; // LED2 color globals

// static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
//                    AudioHandle::InterleavingOutputBuffer out,
//                    size_t size)
// {
//     // Minimal encoder test: only update LED2 color based on encoder movement
//     hw.encoder.Debounce();
//     int inc = hw.encoder.Increment();
//     if(inc > 0)
//     {
//         hw.led2.Set(1.0f, 0.0f, 0.0f); // Red
//     }
//     else if(inc < 0)
//     {
//         hw.led2.Set(0.0f, 0.0f, 1.0f); // Blue
//     }
//     else
//     {
//         hw.led2.Set(0.0f, 1.0f, 0.0f); // Green
//     }
//     hw.UpdateLeds();
//     // (No audio processing)
// }


// void BlinkBlueNTimes(int n)
// {
//     for(int i = 0; i < n; i++)
//     {
//         hw.led1.Set(0.0f, 0.0f, 1.0f); // Blue
//         System::Delay(500);
//         hw.UpdateLeds();

//         hw.led1.Set(1.0f, 0.0f, 0.0f); // Red
//         System::Delay(500);
//         hw.UpdateLeds();
//     }
// }


// int main(void)

// {
//     hw.Init();
//     // hw.StartAdc();

//     // float sample_rate = hw.AudioSampleRate(); 
//     // hw.SetAudioBlockSize(4); // Set block size to 4 samples
    
//     //LED stuff
//     // hw.led1.Set(1.0f,0.0f,0.0f); // Set LED 1 to red
//     // hw.led2.Set(0.3f, 0.7f, 1.0f);
//     // hw.UpdateLeds();
//     // System::Delay(100); // Wait for 100 milliseconds
//     // BlinkBlueNTimes(2); // Blink blue 2 times
//     // float r = 0, g = 0, b = 0;
//     // p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
//     // p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);    

//     hw.StartAudio(AudioCallback);

//     while(1){
//         // Do nothing in main loop
//     }
// }

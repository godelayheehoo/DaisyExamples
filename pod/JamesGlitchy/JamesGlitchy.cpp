#include "daisy_pod.h"
#include "daisy_seed.h"

#include <stdio.h>
#include <string.h>
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
static uint8_t  led_sel;
static int32_t  inc;
static Parameter p_knob1, p_knob2;

#define NUM_COLORS 5

Color my_colors[5];

// static void callback(AudioHandle::InterleavingInputBuffer  in,
//                      AudioHandle::InterleavingOutputBuffer out,
//                      size_t                                size)
// {
// }

int main(void)
{
    hw.seed.Configure();
    hw.Init();
    hw.seed.StartLog(true); // Enables USB CDC (serial) device
    int  tick_cnt = 0;
    bool ledstate = false;
    int encoder_total = 0;
    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);


    // hw.StartAdc();
    // p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    // p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);
    // hw.StartAudio(callback);

    char buff[128];
    Metro metro; // 500ms interval
    metro.Init(2.0f, hw.seed.AudioSampleRate()); // 2Hz frequency
    while(1)
    {
        hw.ProcessAllControls();
        int inc = hw.encoder.Increment();
        encoder_total += inc;
        float knob1_val = p_knob1.Process();
        if(metro.Process())
        {
            sprintf(buff, "Tick:\t%d\tEncoder:\t%d\tKnob1:\t%.3f\r\n", tick_cnt, encoder_total, knob1_val);
            hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
            tick_cnt = (tick_cnt + 1) % 100;
            hw.seed.SetLed(ledstate);
            ledstate = !ledstate;
        }
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

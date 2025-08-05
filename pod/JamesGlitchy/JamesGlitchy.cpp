#include "daisy_pod.h"
#include "daisy_seed.h"

#include <stdio.h>
#include <string.h>
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
// static Parameter p_knob1, p_knob2;


// static void callback(AudioHandle::InterleavingInputBuffer  in,
//                      AudioHandle::InterleavingOutputBuffer out,
//                      size_t                                size)
// {
// }


volatile int encoder_total = 0;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                         AudioHandle::InterleavingOutputBuffer out,
                         size_t size)
{
    hw.encoder.Debounce();
    int inc = hw.encoder.Increment();
    encoder_total += inc;
}

int main(void)
{
    hw.Init();
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    char buff[128];
    hw.led1.Set(0.0f, 0.0f, 1.0f); // Set LED 1 to blue
    hw.UpdateLeds();
    System::Delay(500);
    hw.led1.Set(0.0f, 1.0f, 1.0f); // Set LED 1 to turquoise
    hw.UpdateLeds();
    System::Delay(500);
    hw.led1.Set(1.0f, 0.0f, 1.0f); // Set LED 1 to purple
    hw.UpdateLeds();

    hw.StartAudio(AudioCallback);

    while(1)
    {
        sprintf(buff, "Encoder:\t%d\r\n", encoder_total);
        hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
        System::Delay(100);
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

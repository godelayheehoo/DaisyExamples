#include "daisy_pod.h"
#include "daisysp.h"
// #include "../../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.h"
#include "Effects/reverbsc.h"

using namespace daisy;
using namespace daisysp;


DaisyPod  hw;
Parameter p_knob1, p_knob2;


// Glitch buffer settings
#define GLITCH_BUF_SIZE 48000 // 1 second at 48kHz
float glitch_buf[GLITCH_BUF_SIZE];
size_t glitch_write_pos = 0;
size_t glitch_play_pos = 0;
bool glitch_active = false;
size_t glitch_len = 2400; // default 50ms at 48kHz

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Read knob1 for glitch length (10ms to 500ms)
    float knob1_val = p_knob1.Process();
    glitch_len = 480 + (size_t)(knob1_val * (GLITCH_BUF_SIZE/2 - 480)); // 10ms to 0.5s

    // Check button1 state
    glitch_active = hw.button1.Pressed();

    for(size_t i=0; i<size; i++)
    {
        float inputL = in[0][i];
        float inputR = in[1][i];

        // Always record to buffer (mono)
        glitch_buf[glitch_write_pos] = 0.5f * (inputL + inputR);
        glitch_write_pos = (glitch_write_pos + 1) % GLITCH_BUF_SIZE;

        float outL, outR;
        if(glitch_active)
        {
            // Play back a segment from the buffer
            outL = outR = glitch_buf[glitch_play_pos];
            glitch_play_pos = (glitch_play_pos + 1) % glitch_len;
        }
        else
        {
            // Normal passthrough
            outL = inputL;
            outR = inputR;
            // Reset play position to just behind write position
            glitch_play_pos = (glitch_write_pos + GLITCH_BUF_SIZE - glitch_len) % GLITCH_BUF_SIZE;
        }

        out[0][i] = outL;
        out[1][i] = outR;
    }
}


void BlinkBlueNTimes(int n)
{
    for(int i = 0; i < n; i++)
    {
        hw.led1.Set(0.0f, 0.0f, 1.0f); // Blue
        System::Delay(500);
        hw.UpdateLeds();

        hw.led1.Set(1.0f, 0.0f, 0.0f); // Red
        System::Delay(500);
        hw.UpdateLeds();
    }
}


int main(void)

{
    hw.Init();
    hw.StartAdc();

    float sample_rate = hw.AudioSampleRate(); 
    hw.SetAudioBlockSize(4); // Set block size to 4 samples
    

    //LED stuff
    hw.led1.Set(1.0f,0.0f,0.0f); // Set LED 1 to red
    hw.UpdateLeds();
    System::Delay(200); // Wait for 200 milliseconds
    BlinkBlueNTimes(3); // Blink blue 5 times
    float r = 0, g = 0, b = 0;
    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);    

    
    hw.StartAudio(AudioCallback);
    while(1){


    hw.ProcessAllControls();
    if(hw.button1.Pressed())
    {
        r = p_knob1.Process();
        g = p_knob2.Process();

        hw.led1.Set(r, g, b);

        hw.led2.Set(0,0,0); // Turn off LED 2
        // Update the LEDs

        hw.UpdateLeds();
    }
    else{
        // If button1 is not pressed, turn off the LED
        hw.led1.Set(0, 0, 0);
        // set LED 2 to be fully on
        hw.led2.Set(1, 1, 1);
        
        hw.UpdateLeds();
    }

    if(hw.encoder.Pressed())
    {
        // If encoder is pressed, light up LED 1
        hw.seed.SetLed(true);
    }else{hw.seed.SetLed(false);}
}
}

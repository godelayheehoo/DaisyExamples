#include "daisy_pod.h"
#include "daisysp.h"
// #include "../../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.h"
#include "Effects/reverbsc.h"

using namespace daisy;
using namespace daisysp;


DaisyPod  hw;
Parameter p_knob1, p_knob2;
Tremolo    trem;
Metro      tick;
ReverbSc  reverb;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
            

    for(size_t i=0; i<size; i++)
    {
        out[0][i] = trem.Process(in[0][i]); // Process left channel
        out[1][i] = trem.Process(in[1][i]); // Process right channel

     
        if(hw.encoder.Pressed())
        {
            float dryL = out[0][i]; // Store dry signal for left channel
            float dryR = out[1][i]; // Store dry signal for right channel

            float wetL = dryL; // Initialize wet signal for left channel
            float wetR = dryR; // Initialize wet signal for right channel 
            // Apply reverb effect
            reverb.Process(dryL, dryR, &wetL, &wetR); // Process reverb
            
            out[0][i] = dryL + 0.6f* wetL; // Mix dry and wet signals for left channel
            out[1][i] = dryR + 0.6f * wetR; // Mix dry and wet signals for right channel
        }
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
    
    //Tremolo setup
    trem.Init(sample_rate);


    //LED stuff
    hw.led1.Set(1.0f,0.0f,0.0f); // Set LED 1 to red
    hw.UpdateLeds();
    System::Delay(1000); // Wait for 1 second
    BlinkBlueNTimes(3); // Blink blue 5 times
    float r = 0, g = 0, b = 0;
    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);    

    //reverb stuff
    reverb.Init(sample_rate);
    reverb.SetFeedback(0.5f); // Set feedback to 50%

    hw.StartAudio(AudioCallback);
    while(1){

    // Update the tremolo effect parameters based on knob values
    trem.SetFreq(p_knob1.Process() * 10.f); // Set frequency based on knob 1
    trem.SetDepth(p_knob2.Process()); // Set depth based on knob 2

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

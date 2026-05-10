#include "daisy_pod.h"
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
static Parameter p_knob1, p_knob2;
float knob_1_value, knob_2_value;

int tick = 0;
int encoder_total = 0;

float sample_rate;
Metro metro_tick;

float METRO_FREQ = 2.0f; // Default metro frequency

static void updateKnobs();

static void printKnobs();

static void startupLightShow();

// Simple 1-pole smoothing helper
inline float SmoothValue(float current, float target, float smoothFactor)
{
    // smoothFactor ~ 0.0f → very slow change
    // smoothFactor ~ 1.0f → instant change (no smoothing)
    return current + smoothFactor * (target - current);
}

static void AudioCallback(AudioHandle::InputBuffer in,
                         AudioHandle::OutputBuffer out,
                         size_t size)
{

for(size_t i = 0; i < size; i++)
    {
    hw.ProcessAllControls(); // Update all knobs, encoders, and buttons
    //control logging 
    updateKnobs(); // Update all knobs
    if(metro_tick.Process()){
        printKnobs();
    }

    
    }
}

int main(void)
{
    hw.Init();
    startupLightShow(); // Show some lights on startup

    sample_rate = 48000.0f;
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    sample_rate = hw.AudioSampleRate(); // safer than hardcoding
    metro_tick.Init(METRO_FREQ, sample_rate);

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);

    hw.seed.SetLed(true);
    hw.seed.StartLog();
    hw.seed.PrintLine("Starting script");

    hw.StartAudio(AudioCallback);

    hw.StartAdc();


    hw.seed.SetLed(false);
    hw.led1.Set(0.0f, 1.0f, 0.0f); // Set LED 1 to green
    hw.led2.Set(0.0f,1.0f, 0.0f); // Set LED 2 to green
    hw.UpdateLeds();
    while(1)
    {
    }

}


void updateKnobs()
{
    //don't debounce if you're calling processallcontrols
    knob_1_value = p_knob1.Process();
    knob_2_value = p_knob2.Process();
    encoder_total += hw.encoder.Increment();
}


void printKnobs(){
    hw.seed.PrintLine("K1:%d,K2:%d,Enc:%d",
                      (int)(knob_1_value * 1000),
                      (int)(knob_2_value * 1000),
                      encoder_total);
}

void startupLightShow(){
hw.led1.Set(1.0f, 0.0f, 0.0f);  // Red
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(0.0f, 1.0f, 0.0f);  // Green
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(0.0f, 0.0f, 1.0f);  // Blue
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(1.0f, 0.5f, 0.0f);  // Orange
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(0.6f, 0.0f, 1.0f);  // Purple
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(0.0f, 1.0f, 1.0f);  // Cyan
hw.UpdateLeds();
System::Delay(400);

hw.led1.Set(0.0f,0.0f, 0.0f);  // Off
hw.UpdateLeds();
}


#define DIAGNOSTICS 1 // Enable diagnostics for debugging

#include "daisy_pod.h"
#include "daisy_seed.h"
#include "daisysp.h"
#include <atomic>

using namespace daisy;
using namespace daisysp;

//constants
float METRO_FREQ = 2.0f; // Default metro frequency
float CONTROL_METRO_FREQ = 200.0f;

static DaisyPod hw;
static Parameter p_knob1, p_knob2;
float knob_1_value, knob_2_value;
int tick = 0;
int encoder_total = 50;
int pressed_encoder_total = 0;
float gain = 0.5f; // Initial gain value
float density = 0.5f; // Initial density value

float smoothed_knob_1_value = 0.5f;
float smoothed_knob_2_value = 0.5f;


float sample_rate;
Metro metro_tick;
Metro control_metro_tick;

Particle particle;




//some colors
struct RGB {
    float r;
    float g;
    float b;
};

// Example: array of 5 colors
RGB colors[5] = {
    {1.0f, 0.0f, 0.0f}, // Red
    {0.0f, 1.0f, 0.0f}, // Green
    {0.0f, 0.0f, 1.0f}, // Blue
    {1.0f, 1.0f, 0.0f}, // Yellow
    {1.0f, 0.0f, 1.0f}  // Magenta
};

int current_color_idx = 0; // Index for current color

bool seedLedState = false; // State of the seed LED

static void updateKnobs();

static void printKnobs();

static void startupLightShow();

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


    if(control_metro_tick.Process()){
    hw.ProcessAllControls(); // Update all knobs, encoders, and buttons
    updateKnobs(); // Update all knobs
    
    smoothed_knob_1_value = SmoothValue(smoothed_knob_1_value, knob_1_value, 0.01f); // Smooth knob 1 value
    particle.SetFreq(440.0f + (smoothed_knob_1_value * 440.0f)); // Set frequency based on knob 1

    smoothed_knob_2_value = SmoothValue(smoothed_knob_2_value, knob_2_value, 0.01f); // Smooth knob 2 value
    particle.SetResonance(smoothed_knob_2_value);

   


  

    
    // Map encoder_total modulo 101 steps into [0,1)
    int steps = 101;
    int mod_val = encoder_total % steps;  // wrap in [0, 100]
    if(mod_val < 0) mod_val += steps;     // handle negative values

    // Convert steps to [0, 1)
    density = (float)mod_val / (steps - 1); // (steps-1)=100 so 0..100 maps to 0..1.0

    // Clamp to avoid zero density if needed
    if(density < 0.05f) density = 0.05f;
    particle.SetDensity(density);
    

    }

  
    
  
   //todo: remove *1000.0 after testing
    out[0][i] = particle.Process(); // Process the particle module
    out[1][i] = out[0][i]; // copy left channel to right channel
     
 

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
    control_metro_tick.Init(100.0f, sample_rate);

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);

    hw.seed.SetLed(true);
    hw.seed.StartLog();
    

    #if DIAGNOSTICS
    hw.seed.PrintLine("DIAGNOSTICS is enabled (DIAGNOSTICS=1)");
    #else
        hw.seed.PrintLine("DIAGNOSTICS is disabled (DIAGNOSTICS=0)");
    #endif

    hw.seed.PrintLine("Starting script");

    //init particle
    particle.Init(sample_rate);
    particle.SetFreq(440.0f); // Set initial frequency
    particle.SetResonance(0.5f); // Set initial resonance
    particle.SetDensity(density); // Set initial density
    particle.SetGain(gain); // Set initial gain
    particle.SetSpread(1.0f); // Set initial spread
    particle.SetRandomFreq(0.1f); // Set random frequency modulation
    particle.SetSync(true); // Enable frequency synchronization
    
    hw.seed.PrintLine("Sleeping for 1 second to allow logs to be opened");
    hw.led1.Set(0.0f, 0.0f, 0.0f); // Turn off LED 1
    hw.led2.Set(0.0f, 0.0f, 0.0f); // Turn off LED 2
    hw.UpdateLeds();    
   
    hw.led1.Set(1.0f, 0.0f, 0.0f); // Turn on LED 1
    hw.led2.Set(0.0f, 1.0f, 0.0f); // Turn on LED 2
    hw.UpdateLeds(); // Update LEDs
    //sleep for one more second to allow logs to be opened
    System::Delay(1000);
    hw.seed.PrintLine("Starting Audio");
    // Start audio with the callback

    hw.StartAudio(AudioCallback);

    hw.StartAdc();


    hw.seed.SetLed(false);
    hw.led1.Set(0.0f, 1.0f, 0.0f); // Set LED 1 to green

    hw.led2.Set(0.0f,1.0f, 0.0f); // Set LED 2 to green
    hw.UpdateLeds();

    while(1)
    {
    #if DIAGNOSTICS

        printKnobs();
        hw.seed.PrintLine("Button and Encoder Press States: Button1: %d, Button2: %d, Encoder Pressed: %d",
        hw.button1.Pressed(), hw.button2.Pressed(), hw.encoder.Pressed());

    System::Delay(500); // small sleep to avoid spamming USB

    #endif

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
#include "daisy_pod.h"
#include "daisy_seed.h"
#include "daisysp.h"

#define DEBUG_PRINT 1

using namespace daisy;
using namespace daisysp;

static DaisyPod hw;
static Parameter p_knob1, p_knob2;
float knob_1_value, knob_2_value;
float k1_smooth = 0.5f;
float k2_smooth = 0.5f;
float old_k1_smooth = k1_smooth;
float old_k2_smooth = k2_smooth;
bool update_led_in_callback = false;

int tick = 0;
int encoder_total = 0;

float sample_rate;
Metro control_metro_tick;
PitchShifter ps;


//debug stuff

float CONTROL_METRO_FREQ = 200.0f; // Default metro frequency
float SMOOTHING_FACTOR = 0.05f;

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

inline bool compare_floats(float a, float b, float epsilon = 0.0001f)
{
    return fabs(a - b) < epsilon;
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                         AudioHandle::InterleavingOutputBuffer out,
                         size_t size)
{

    for(size_t i = 0; i < size; i += 2)
    {

    hw.ProcessAllControls(); // Update all knobs, encoders, and buttons

    if(control_metro_tick.Process()){
        updateKnobs();   
        
        k1_smooth = SmoothValue(k1_smooth, knob_1_value, SMOOTHING_FACTOR);
        k2_smooth = SmoothValue(k2_smooth, knob_2_value, SMOOTHING_FACTOR);

        //check if k1_smooth is the same as old_k1_smooth, then update old_k1_smooth
        if(!compare_floats(k1_smooth, old_k1_smooth)){
            hw.led1.Set(k1_smooth, 0.0f, 1.0f - k1_smooth); // Map knob 1 to LED 1
            old_k1_smooth = k1_smooth;
            update_led_in_callback = true; // Set flag to update LED in callback
        }

        // //check if k2_smooth is the same as old_k2_smooth, then update old_k2_smooth
        // if(!compare_floats(k2_smooth, old_k2_smooth)){
        //     hw.led2.Set(k2_smooth, 0.0f, 1.0f - k2_smooth); // Map knob 2 to LED 2
        //     old_k2_smooth = k2_smooth;
        //     update_led_in_callback = true; // Set flag to update LED in callback
        // }


        
    }

        float sample_in_L = in[i];          // Left channel input (assuming mono input on left)
        float sample_in_R = in[i + 1];    // Right channel input (assuming mono input on right)

        float shifted_L;
        float shifted_R;
        if(hw.button1.Pressed()){
            shifted_L = ps.Process(sample_in_L);
            shifted_R = ps.Process(sample_in_R);
        }
        else{
            shifted_L = sample_in_L;
            shifted_R = sample_in_R;
        }

        out[i] = shifted_L;                 // Left channel output
        out[i+1] = shifted_R;               // Right channel output (duplicate)
    }



}

int main(void)
{
    hw.Init();
    startupLightShow(); // Show some lights on startup

    sample_rate = 48000.0f;
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    sample_rate = hw.AudioSampleRate(); // safer than hardcoding
    control_metro_tick.Init(CONTROL_METRO_FREQ, sample_rate);

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);

    hw.seed.SetLed(true);
    hw.seed.StartLog();
    hw.seed.PrintLine("Starting script");

    ps.Init(sample_rate);
    ps.SetTransposition(10.0f); // Map knob 1 to pitch shift range

    hw.led1.Set(0.0f, 1.0f, 0.0f); // Set LED 1 to green
    hw.led2.Set(0.0f,1.0f, 0.0f); // Set LED 2 to green

    
    hw.StartAudio(AudioCallback);

    hw.StartAdc();


    hw.seed.SetLed(false);
    hw.UpdateLeds();
    while(1)
    {
        #if DEBUG_PRINT
            printKnobs();
            //print the update_in_led_callback status
            hw.seed.PrintLine("Update LED in callback: %s",
                          update_led_in_callback ? "true" : "false");
        #endif

        
        if(update_led_in_callback){
            hw.UpdateLeds(); // Update LEDs only if they changed
            update_led_in_callback = false; // Reset flag

            #if DEBUG_PRINT
                hw.seed.PrintLine("LEDs updated in callback");
            #endif
        }
        
    
    System::Delay(500); // Adjust delay as needed

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


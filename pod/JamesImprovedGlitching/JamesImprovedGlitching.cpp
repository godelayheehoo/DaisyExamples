

#define WRITE_USB 1

#include "daisy_pod.h"
#include "daisy_seed.h"
#include <stdio.h>
#include <string.h>
#include <cmath>   // for fabs
#include "daisysp.h"
#include "daisy_core.h" // for Color

using namespace daisy;
using namespace daisysp;
using daisy::Color;

#include "Sampling/granularplayer.h"

static DaisyPod hw;
char buff[128];
Metro metro_tick;


#if WRITE_USB
int print_tick = 0;
#endif

constexpr int NUM_PRESET_COLORS = 7;
Color preset_colors[NUM_PRESET_COLORS];

// Initializes all available preset colors
void InitPresetColors()
{
    preset_colors[0].Init(Color::PresetColor::RED);
    preset_colors[1].Init(Color::PresetColor::GREEN);
    preset_colors[2].Init(Color::PresetColor::BLUE);
    preset_colors[3].Init(Color::PresetColor::WHITE);
    preset_colors[4].Init(Color::PresetColor::GOLD);
    preset_colors[5].Init(Color::PresetColor::CYAN);
    preset_colors[6].Init(Color::PresetColor::PURPLE);
}

// Colors: 0->Red, 1->Green, 2->Blue, 3->White, 4->Gold, 5->Cyan, 6->Purple
// This function cycles through the specified colors, updating the LED and waiting 200ms between each
void CycleLedColors(int idx1, int idx2, int idx3)
{
    int STARTUP_DELAY = 500;
    
    hw.led1.SetColor(preset_colors[idx1]);
    hw.UpdateLeds();
    System::Delay(STARTUP_DELAY);

    hw.led1.SetColor(preset_colors[idx2]);
    hw.UpdateLeds();
    System::Delay(STARTUP_DELAY);

    hw.led1.SetColor(preset_colors[idx3]);
    hw.UpdateLeds();
    System::Delay(STARTUP_DELAY);
}


constexpr float SAMPLE_RATE = 48000.0f;
constexpr size_t SAMPLE_SIZE = 24000; // 0.5 seconds at 48kHz
float audio_buffer[SAMPLE_SIZE] = {0.0f}; // buffer to hold incoming audio
size_t buffer_pos = 0;
GranularPlayer granular;
bool buffer_filled = false;
Parameter p_knob1;
float knob_1_value = 0.5f; // Normalized value for knob 1

static void AudioCallback(AudioHandle::InputBuffer in,
                         AudioHandle::OutputBuffer out,
                         size_t size)
{
    hw.ProcessAllControls();



    // Update knob value once per block
    knob_1_value = p_knob1.Process();
    // Continuously overwrite buffer in circular fashion
    // for (size_t i = 0; i < size; i++)
    // {
    //     #if WRITE_USB
    //     if(metro_tick.Process())
    //     {
    //     extern int print_tick;
    //     sprintf(buff, "Tick %d | Knob1: %d\r\n", print_tick, (int)(1000.0f*knob_1_value));
    //     hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
    //     print_tick++;
    //     }
    //     #endif

    //     audio_buffer[buffer_pos] = in[0][i]; // mono input
    //     buffer_pos = (buffer_pos + 1) % SAMPLE_SIZE;
    // }

    // Trigger granular.Init() only when button 1 is pressed
    static bool last_button1 = false;
    bool button1 = hw.button1.Pressed();
    bool button2 = hw.button2.Pressed();

    if(button2){

    if (button1 && !last_button1)
    {
        granular.Init(audio_buffer, SAMPLE_SIZE, SAMPLE_RATE);
    }
    last_button1 = button1;

        // Continuously overwrite buffer in circular fashion
    for (size_t i = 0; i < size; i++)
    {
        #if WRITE_USB
        if(metro_tick.Process())
        {
        extern int print_tick;
        sprintf(buff, "Tick %d | Knob1: %d\r\n", print_tick, (int)(1000.0f*knob_1_value));
        hw.seed.usb_handle.TransmitInternal((uint8_t*)buff, strlen(buff));
        print_tick++;
        }
        #endif

        audio_buffer[buffer_pos] = in[0][i]; // mono input
        buffer_pos = (buffer_pos + 1) % SAMPLE_SIZE;
    }

    // Trigger granular.Init() only when button 1 is pressed

    // Granular synthesis output with wet/dry mix
    for (size_t i = 0; i < size; i++)
    {
        
        float dry = in[0][i];
        float wet = granular.Process(0.5f, 0.0f, 500.0f);
        float sig = (1.0f - knob_1_value) * dry + knob_1_value * wet;
        out[0][i] = sig;
        out[1][i] = sig;
    }

        }
        else{ 
               for(size_t i=0; i<size; i++){
        out[0][i] = in[0][i]; // Pass through left channel
        out[1][i] = in[0][i]; // Pass L->R
    }}
}

int main(void)
{
    hw.Init();
#if WRITE_USB
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
#endif

    metro_tick.Init(2.0f, SAMPLE_RATE);

    // Initialize all available preset colors
    InitPresetColors();

    // Example usage: cycle through RED, GREEN, GOLD on startup
    CycleLedColors(0, 1, 6);

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);

    // Initialize granular with silence before audio starts
    for(size_t i = 0; i < SAMPLE_SIZE; i++)
    {
        audio_buffer[i] = 0.0f;
    }
    granular.Init(audio_buffer, SAMPLE_SIZE, SAMPLE_RATE);

    hw.StartAudio(AudioCallback);
    hw.StartAdc();
    while(1)
    {
        // Empty main loop
    }
}

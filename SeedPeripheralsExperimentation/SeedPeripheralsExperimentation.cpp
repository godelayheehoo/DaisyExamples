#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
char      buff[128];

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int main(void)
{
    hw.Init();

    // Start USB logging
    hw.StartLog(true);

    // Initialize ADC for potentiometer on pin A0
    AdcChannelConfig adc_cfg;
    adc_cfg.InitSingle(seed::A0);
    hw.adc.Init(&adc_cfg, 1);
    hw.adc.Start();

    // Initialize 3-position switch on pins D29 and D30
    Switch3 toggle;
    toggle.Init(seed::D29, seed::D30);

    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAudio(AudioCallback);

    hw.PrintLine("Initializing...");

    while(1)
    {
        // Delay to prevent serial monitor from being overwhelmed
        System::Delay(100);

        // Read pot (0.0 to 1.0)
        float pot_val = hw.adc.GetFloat(0);

        // Read switch
        uint8_t     sw_val = toggle.Read();
        const char* sw_str = "UNKNOWN";
        if(sw_val == Switch3::POS_LEFT)
            sw_str = "LEFT";
        else if(sw_val == Switch3::POS_CENTER)
            sw_str = "CENTER";
        else if(sw_val == Switch3::POS_RIGHT)
            sw_str = "RIGHT";

        // Print values
        hw.PrintLine(
            "Pot: " FLT_FMT3 " | Switch: %s", FLT_VAR3(pot_val), sw_str);
    }
}

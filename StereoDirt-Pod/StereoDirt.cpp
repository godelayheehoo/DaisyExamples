#include "daisy_pod.h"
#include "hid/parameter.h"
#include "daisysp.h"
#include <cmath>

using namespace daisy;
using namespace daisysp;

DaisyPod         hw;
static Parameter p_knob1, p_knob2;

Overdrive  overdrive_l, overdrive_r;
Wavefolder wavefolder_l, wavefolder_r;
Decimator  decimator_l, decimator_r;
Limiter    limiter_l, limiter_r;
Svf        side_hpf;

float r = 0.0, g = 0.0, b = 0.9;

enum class DistortionMode
{
    HARD_CLIP = 0,
    SOFT_CLIP = 1,
    FOLDOVER  = 2,
    DECIMATE  = 3,
    OFF       = 4
};
const int num_distortion_modes = 5;

static DistortionMode distortion_mode = DistortionMode::HARD_CLIP;

DistortionMode next_distortion_mode(DistortionMode m)
{
    int i = static_cast<int>(m);
    return static_cast<DistortionMode>((i + 1) % num_distortion_modes);
}

enum class StereoMode
{
    NORMAL = 0,
    WIDTH  = 1,
    MS_ISH = 2
};
const int  num_stereo_modes = 3;
StereoMode stereo_mode      = StereoMode::NORMAL;

StereoMode next_stereo_mode(StereoMode s)
{
    int i = static_cast<int>(s);
    return static_cast<StereoMode>((i + 1) % num_stereo_modes);
}

float drive_amount     = 0.0f;
float ms_drive_balance = 0.5f;
bool  catch_drive      = false;
bool  catch_ms         = false;

float out_level       = 0.5f;
float side_hpf_cutoff = 0.0f;
bool  catch_out       = false;
bool  catch_hpf       = false;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    float knob1_val = p_knob1.Process();
    float knob2_val = p_knob2.Process(); // knob 2

    if(hw.encoder.Pressed())
    {
        catch_drive = true; // Next time we release, drive needs to catch up
        if(catch_ms)
        {
            if(fabs(knob1_val - ms_drive_balance) < 0.05f)
                catch_ms = false;
        }

        if(!catch_ms)
        {
            ms_drive_balance = knob1_val;
        }

        catch_out = true; // Next time we release, out needs to catch up
        if(catch_hpf)
        {
            if(fabs(knob2_val - side_hpf_cutoff) < 0.05f)
                catch_hpf = false;
        }

        if(!catch_hpf)
        {
            side_hpf_cutoff = knob2_val;
        }
    }
    else
    {
        catch_ms = true; // Next time we press, ms needs to catch up
        if(catch_drive)
        {
            if(fabs(knob1_val - drive_amount) < 0.05f)
                catch_drive = false;
        }

        if(!catch_drive)
        {
            drive_amount = knob1_val;
        }

        catch_hpf = true; // Next time we press, hpf needs to catch up
        if(catch_out)
        {
            if(fabs(knob2_val - out_level) < 0.05f)
                catch_out = false;
        }

        if(!catch_out)
        {
            out_level = knob2_val;
        }
    }

    float hpf_freq = 20.0f * powf(100.0f, side_hpf_cutoff);
    side_hpf.SetFreq(hpf_freq);

    float drive_l = drive_amount;
    float drive_r = drive_amount;

    if(stereo_mode == StereoMode::NORMAL)
    {
        drive_r = drive_amount * 1.02f; // Slight mismatch
        if(drive_r > 1.0f)
            drive_r = 1.0f;
    }
    else if(stereo_mode == StereoMode::MS_ISH)
    {
        // ms_drive_balance: 0 is all mid, 1 is all side
        drive_l = drive_amount * (1.0f - ms_drive_balance) * 2.0f;
        drive_r = drive_amount * ms_drive_balance * 2.0f;

        if(drive_l > 1.0f)
            drive_l = 1.0f;
        if(drive_r > 1.0f)
            drive_r = 1.0f;
    }

    overdrive_l.SetDrive(drive_l);
    overdrive_r.SetDrive(drive_r);

    float wf_gain_l = 1.0f + (drive_l * 40.0f);
    float wf_gain_r = 1.0f + (drive_r * 40.0f);
    wavefolder_l.SetGain(wf_gain_l);
    wavefolder_r.SetGain(wf_gain_r);

    decimator_l.SetDownsampleFactor(drive_l);
    decimator_l.SetBitcrushFactor(drive_l);
    decimator_r.SetDownsampleFactor(drive_r);
    decimator_r.SetBitcrushFactor(drive_r);

    for(size_t i = 0; i < size; i++)
    {
        float in_l = in[0][i];
        float in_r = in[1][i];

        float process_l = in_l;
        float process_r = in_r;

        if(stereo_mode == StereoMode::MS_ISH)
        {
            float mid  = (in_l + in_r) / 2.0f;
            float side = (in_l - in_r) / 2.0f;
            process_l  = mid;
            process_r  = side;
        }

        switch(distortion_mode)
        {
            case DistortionMode::HARD_CLIP:
            {
                float pre_gain_l = 1.0f + (drive_l * 100.0f);
                process_l        = process_l * pre_gain_l;
                if(process_l > 1.0f)
                    process_l = 1.0f;
                else if(process_l < -1.0f)
                    process_l = -1.0f;

                float pre_gain_r = 1.0f + (drive_r * 100.0f);
                process_r        = process_r * pre_gain_r;
                if(process_r > 1.0f)
                    process_r = 1.0f;
                else if(process_r < -1.0f)
                    process_r = -1.0f;
                break;
            }
            case DistortionMode::SOFT_CLIP:
            {
                process_l = overdrive_l.Process(process_l);
                process_r = overdrive_r.Process(process_r);
                break;
            }
            case DistortionMode::FOLDOVER:
            {
                process_l = wavefolder_l.Process(process_l);
                process_r = wavefolder_r.Process(process_r);
                break;
            }
            case DistortionMode::DECIMATE:
            {
                process_l = decimator_l.Process(process_l);
                process_r = decimator_r.Process(process_r);

                // Boost signal back up a bit and clip just in case
                // since decimator can sometimes reduce apparent volume at high crush
                process_l *= 2.0f;
                process_r *= 2.0f;

                if(process_l > 1.0f)
                    process_l = 1.0f;
                else if(process_l < -1.0f)
                    process_l = -1.0f;

                if(process_r > 1.0f)
                    process_r = 1.0f;
                else if(process_r < -1.0f)
                    process_r = -1.0f;
                break;
            }
            case DistortionMode::OFF:
            {
                break;
            }
        }

        if(stereo_mode == StereoMode::WIDTH)
        {
            float mid  = (process_l + process_r) / 2.0f;
            float side = (process_l - process_r) / 2.0f;

            side *= 1.5f; // w > 1
            mid *= 0.9f;  // slightly reduce mid

            process_l = mid + side;
            process_r = mid - side;

            if(process_l > 1.0f)
                process_l = 1.0f;
            else if(process_l < -1.0f)
                process_l = -1.0f;

            if(process_r > 1.0f)
                process_r = 1.0f;
            else if(process_r < -1.0f)
                process_r = -1.0f;
        }
        else if(stereo_mode == StereoMode::MS_ISH)
        {
            float mid  = process_l;
            float side = process_r;

            process_l = mid + side;
            process_r = mid - side;

            if(process_l > 1.0f)
                process_l = 1.0f;
            else if(process_l < -1.0f)
                process_l = -1.0f;

            if(process_r > 1.0f)
                process_r = 1.0f;
            else if(process_r < -1.0f)
                process_r = -1.0f;
        }

        // Universal Side HPF
        float mid_hpf      = (process_l + process_r) / 2.0f;
        float side_hpf_sig = (process_l - process_r) / 2.0f;

        side_hpf.Process(side_hpf_sig);
        float wet_side = side_hpf.High();

        float mix = side_hpf_cutoff / 0.05f;
        if(mix > 1.0f)
            mix = 1.0f;

        side_hpf_sig = side_hpf_sig * (1.0f - mix) + wet_side * mix;

        process_l = mid_hpf + side_hpf_sig;
        process_r = mid_hpf - side_hpf_sig;

        limiter_l.ProcessBlock(&process_l, 1, 1.0f);
        limiter_r.ProcessBlock(&process_r, 1, 1.0f);

        out[0][i] = process_l * out_level;
        out[1][i] = process_r * out_level;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    //flash built-in led 4 times
    bool state = true;
    for(int i = 0; i < 8; i++)
    {
        hw.seed.SetLed(state);
        state = not state;
        hw.DelayMs(500);
    }

    p_knob1.Init(hw.knob1, 0.f, 1.f, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0.f, 1.f, Parameter::LINEAR);

    overdrive_l.Init();
    overdrive_r.Init();
    wavefolder_l.Init();
    wavefolder_r.Init();
    decimator_l.Init();
    decimator_r.Init();

    limiter_l.Init();
    limiter_r.Init();

    side_hpf.Init(hw.AudioSampleRate());
    side_hpf.SetRes(0.0f);

    hw.led1.Set(1.0, 0.0, 0.0);
    hw.led2.Set(0.0, 0.5, 0.5);
    hw.UpdateLeds();


    while(1)
    {
        hw.ProcessAllControls();
        hw.seed.SetLed(hw.encoder.Pressed());

        if(hw.button1.RisingEdge())
        {
            distortion_mode = next_distortion_mode(distortion_mode);
            switch(distortion_mode)
            {
                case DistortionMode::HARD_CLIP: hw.led1.Set(1.0, 0, 0); break;
                case DistortionMode::SOFT_CLIP: hw.led1.Set(0, 1.0, 0); break;
                case DistortionMode::FOLDOVER: hw.led1.Set(0, 0, 1.0); break;
                case DistortionMode::DECIMATE:
                    hw.led1.Set(1.0, 0.0, 1.0);
                    break; // Purple
                case DistortionMode::OFF: hw.led1.Set(0, 0, 0); break;
            }
            hw.UpdateLeds();
        }

        if(hw.button2.RisingEdge())
        {
            stereo_mode = next_stereo_mode(stereo_mode);
            switch(stereo_mode)
            {
                case StereoMode::NORMAL:
                    hw.led2.Set(0.0, 0.5, 0.5);
                    break; // Cyan-ish
                case StereoMode::WIDTH:
                    hw.led2.Set(1.0, 0.0, 1.0);
                    break; // Magenta
                case StereoMode::MS_ISH:
                    hw.led2.Set(1.0, 0.5, 0.0);
                    break; // Orange
            }
            hw.UpdateLeds();
        }
    }
}

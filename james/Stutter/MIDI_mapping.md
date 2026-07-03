# MIDI Mapping Configuration

This document lists all the parameters and controls on the Daisy Seed Stutter pedal that can be mapped to MIDI. It includes their internal runtime variable names, ranges, and descriptions.

The **MIDI CC** column lists the assigned MIDI Continuous Controller (CC) numbers (starting at CC 20).

## 1. Parameters & Controls Mapping Table

| Parameter / Control | Target Variable | MIDI Message Type | MIDI CC | Value Range / Translation | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Stutter Trigger (Momentary)** | `runtime.trigger_active` | Note or CC | 20 | **Note**: Press (Note On) = Active, Release (Note Off) = Idle.<br>**CC**: Value $\ge$ 64 = Active, Value < 64 = Idle. | Engages the stutter loop capture/playback momentarily, mimicking the Rate Encoder button push. |
| **Stutter Toggle (Latching)** | `runtime.state` transition | Note or CC | 21 | **Note**: Note On toggles state.<br>**CC**: Value $\ge$ 64 toggles state or turns ON; < 64 turns OFF. | Toggles the stutter effect between Active (`STUTTER_PLAYING`/`STUTTER_RECORDING`) and `STUTTER_IDLE`. |
| **Playback Rate (Continuous)** | `runtime.target_rate` | CC / Pitch Bend | 22 | `0` = 0.25x (`RATE_MIN`) <br> `64` = 1.0x (Unity) <br> `127` = 4.0x (`RATE_MAX`) | Adjusts the playback speed multiplier. Only applies when **Playback Rate Mode** is set to Continuous (OFF). |
| **Quantized Semitone Offset** | `runtime.semitone_offset` | CC / Pitch Bend | 23 | `0` = -24 semitones <br> `64` = 0 semitones (Unchanged) <br> `127` = +24 semitones | Adjusts the playback pitch in semitone intervals relative to the root frequency. Active in LFQ or PTQ modes. |
| **Wet/Dry Mix** | `runtime.wet` | CC | 24 | `0` = 100% Dry <br> `127` = 100% Wet | Blends between dry input and loop playback. Targets the same internal blend parameter as the physical potentiometer. |
| **Loop Length (Subdivision)** | `runtime.subdiv_pos` | CC / Program Change | 25 | **CC Value Ranges**:<br>- `0–25` = 1/32 note<br>- `26–50` = 1/16 note<br>- `51–76` = 1/8 note<br>- `77–102` = 1/4 note<br>- `103–127` = 1/2 note | Changes the loop length subdivision factor. Replicates turning the 5-position rotary switch. |
| **MIDI Sync Enable** | `config.midi_sync_enabled` | CC | 26 | `0–63` = OFF (Internal Tempo)<br>`64–127` = ON (Sync to MIDI Clock) | Enables or disables syncing the stutter buffer length to the incoming MIDI clock. |
| **Quantize Trigger Enable** | `config.quantize_trigger` | CC | 27 | `0–63` = OFF (Immediate Trigger)<br>`64–127` = ON (Quantize to MIDI Beat Grid) | Enables or disables beat-quantized stutter triggers on the incoming MIDI clock beats. |
| **Playback Rate Mode** | `config.playback_rate_mode` | CC | 28 | `0–42` = Continuous (OFF)<br>`43–85` = Loop-Freq Quantized (LFQ)<br>`86–127` = Pitch-Detection Quantized (PTQ) | Selects the pitch quantization mode of the playback engine. |
| **Manual BPM (Tempo)** | `runtime.bpm` | CC | 29 | `0–127` mapped linearly to **40–240 BPM** | Manually sets the tempo (BPM) when MIDI sync is disabled or inactive. |
| **Clear Loop / Reset** | Reset routine | CC or Note | 30 | **Note**: Note On triggers clear.<br>**CC**: Value $\ge$ 64 triggers clear. | Instantly stops the active stutter loop, clears the buffer, and returns the machine state to `STUTTER_IDLE`. |
| **Bypass / Active** | Audio path bypass | CC | 31 | `0–63` = Bypass (True Bypass/Analog Dry Path)<br>`64–127` = Active (Effect Processor Engaged) | Bypasses or engages the DSP processing chain entirely. |

---

## 2. Hardcoded System Realtime Messages

The firmware already processes the following MIDI System Realtime messages. They do not require CC or Note mappings and will be listened to globally if MIDI Sync is enabled:

| Message | Hex Byte | Firmware Action |
| :--- | :--- | :--- |
| **Timing Clock** | `0xF8` | Increments `runtime.midi_clock_ticks`. Used to smooth and calculate the external MIDI BPM. |
| **Start** | `0xFA` | Resets clock ticks, sets `runtime.midi_play_seen = true`, and prepares for beat alignment. |
| **Continue** | `0xFB` | Resets tempo tracking filters and sets `runtime.midi_play_seen = true`. |
| **Stop** | `0xFC` | Sets `runtime.midi_play_seen = false` and disables clock-dependent actions. |

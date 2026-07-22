#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstddef>

// Threshold above which ratio becomes infinite (1:inf)
constexpr float INFINITY_CUTOFF = 0.95f;

// Compressor Config & Input Defaults
constexpr float COMP_INPUT_GAIN_DB    = 6.0f;
constexpr float COMP_INPUT_SATURATION = 0.15f;

// Sidechain Filter Defaults
constexpr float SC_FILTER_INIT_FREQ = 1000.0f;
constexpr float SC_FILTER_INIT_RES  = 0.5f;

// Parameter Ranges & Scales
constexpr float CUTOFF_MIN_HZ    = 20.0f;
constexpr float CUTOFF_MAX_SCALE = 1000.0f;

constexpr float THRESHOLD_MIN_DB   = -60.0f;
constexpr float THRESHOLD_RANGE_DB = 60.0f;

constexpr float RATIO_MAX_EXP = 100.0f;

constexpr float ATTACK_MIN_MS   = 0.2f;
constexpr float ATTACK_RANGE_MS = 150.0f;

constexpr float RELEASE_MIN_MS   = 0.2f;
constexpr float RELEASE_RANGE_MS = 500.0f;

constexpr float STEREO_WIDTH_MAX   = 2.0f;
constexpr float MAKEUP_GAIN_MAX_DB = 24.0f;

// Timing & Boot Settings
constexpr int BOOT_FLASH_COUNT          = 4;
constexpr int BOOT_FLASH_DELAY_MS       = 100;
constexpr int STARTUP_DELAY_MS          = 10;
constexpr int BOOTLOADER_RESET_DELAY_MS = 500;

constexpr int INIT_FLASH_COUNT    = 3;
constexpr int INIT_FLASH_DELAY_MS = 100;

constexpr std::size_t AUDIO_BLOCK_SIZE = 4;

#endif // CONSTANTS_H

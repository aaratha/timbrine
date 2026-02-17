#pragma once

#define DEVICE_SAMPLE_RATE 48000.0f
#define DEVICE_FORMAT ma_format_f32
#define OUTPUT_CHANNELS 2
#define INPUT_BIN_SIZE 2048 
#define RANDOM_PHASE 0
#define NORMALIZE_PEAK 1
#define BIN_DECIMATION_FACTOR 2
#define STFT_DECIMATION_FACTOR 2
#define ROLLOFF_THRESHOLD 0.85f
#define MFCC_COEFF_COUNT 12

static constexpr float COMB_DELAYS[8] = {
    0.0297f, 0.0371f, 0.0411f, 0.0437f,
    0.0543f, 0.0587f, 0.0631f, 0.0677f
};

static constexpr float ALLPASS_DELAYS[4] = {
    0.0050f, 0.0017f, 0.0063f, 0.0089f
};

static constexpr float ALLPASS_FEEDBACK = 0.5f;
static constexpr float COMB_FEEDBACK     = 0.84f;
static constexpr float DEFAULT_DAMPING   = 5000.0f;

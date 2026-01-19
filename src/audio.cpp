#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.hpp"
#include "globals.hpp"

#include <iostream>

AudioCore::AudioCore(AnalysisCore &analysisCore) : analysisCore(analysisCore) {
  if (audioInitialized)
    return;

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = DEVICE_FORMAT;
  deviceConfig.playback.channels = OUTPUT_CHANNELS;
  deviceConfig.sampleRate = static_cast<ma_uint32>(DEVICE_SAMPLE_RATE);
  deviceConfig.dataCallback = AudioCore::dataCallback;
  deviceConfig.pUserData = this;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    std::cerr << "ma_device_init failed\n";
    return;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    std::cerr << "ma_device_start failed\n";
    ma_device_uninit(&device);
    return;
  }

  audioInitialized = true;
  running.store(true);
}

AudioCore::~AudioCore() {
  if (audioInitialized) {
    ma_device_uninit(&device);
    audioInitialized = false;
    running.store(false);
  }
}

void AudioCore::setBinIndex(size_t index) {
  size_t binCount = analysisCore.getBinCount();
  if (binCount == 0) {
    binIndex.store(0);
    return;
  }

  if (index >= binCount) {
    index = binCount - 1;
  }

  binIndex.store(index);
}

void AudioCore::dataCallback(ma_device *pDevice, void *pOutput,
                             const void * /*pInput*/, ma_uint32 frameCount) {
  auto *core = static_cast<AudioCore *>(pDevice->pUserData);
  float *out = static_cast<float *>(pOutput);

  if (!core->callbackSeen.exchange(true)) {
    std::cerr << "audio callback active\n";
  }

  core->processAudio(out, frameCount);
}

void AudioCore::prepareBinBuffer(size_t index, std::vector<float> &buffer,
                                 float &gain) {
  analysisCore.resynthesizeBin(index, buffer);
  if (buffer.empty()) {
    gain = 1.0f;
    return;
  }

  gain = 1.0f;
#if NORMALIZE_PEAK
  float peak = 0.0f;
  for (float sample : buffer) {
    float absSample = std::abs(sample);
    if (absSample > peak) {
      peak = absSample;
    }
  }

  if (peak > 0.0f) {
    gain = maxOutputAmplitude / peak;
  }
#endif
}

void AudioCore::processAudio(float *out, ma_uint32 frameCount) {
  size_t binCount = analysisCore.getBinCount();
  if (binCount == 0) {
    for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
      for (ma_uint32 ch = 0; ch < OUTPUT_CHANNELS; ++ch) {
        *out++ = 0.0f;
      }
    }
    return;
  }

  if (outputBuffer.size() != frameCount) {
    outputBuffer.assign(frameCount, 0.0f);
  }

  size_t currentBinIndex = binIndex.load();
  if (currentBinIndex >= binCount) {
    currentBinIndex = 0;
  }

  if (currentBinIndex != lastSynthBinIndex || binBuffer.empty()) {
    prepareBinBuffer(currentBinIndex, binBuffer, binGain);
    binPlayhead = 0;
    overlapSize = binBuffer.size() / 2;
    nextBufferReady = false;
    lastSynthBinIndex = currentBinIndex;
  }

  if (binBuffer.empty()) {
    for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
      for (ma_uint32 ch = 0; ch < OUTPUT_CHANNELS; ++ch) {
        *out++ = 0.0f;
      }
    }
    return;
  }

  for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
    if (!nextBufferReady && overlapSize > 0 &&
        binPlayhead >= binBuffer.size() - overlapSize) {
      prepareBinBuffer(currentBinIndex, nextBinBuffer, nextBinGain);
      nextBufferReady = !nextBinBuffer.empty();
    }

    float sample = 0.0f;
    if (nextBufferReady && overlapSize > 0 &&
        binPlayhead >= binBuffer.size() - overlapSize) {
      size_t x = binPlayhead - (binBuffer.size() - overlapSize);
      float t = static_cast<float>(x) / overlapSize;
      float fadeOut = 0.5f * (1.0f + cosf(M_PI * t));
      float fadeIn = 1.0f - fadeOut;
      sample = (binBuffer[binPlayhead] * binGain * fadeOut) +
               (nextBinBuffer[x] * nextBinGain * fadeIn);
    } else {
      sample = binBuffer[binPlayhead] * binGain;
    }

    if (sample > maxOutputAmplitude) {
      sample = maxOutputAmplitude;
    } else if (sample < -maxOutputAmplitude) {
      sample = -maxOutputAmplitude;
    }

    ++binPlayhead;
    if (binPlayhead >= binBuffer.size()) {
      if (nextBufferReady) {
        binBuffer.swap(nextBinBuffer);
        binGain = nextBinGain;
        binPlayhead = overlapSize;
        nextBufferReady = false;
      } else {
        binPlayhead = 0;
      }
    }

    outputBuffer[frame] = sample;
  }

  effectPipeline.processBuffer(outputBuffer);
  for (ma_uint32 frame = 0; frame < frameCount; ++frame) {
    float sample = outputBuffer[frame];
    for (ma_uint32 ch = 0; ch < OUTPUT_CHANNELS; ++ch) {
      *out++ = sample;
    }
  }
}

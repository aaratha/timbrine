#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.hpp"
#include "globals.hpp"

#include <algorithm>
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

  minSwitchIntervalSamples =
      static_cast<size_t>(DEVICE_SAMPLE_RATE / 15.0f); // 15 switches/sec
  samplesSinceSwitch = minSwitchIntervalSamples;
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
    targetBinIndex.store(0);
    return;
  }

  if (index >= binCount) {
    index = binCount - 1;
  }

  targetBinIndex.store(index);
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

  samplesSinceSwitch += frameCount;

  size_t desiredBinIndex = targetBinIndex.load();
  if (desiredBinIndex >= binCount) {
    desiredBinIndex = 0;
  }

  if (binBuffer.empty()) {
    prepareBinBuffer(desiredBinIndex, binBuffer, binGain);
    binPlayhead = 0;
    overlapSize = binBuffer.size() / 2;
    nextBufferReady = false;
    currentBinIndex = desiredBinIndex;
  }

  if (currentBinIndex != desiredBinIndex && !switching &&
      samplesSinceSwitch >= minSwitchIntervalSamples) {
    prepareBinBuffer(desiredBinIndex, switchBuffer, switchGain);
    if (!switchBuffer.empty() && !binBuffer.empty()) {
      const size_t maxSwitch = std::min(binBuffer.size(), switchBuffer.size());
      const size_t defaultSwitch =
          static_cast<size_t>(DEVICE_SAMPLE_RATE * 0.02f);
      switchLength = std::max<size_t>(1, std::min(maxSwitch, defaultSwitch));
      switchPos = 0;
      switchPlayhead = 0;
      switchTargetIndex = desiredBinIndex;
      switching = true;
      samplesSinceSwitch = 0;
    } else {
      currentBinIndex = desiredBinIndex;
      switching = false;
    }
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

    if (switching && switchLength > 0) {
      const float t = static_cast<float>(switchPos) /
                      static_cast<float>(switchLength);
      const float fadeOut = 0.5f * (1.0f + cosf(M_PI * t));
      const float fadeIn = 1.0f - fadeOut;
      float switchSample = 0.0f;
      if (switchPlayhead < switchBuffer.size()) {
        switchSample = switchBuffer[switchPlayhead] * switchGain;
      }
      sample = (sample * fadeOut) + (switchSample * fadeIn);
      ++switchPos;
      ++switchPlayhead;
      if (switchPos >= switchLength) {
        binBuffer.swap(switchBuffer);
        binGain = switchGain;
        if (binBuffer.empty()) {
          binPlayhead = 0;
        } else if (switchPlayhead == 0) {
          binPlayhead = binBuffer.size() - 1;
        } else {
          binPlayhead = switchPlayhead - 1;
        }
        overlapSize = binBuffer.size() / 2;
        nextBufferReady = false;
        currentBinIndex = switchTargetIndex;
        switching = false;
        samplesSinceSwitch = 0;
      }
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

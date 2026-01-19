#pragma once

#include <atomic>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "miniaudio.h"
#include "analysis.hpp"
#include "effects.hpp"

class AudioCore {
  AnalysisCore &analysisCore;

  ma_device_config deviceConfig{};
  ma_device device{};
  std::atomic<bool> running{false};
  bool audioInitialized{false};
  std::atomic<bool> callbackSeen{false};

  std::atomic<size_t> binIndex{0};
  size_t lastBinIndex{std::numeric_limits<size_t>::max()};
  std::vector<float> binBuffer;
  std::vector<float> nextBinBuffer;
  size_t binPlayhead{0};
  size_t lastSynthBinIndex{std::numeric_limits<size_t>::max()};
  size_t overlapSize{0};
  bool nextBufferReady{false};
  float binGain{1.0f};
  float nextBinGain{1.0f};
  float maxOutputAmplitude{0.8f};
  // Output buffer for the audio callback after passing through effect pipeline
  std::vector<float> outputBuffer;
  EffectPipeline effectPipeline;

  static void dataCallback(ma_device *pDevice, void *pOutput,
                           const void * /*pInput*/, ma_uint32 frameCount);
  void prepareBinBuffer(size_t index, std::vector<float> &buffer, float &gain);

public:
  AudioCore(AnalysisCore &analysisCore);
  ~AudioCore();

  void setBinIndex(size_t index);

  // Called per audio buffer
  void processAudio(float *out, ma_uint32 frameCount);

  // Optional: load samples
  void loadSample(const std::string &id, const std::vector<float> &data);
};

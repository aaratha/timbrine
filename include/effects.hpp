#pragma once

#include <vector>

enum class FilterType { Lowpass = 0, Highpass = 1, Bandpass = 2 };

struct DelayLine {
  std::vector<float> buffer;
  int size = 0;
  int writeIndex = 0;
  float delaySamples = 0.0f;
};

struct AudioEffect {
  virtual void process(std::vector<float> &buffer) = 0;
  virtual ~AudioEffect() {}
};

struct DelayEffect : public AudioEffect {
  DelayLine dl;
  float feedback = 0.5f;
  float mix = 0.5f;
  float sampleRate;

  DelayEffect(float maxDelayTime, float delayTime, int sampleRate);

  void setDelayTime(float seconds);
  void setFeedback(float fb);
  void setMix(float m);
  void process(std::vector<float> &buffer) override;
};

struct OnePoleFilter : public AudioEffect {
  FilterType type; // 0 = lowpass, 1 = highpass
  float a0 = 1.0f;
  float a1 = 0.0f;
  float b1 = 0.0f;
  float z1_y = 0.0f; // delayed output
  float z1_x = 0.0f; // delayed input
  bool initialized = false;

  OnePoleFilter(FilterType type);
  void setCutoff(float cutoffFreq, float sampleRate);
  float processSample(float in); // single sample for internal use
  void process(std::vector<float> &input) override;
};

struct CombFilter : public AudioEffect {
  DelayLine dl;
  OnePoleFilter dampingFilter{FilterType::Lowpass};
  float feedback = 0.5f;
  bool initializedDelayTime = false;
  bool initializedFeedback = false;
  bool initializedDamping = false;

  CombFilter(float maxDelayTime, float sampleRate);
  void setDampingCutoff(float cutoffFreq, float sampleRate);
  void setDelayTime(float seconds, float sampleRate);
  void setFeedback(float fb);
  void process(std::vector<float> &input) override;
};

struct AllpassFilter : public AudioEffect {
  DelayLine dl;
  float feedback = 0.5f;
  bool initializedDelayTime = false;
  bool initializedFeedback = false;

  AllpassFilter(float maxDelayTime, float sampleRate);
  void setDelayTime(float seconds, float sampleRate);
  void setFeedback(float fb);
  float processSample(float in); // single sample for internal use
  void process(std::vector<float> &input) override;
};

struct BiquadFilter : public AudioEffect {
  float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
  float b1 = 0.0f, b2 = 0.0f;
  float z1 = 0.0f, z2 = 0.0f;

  void setLowpass(float cutoffFreq, float Q, float sampleRate);
  void setHighpass(float cutoffFreq, float Q, float sampleRate);
  void setBandpass(float cutoffFreq, float Q, float sampleRate);
  void process(std::vector<float> &input) override;
};

struct ReverbEffect : public AudioEffect {
  static constexpr int NUM_COMBS = 8;
  static constexpr int NUM_ALLPASS = 4;

  std::array<CombFilter, NUM_COMBS> combs;
  std::array<AllpassFilter, NUM_ALLPASS> allpass;

  float wetMix = 0.3f;
  float dryMix = 0.7f;
  float roomSize = 1.0f;

  ReverbEffect(float sampleRate);

  void setRoomSize(float size, float sampleRate); // scales delay times
  void setDamping(float cutoffHz,
                  float sampleRate); // lowpass cutoff in feedback
  void setDecayTime(float rt60, float sampleRate); // sets feedback for a given RT60
  void setWet(float wet);            // 0.0 - 1.0
  void process(std::vector<float> &input) override;
};

class EffectPipeline {
  double sampleRate;
  int maxBlockSize;
  std::vector<AudioEffect *> effects;

public:
  EffectPipeline();
  ~EffectPipeline();

  void processBuffer(std::vector<float> &input);
};

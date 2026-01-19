#pragma once

#include <vector>

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

struct FilterEffect : public AudioEffect {
  float cutoffFrequency;

  FilterEffect(float cutoffFrequency = 1000.0f);

  void process(std::vector<float> &input) override;
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
  void process(std::vector<float>& buffer) override;
};

struct ReverbEffect : public AudioEffect {
  float decayTime;

  ReverbEffect(float decayTime = 1.0f);

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

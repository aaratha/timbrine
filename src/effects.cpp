#include "effects.hpp"
#include "globals.hpp"

// ---------------Filter Effect-----------------
FilterEffect::FilterEffect(float cutoffFrequency)
    : cutoffFrequency(cutoffFrequency) {}

void FilterEffect::process(std::vector<float> &input) {
  // Simple placeholder filter effect (not a real implementation)
  for (size_t i = 1; i < input.size(); ++i) {
    input[i] = (input[i - 1] + input[i]) * 0.5f;
  }
}

// ---------------Delay Effect-----------------
DelayEffect::DelayEffect(float maxDelayTime, float delayTime, int sampleRate)
    : sampleRate(sampleRate) {

  dl.size = static_cast<int>(maxDelayTime * sampleRate) + 2;
  dl.buffer.resize(dl.size, 0.0f);
  dl.writeIndex = 0;

  setDelayTime(delayTime);
}

void DelayEffect::setDelayTime(float seconds) {
  dl.delaySamples = std::clamp(
      seconds * sampleRate,
      1.0f,
      static_cast<float>(dl.size - 2)
  );
}

void DelayEffect::setFeedback(float fb) {
  feedback = std::clamp(fb, 0.0f, 0.999f);
}

void DelayEffect::setMix(float m) {
  mix = std::clamp(m, 0.0f, 1.0f);
}

void DelayEffect::process(std::vector<float>& input) {
  for (size_t n = 0; n < input.size(); ++n) {
    float in = input[n];

    float readPos = dl.writeIndex - dl.delaySamples;
    if (readPos < 0.0f)
      readPos += dl.size;

    int i0 = static_cast<int>(readPos);
    int i1 = (i0 + 1) % dl.size;
    float frac = readPos - i0;

    float delayed =
        dl.buffer[i0] + frac * (dl.buffer[i1] - dl.buffer[i0]);

    // Write input + feedback
    dl.buffer[dl.writeIndex] = in + delayed * feedback;

    // Advance write head
    dl.writeIndex++;
    if (dl.writeIndex >= dl.size)
      dl.writeIndex = 0;

    // Output
    input[n] = in * (1.0f - mix) + delayed * mix;
  }
}

// ---------------Reverb Effect-----------------
ReverbEffect::ReverbEffect(float decayTime) : decayTime(decayTime) {}

void ReverbEffect::process(std::vector<float> &input) {
  // Simple placeholder reverb effect (not a real implementation)
  for (size_t i = 1; i < input.size(); ++i) {
    input[i] += decayTime * input[i - 1] * 0.5f;
  }
}

// ---------------Effect Pipeline-----------------
EffectPipeline::EffectPipeline() {
  auto *delay = new DelayEffect(1.0f, 0.25f,
                                static_cast<int>(DEVICE_SAMPLE_RATE));
  delay->setFeedback(0.8f);
  delay->setMix(0.5f);
  effects.push_back(delay);
}

EffectPipeline::~EffectPipeline() {
  for (AudioEffect *effect : effects) {
    delete effect;
  }
}

void EffectPipeline::processBuffer(std::vector<float> &input) {
  for (AudioEffect *effect : effects) {
    effect->process(input);
  }
}

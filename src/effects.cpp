#include "effects.hpp"
#include "globals.hpp"

#include <cassert>
#include <iostream>

// ============================================
// ---------------Delay Effect-----------------
// ============================================
DelayEffect::DelayEffect(float maxDelayTime, float delayTime, int sampleRate)
    : sampleRate(sampleRate) {

  dl.size = static_cast<int>(maxDelayTime * sampleRate) + 2;
  dl.buffer.resize(dl.size, 0.0f);
  dl.writeIndex = 0;

  setDelayTime(delayTime);
}

void DelayEffect::setDelayTime(float seconds) {
  dl.delaySamples =
      std::clamp(seconds * sampleRate, 1.0f, static_cast<float>(dl.size - 2));
}

void DelayEffect::setFeedback(float fb) {
  feedback = std::clamp(fb, 0.0f, 0.999f);
}

void DelayEffect::setMix(float m) { mix = std::clamp(m, 0.0f, 1.0f); }

void DelayEffect::process(std::vector<float> &input) {
  for (size_t n = 0; n < input.size(); ++n) {
    float in = input[n];

    float readPos = dl.writeIndex - dl.delaySamples;
    if (readPos < 0.0f)
      readPos += dl.size;

    int i0 = static_cast<int>(readPos);
    int i1 = (i0 + 1) % dl.size;
    float frac = readPos - i0;

    float delayed = dl.buffer[i0] + frac * (dl.buffer[i1] - dl.buffer[i0]);

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

// ============================================
// ----------One-pole Filter Effect------------
// ============================================

OnePoleFilter::OnePoleFilter(FilterType type) { this->type = type; }

void OnePoleFilter::setCutoff(float cutoffFreq, float sampleRate) {
  initialized = true; // Must compute coefficients before processing any samples
  float x = expf(-2.0f * M_PI * cutoffFreq / sampleRate);
  if (type == FilterType::Lowpass) {
    a0 = 1.0f - x;
    a1 = 0.0f;
    b1 = x;
  } else if (type == FilterType::Highpass) {
    a0 = (1.0f + x) / 2.0f;
    a1 = -(1.0f + x) / 2.0f;
    b1 = x;
  } else if (type == FilterType::Bandpass) {
    std::cerr << "OnePoleFilter cannot be bandpass\n";
  }
}

float OnePoleFilter::processSample(float in) {
  assert(initialized &&
         "OnePoleFilter::setCutoff must be called before process()");
  float out = a0 * in + a1 * z1_x + b1 * z1_y;
  z1_x = in;
  z1_y = out;
  return out;
}

void OnePoleFilter::process(std::vector<float> &input) {
  for (size_t n = 0; n < input.size(); ++n) {
    float in = input[n];
    input[n] = processSample(in);
  }
}

// ============================================
// ------------Comb Filter Effect--------------
// ============================================
CombFilter::CombFilter(float maxDelayTime, float sampleRate) {
  dl.size = static_cast<int>(maxDelayTime * sampleRate) + 2;
  dl.buffer.resize(dl.size, 0.0f);
  dl.writeIndex = 0;
}

void CombFilter::setDelayTime(float seconds, float sampleRate) {
  dl.delaySamples =
      std::clamp(seconds * sampleRate, 1.0f, static_cast<float>(dl.size - 2));
  initializedDelayTime = true;
}

void CombFilter::setFeedback(float fb) {
  feedback = std::clamp(fb, 0.0f, 0.999f);
  initializedFeedback = true;
}

void CombFilter::setDampingCutoff(float cutoffFreq, float sampleRate) {
  dampingFilter.setCutoff(cutoffFreq, sampleRate);
  initializedDamping = true;
}

void CombFilter::process(std::vector<float> &input) {
  assert(initializedDelayTime && initializedFeedback && initializedDamping &&
         "CombFilter::set... functions must be called before process()");
  for (size_t n = 0; n < input.size(); ++n) {
    float in = input[n];

    float readPos = dl.writeIndex - dl.delaySamples;
    readPos =
        fmodf(readPos + dl.size, dl.size); // Wrap around for negative values

    int i0 = static_cast<int>(readPos);
    int i1 = (i0 + 1) % dl.size;
    float frac = readPos - i0;

    float delayed = dl.buffer[i0] + frac * (dl.buffer[i1] - dl.buffer[i0]);
    // Apply damping to the delayed signal before feeding back
    float damped = dampingFilter.processSample(delayed);

    // Write input + feedback
    dl.buffer[dl.writeIndex] = in + damped * feedback;

    // Advance write head
    dl.writeIndex++;
    if (dl.writeIndex >= dl.size)
      dl.writeIndex = 0;

    // Output is just the delayed signal
    input[n] = delayed;
  }
}

// ============================================
// -----------Allpass Filter Effect------------
// ============================================
AllpassFilter::AllpassFilter(float maxDelayTime, float sampleRate) {
  dl.size = static_cast<int>(maxDelayTime * sampleRate) + 2;
  dl.buffer.resize(dl.size, 0.0f);
  dl.writeIndex = 0;
}

void AllpassFilter::setDelayTime(float seconds, float sampleRate) {
  dl.delaySamples =
      std::clamp(seconds * sampleRate, 1.0f, static_cast<float>(dl.size - 2));
  initializedDelayTime = true;
}

void AllpassFilter::setFeedback(float fb) {
  feedback = std::clamp(fb, 0.0f, 0.999f);
  initializedFeedback = true;
}

float AllpassFilter::processSample(float in) {
  assert(
      initializedDelayTime && initializedFeedback &&
      "AllpassFilter::set... functions must be called before processSample()");
  float readPos = dl.writeIndex - dl.delaySamples;
  readPos =
      fmodf(readPos + dl.size, dl.size); // Wrap around for negative values

  int i0 = static_cast<int>(readPos);
  int i1 = (i0 + 1) % dl.size;
  float frac = readPos - i0;

  float delayed = dl.buffer[i0] + frac * (dl.buffer[i1] - dl.buffer[i0]);

  // Write input + feedback
  dl.buffer[dl.writeIndex] = in + delayed * feedback;

  // Advance write head
  dl.writeIndex++;
  if (dl.writeIndex >= dl.size)
    dl.writeIndex = 0;

  // Output is the sum of input and delayed signal, with feedback applied
  return delayed - in * feedback;
}

void AllpassFilter::process(std::vector<float> &input) {
  assert(initializedDelayTime && initializedFeedback &&
         "AllpassFilter::set... functions must be called before process()");
  for (size_t n = 0; n < input.size(); ++n) {
    float in = input[n];
    input[n] = processSample(in);
  }
}

// ============================================
// --------------Reverb Effect-----------------
// ============================================

ReverbEffect::ReverbEffect(float sampleRate)
    : combs({CombFilter(0.1f, sampleRate), CombFilter(0.1f, sampleRate),
             CombFilter(0.1f, sampleRate), CombFilter(0.1f, sampleRate),
             CombFilter(0.1f, sampleRate), CombFilter(0.1f, sampleRate),
             CombFilter(0.1f, sampleRate), CombFilter(0.1f, sampleRate)}),
      allpass({AllpassFilter(0.1f, sampleRate), AllpassFilter(0.1f, sampleRate),
               AllpassFilter(0.1f, sampleRate),
               AllpassFilter(0.1f, sampleRate)}) {
  for (int i = 0; i < NUM_COMBS; ++i) {
    combs[i].setDelayTime(COMB_DELAYS[i], sampleRate);
    float delaySamples = COMB_DELAYS[i] * roomSize * sampleRate;
    float fb = powf(0.001f, delaySamples / (DEFAULT_RT60 * sampleRate));
    combs[i].setFeedback(fb);
    combs[i].setDampingCutoff(DEFAULT_DAMPING, sampleRate);
  }
  for (int i = 0; i < NUM_ALLPASS; ++i) {
    allpass[i].setDelayTime(ALLPASS_DELAYS[i], sampleRate);
    allpass[i].setFeedback(ALLPASS_FEEDBACK);

  }
}

void ReverbEffect::setRoomSize(float size, float sampleRate) {
  // scale 0.0-1.0 maps to a multiplier on the base delay times
  float s = std::clamp(size, 0.1f, 2.0f);
  roomSize = s;
  for (int i = 0; i < NUM_COMBS; ++i)
    combs[i].setDelayTime(COMB_DELAYS[i] * s, sampleRate);
}

void ReverbEffect::setDamping(float cutoffHz, float sampleRate) {
  for (int i = 0; i < NUM_COMBS; ++i)
    combs[i].setDampingCutoff(cutoffHz, sampleRate);
}

void ReverbEffect::setDecayTime(float rt60, float sampleRate) {
    for (int i = 0; i < NUM_COMBS; ++i) {
        float delaySamples = COMB_DELAYS[i] * roomSize * sampleRate;
        float fb = powf(0.001f, delaySamples / (rt60 * sampleRate));
        combs[i].setFeedback(fb);
    }
}

void ReverbEffect::setWet(float wet) {
  wetMix = std::clamp(wet, 0.0f, 1.0f);
  dryMix = 1.0f - wetMix;
}

void ReverbEffect::process(std::vector<float> &input) {
  std::vector<float> wet(input.size(), 0.0f);

  // Run all comb filters in parallel, accumulate into wet buffer
  for (int i = 0; i < NUM_COMBS; ++i) {
    std::vector<float> combInput(
        input); // copy so each comb gets the dry signal
    combs[i].process(combInput);
    for (size_t n = 0; n < input.size(); ++n)
      wet[n] += combInput[n];
  }

  // Normalize comb output
  float norm = 1.0f / NUM_COMBS;
  for (size_t n = 0; n < wet.size(); ++n)
    wet[n] *= norm;

  // Run allpass filters in series on wet buffer
  for (int i = 0; i < NUM_ALLPASS; ++i)
    allpass[i].process(wet);

  // Mix dry and wet
  for (size_t n = 0; n < input.size(); ++n)
    input[n] = dryMix * input[n] + wetMix * wet[n];
}

// ============================================
// --------------Effect Pipeline---------------
// ============================================
EffectPipeline::EffectPipeline() {
  auto *delay =
      new DelayEffect(1.0f, 0.3f, static_cast<int>(DEVICE_SAMPLE_RATE));
  delay->setFeedback(0.2f);
  delay->setMix(0.0f);
  effects.push_back(delay);

  auto *reverb = new ReverbEffect(static_cast<float>(DEVICE_SAMPLE_RATE));
  reverb->setRoomSize(1.0f, static_cast<float>(DEVICE_SAMPLE_RATE));
  reverb->setDamping(4000.0f, static_cast<float>(DEVICE_SAMPLE_RATE));
  reverb->setDecayTime(0.5f, static_cast<float>(DEVICE_SAMPLE_RATE));
  reverb->setWet(0.3f);
  effects.push_back(reverb);
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

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// Simple stereo biquad (RBJ cookbook), allocation-free so it can be retuned on the audio thread.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    float lastGain = 1000.0f;
    void setShelf (bool high, double sr, float freq, float gainDb);
    void setPeak (double sr, float freq, float q, float gainDb);
    inline float process (int ch, float x)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; lastGain = 1000.0f; }
};

// The six FX modules of one channel (a pad or the master), in signal order:
// TRANSIENT > FILTER > EQUALIZER > COMPRESSOR > DELAY > REVERB
struct FxParams
{
    std::atomic<float> *dlyOn, *dlySync, *dlyFb, *dlyMix;
    std::atomic<float> *fltOn, *fltType, *fltCut, *fltRes, *fltEnv;
    std::atomic<float> *eqOn, *eqLow, *eqMid, *eqHigh;
    std::atomic<float> *revOn, *revSize, *revDecay, *revTone, *revMix;
    std::atomic<float> *cmpOn, *cmpThr, *cmpRatio, *cmpAtk, *cmpRel;
    std::atomic<float> *trnOn, *trnPunch, *trnSus;
};

class FxChain
{
public:
    // withSpace: a little delay and reverb on by default (the VOX pad)
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout&, const juce::String& prefix, const juce::String& namePrefix, bool withSpace = false);
    static FxParams bind (juce::AudioProcessorValueTreeState&, const juce::String& prefix);
    static const juce::StringArray& ids();

    void prepare (double sampleRate);
    void reset();
    void process (const FxParams&, float* L, float* R, int num, double bpm);

    std::atomic<float> gainReductionDb { 0.0f };

private:
    double sr = 44100.0;

    // transient
    float tFast = 0, tSlow = 0, tSusFast = 0, tSusSlow = 0;
    // filter
    juce::dsp::StateVariableTPTFilter<float> svf;
    float fltFollow = 0;
    int lastType = -1;
    // eq
    Biquad low, mid, high;
    // compressor
    float cmpEnv = 0;
    // delay
    juce::AudioBuffer<float> delayLine;
    int delayWrite = 0;
    float dampL = 0, dampR = 0;
    // reverb
    juce::Reverb reverb;
    juce::Reverb::Parameters lastRev;
    juce::AudioBuffer<float> wet;
};

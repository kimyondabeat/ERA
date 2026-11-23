#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Library.h"
#include "Fx.h"
#include "SampleEdit.h"

constexpr int numPads = 8;

struct PadInfo { const char* name; int note; const char* noteName; };
// KICK SNARE RIM PERCS / VOX FX CLAP TOM  (General MIDI drum notes where they exist)
constexpr std::array<PadInfo, numPads> padInfo { {
    { "KICK", 36, "C1" }, { "SNARE", 38, "D1" }, { "RIM", 37, "C#1" }, { "PERCS", 44, "G#1" },
    { "VOX", 45, "A1" },  { "FX", 47, "B1" },    { "CLAP", 39, "D#1" }, { "TOM", 41, "F1" },
} };

class EraProcessor : public juce::AudioProcessor,
                     private juce::Timer
{
public:
    EraProcessor();
    ~EraProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ERA"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Pads (message thread) ----
    static bool canAssign (int pad, const SamplePtr& s) { return s != nullptr && s->category == pad; }
    bool assignSample (int pad, SamplePtr);   // only sounds of the pad's category
    void assignLayer (int pad, SamplePtr);    // layer B can be any sound
    SamplePtr getPadSample (int pad) const   { return std::atomic_load (&padSamples[(size_t) pad]); }
    SamplePtr getLayerSample (int pad) const { return std::atomic_load (&layerSamples[(size_t) pad]); }
    void stepSound (int pad, int direction);
    void triggerPad (int pad, float velocity) { pendingTriggers[(size_t) pad].store (velocity); }
    void preview (SamplePtr);
    void resetSampleEdit (int pad);
    void resetFx (int chain);
    SampleEditSettings getEditSettings (int pad) const;

    // ---- Meters (any thread) ----
    uint32_t getHitCount (int pad) const { return hitCounts[(size_t) pad].load(); }
    uint32_t getMidiCount() const { return midiCount.load(); }
    float takePadLevel (int pad) { return padLevels[(size_t) pad].exchange (0.0f); }
    float takeMasterLevel() { return masterLevel.exchange (0.0f); }
    float getGainReduction (int chain) const { return chains[(size_t) chain].gainReductionDb.load(); }

    juce::AudioProcessorValueTreeState apvts;

    static juce::String pid (int pad, const char* what) { return "p" + juce::String (pad + 1) + "_" + what; }
    static juce::String fxPrefix (int chain) { return chain < numPads ? "p" + juce::String (chain + 1) + "_" : juce::String ("m_"); }
    static constexpr int masterChain = numPads;

private:
    struct Voice
    {
        SamplePtr data;
        double position = 0.0, increment = 1.0;
        float gain = 1.0f, env = 1.0f, envCoef = 1.0f;
        int attackSamples = 0, age = 0, startDelay = 0, pad = -1, fadeRemaining = 0;
        uint32_t order = 0;
        bool active = false;
    };

    struct Event { int pos; int pad; float velocity; };

    struct PadParams
    {
        std::atomic<float> *vol, *pan, *tune, *attack, *decay, *mute, *solo,
                           *start, *end, *fadeIn, *fadeOut, *offset, *stretch, *reverse, *normalize, *snap,
                           *layer, *blend, *lTune, *lAttack, *lDecay, *lVol;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setPadSample (int pad, SamplePtr);
    void startVoice (int pad, float velocity);
    void startOneVoice (int pad, SamplePtr, float gain, float tune, float attackMs, float decay, int delaySamples);
    void renderVoices (int from, int to);
    void mixChunk (juce::AudioBuffer<float>& out, int start, int num);
    void timerCallback() override;

    std::array<SamplePtr, numPads> padSamples, layerSamples, editedSamples;
    std::array<SampleEditSettings, numPads> renderedSettings;
    std::array<SamplePtr, numPads> renderedFrom;
    std::vector<SamplePtr> retired;
    juce::CriticalSection retireLock;
    SamplePtr previewSample;

    std::array<std::atomic<float>, numPads> pendingTriggers {};
    std::array<std::atomic<uint32_t>, numPads> hitCounts {};
    std::array<std::atomic<float>, numPads> padLevels {};
    std::atomic<float> masterLevel { 0.0f };
    std::atomic<uint32_t> midiCount { 0 };

    std::array<Voice, 64> voices;
    uint32_t voiceCounter = 0;
    double sr = 44100.0, bpm = 120.0;
    int fadeLength = 64;
    static constexpr int chunk = 256;
    int chunkStart = 0;
    juce::AudioBuffer<float> padBus, previewBus;

    std::array<FxChain, numPads + 1> chains;
    std::array<FxParams, numPads + 1> fxp {};
    std::array<juce::SmoothedValue<float>, numPads> gainL, gainR;
    juce::SmoothedValue<float> masterGain, widthS;
    float limiterEnv = 0.0f;

    std::array<PadParams, numPads> pp {};
    std::atomic<float> *masterP = nullptr, *widthP = nullptr, *limitP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EraProcessor)
};

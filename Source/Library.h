#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// One sound. Never changed after creation, so UI and audio threads can share it.
struct SampleData
{
    juce::AudioBuffer<float> buffer;  // always stereo
    double sampleRate = 44100.0;
    juce::String id;                  // "snare/Snare (Kraze)"
    juce::String name;                // "Snare (Kraze)"
    int category = -1;
    std::vector<float> peaks;         // waveform overview, 0..1 (signed max per bin)
};

using SamplePtr = std::shared_ptr<const SampleData>;

struct Category
{
    const char* key;
    const char* label;
};

// One category per pad, in pad order.
constexpr int numCategories = 8;
constexpr std::array<Category, numCategories> categories { {
    { "kick",  "Kick" },
    { "snare", "Snare" },
    { "rim",   "Rim" },
    { "percs", "Percs" },
    { "vox",   "Vox" },
    { "fx",    "FX" },
    { "clap",  "Clap" },
    { "tom",   "Tom" },
} };

// The sounds built into ERA (every file in Kit/, embedded at build time).
class Library
{
public:
    static const Library& get();

    const std::vector<SamplePtr>& all() const { return samples; }
    SamplePtr find (const juce::String& id) const;
    std::vector<SamplePtr> inCategory (int category) const;
    SamplePtr neighbour (const SamplePtr& current, int category, int direction) const;

    static std::vector<float> computePeaks (const juce::AudioBuffer<float>&, int numPeaks = 400);

private:
    Library();
    std::vector<SamplePtr> samples;
};

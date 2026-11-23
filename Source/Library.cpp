#include "Library.h"
#include "BinaryData.h"

std::vector<float> Library::computePeaks (const juce::AudioBuffer<float>& b, int numPeaks)
{
    std::vector<float> p ((size_t) numPeaks, 0.0f);
    const int n = b.getNumSamples();
    if (n == 0)
        return p;

    for (int i = 0; i < numPeaks; ++i)
    {
        const int s = (int) ((juce::int64) i * n / numPeaks);
        const int e = juce::jmax (s + 1, (int) ((juce::int64) (i + 1) * n / numPeaks));
        float m = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
            m = juce::jmax (m, b.getMagnitude (c, s, juce::jmin (e, n) - s));
        p[(size_t) i] = m;
    }

    const float top = *std::max_element (p.begin(), p.end());
    if (top > 0.0f)
        for (auto& v : p) v /= top;
    return p;
}

const Library& Library::get()
{
    static const Library lib;
    return lib;
}

Library::Library()
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    struct Entry { juce::String file; int resource; int category; };
    std::vector<Entry> entries;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String file = juce::String::fromUTF8 (BinaryData::originalFilenames[i]);
        const auto key = file.upToFirstOccurrenceOf ("__", false, false);
        for (int c = 0; c < numCategories; ++c)
            if (key == categories[(size_t) c].key)
                entries.push_back ({ file, i, c });
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
    {
        return a.category != b.category ? a.category < b.category : a.file < b.file;
    });

    for (const auto& e : entries)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[e.resource], size);
        if (data == nullptr || size <= 0)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (
            fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false)));
        if (reader == nullptr || reader->lengthInSamples < 2)
            continue;

        const int length = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 12.0));
        auto s = std::make_shared<SampleData>();
        s->buffer.setSize (2, length);
        reader->read (&s->buffer, 0, length, 0, true, true);
        if (reader->numChannels < 2)
            s->buffer.copyFrom (1, 0, s->buffer, 0, 0, length);
        s->sampleRate = reader->sampleRate;
        s->name = e.file.fromLastOccurrenceOf ("__", false, false).upToLastOccurrenceOf (".", false, false);
        s->id = juce::String (categories[(size_t) e.category].key) + "/" + s->name;
        s->category = e.category;
        s->peaks = computePeaks (s->buffer);
        samples.push_back (std::move (s));
    }
}

SamplePtr Library::find (const juce::String& id) const
{
    for (const auto& s : samples)
        if (s->id == id)
            return s;
    return nullptr;
}

std::vector<SamplePtr> Library::inCategory (int category) const
{
    std::vector<SamplePtr> out;
    for (const auto& s : samples)
        if (s->category == category)
            out.push_back (s);
    return out;
}

SamplePtr Library::neighbour (const SamplePtr& current, int category, int direction) const
{
    const auto list = inCategory (category);
    if (list.empty())
        return nullptr;

    int index = -1;
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i] == current)
            index = i;

    const int n = (int) list.size();
    index = index < 0 ? 0 : ((index + direction) % n + n) % n;
    return list[(size_t) index];
}

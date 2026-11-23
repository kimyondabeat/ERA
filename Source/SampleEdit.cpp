#include "SampleEdit.h"

namespace
{
    int nearestZeroCrossing (const float* x, int length, int pos, int radius)
    {
        int best = pos, bestDist = radius + 1;
        for (int d = 0; d <= radius; ++d)
            for (int sgn : { -1, 1 })
            {
                const int i = pos + sgn * d;
                if (i <= 0 || i >= length - 1)
                    continue;
                if ((x[i - 1] <= 0.0f && x[i] >= 0.0f) || (x[i - 1] >= 0.0f && x[i] <= 0.0f))
                    if (d < bestDist) { best = i; bestDist = d; }
            }
        return best;
    }

    void regionSamples (const SampleData& s, const SampleEditSettings& e, int& a, int& b)
    {
        const int len = s.buffer.getNumSamples();
        a = juce::jlimit (0, len - 2, (int) (juce::jmin (e.start, e.end) * (float) len));
        b = juce::jlimit (a + 2, len, (int) (juce::jmax (e.start, e.end) * (float) len));
        if (e.snap)
        {
            const int radius = (int) (s.sampleRate * 0.005);
            const float* x = s.buffer.getReadPointer (0);
            if (a > 0)      a = nearestZeroCrossing (x, len, a, radius);
            if (b < len)    b = nearestZeroCrossing (x, len, b, radius);
            b = juce::jlimit (a + 2, len, b);
        }
    }
}

std::pair<float, float> SampleEdit::snappedRegion (const SampleData& s, const SampleEditSettings& e)
{
    int a = 0, b = 0;
    regionSamples (s, e, a, b);
    const float len = (float) s.buffer.getNumSamples();
    return { (float) a / len, (float) b / len };
}

SamplePtr SampleEdit::render (const SamplePtr& source, const SampleEditSettings& e)
{
    if (source == nullptr || e.isNeutral())
        return source;

    const auto& src = source->buffer;
    int a = 0, b = 0;
    regionSamples (*source, e, a, b);
    const int regionLen = b - a;

    juce::AudioBuffer<float> region (2, regionLen);
    for (int c = 0; c < 2; ++c)
        region.copyFrom (c, 0, src, c, a, regionLen);

    if (e.reverse)
        for (int c = 0; c < 2; ++c)
        {
            auto* x = region.getWritePointer (c);
            std::reverse (x, x + regionLen);
        }

    // STRETCH: overlap-add with Hann windows over the whole region, then the original first
    // 15 ms are put back (with a short crossfade) so the hit stays sharp.
    juce::AudioBuffer<float> out;
    const float stretch = juce::jlimit (0.5f, 2.0f, e.stretch);
    if (std::abs (stretch - 1.0f) > 0.01f)
    {
        const int grain = juce::jmax (256, (int) (source->sampleRate * 0.04));
        const int hopOut = grain / 4;
        const double hopIn = hopOut / (double) stretch;
        const int outLen = juce::jmax (4, (int) (regionLen * stretch));
        const int pad = grain / 2;
        juce::AudioBuffer<float> acc (2, outLen + grain * 2);
        acc.clear();
        std::vector<float> norm ((size_t) acc.getNumSamples(), 0.0f);
        std::vector<float> win ((size_t) grain);
        for (int i = 0; i < grain; ++i)
            win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) grain);

        for (int k = 0;; ++k)
        {
            const int outPos = k * hopOut;              // in acc (shifted by pad)
            const int inPos = (int) (k * hopIn) - pad;  // in region
            if (outPos > outLen + pad)
                break;
            for (int i = 0; i < grain; ++i)
            {
                const int si = inPos + i, di = outPos + i;
                if (di >= acc.getNumSamples()) break;
                const float w = win[(size_t) i];
                norm[(size_t) di] += w;
                if (si < 0 || si >= regionLen) continue;
                for (int c = 0; c < 2; ++c)
                    acc.getWritePointer (c)[di] += region.getReadPointer (c)[si] * w;
            }
        }

        out.setSize (2, outLen);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < outLen; ++i)
            {
                const float n = norm[(size_t) (i + pad)];
                out.getWritePointer (c)[i] = n > 0.05f ? acc.getReadPointer (c)[i + pad] / n : 0.0f;
            }

        const int head = std::min ({ regionLen, outLen, (int) (source->sampleRate * 0.015) });
        const int xf = std::min ({ head, (int) (source->sampleRate * 0.005) });
        for (int c = 0; c < 2; ++c)
        {
            auto* o = out.getWritePointer (c);
            const auto* r = region.getReadPointer (c);
            for (int i = 0; i < head; ++i)
            {
                const float t = i < head - xf ? 1.0f : (float) (head - i) / (float) juce::jmax (1, xf);
                o[i] = r[i] * t + o[i] * (1.0f - t);
            }
        }
    }
    else
    {
        out.makeCopyOf (region);
    }

    const int n = out.getNumSamples();
    const int fadeIn = juce::jmin (n / 2, (int) (e.fadeInMs * 0.001 * source->sampleRate));
    const int fadeOut = juce::jmin (n / 2, (int) (e.fadeOutMs * 0.001 * source->sampleRate));
    for (int c = 0; c < 2; ++c)
    {
        if (fadeIn > 0)  out.applyGainRamp (c, 0, fadeIn, 0.0f, 1.0f);
        if (fadeOut > 0) out.applyGainRamp (c, n - fadeOut, fadeOut, 1.0f, 0.0f);
    }
    // always a tiny fade at the edges so a cut never clicks
    const int edge = juce::jmin (n / 4, 32);
    for (int c = 0; c < 2; ++c)
    {
        if (fadeIn == 0 && a > 0 && ! e.reverse) out.applyGainRamp (c, 0, edge, 0.0f, 1.0f);
        if (fadeOut == 0)                        out.applyGainRamp (c, n - edge, edge, 1.0f, 0.0f);
    }

    if (e.normalize)
    {
        const float peak = out.getMagnitude (0, n);
        if (peak > 1.0e-4f)
            out.applyGain (0.95f / peak);
    }

    auto s = std::make_shared<SampleData>();
    s->buffer = std::move (out);
    s->sampleRate = source->sampleRate;
    s->id = source->id;
    s->name = source->name;
    s->category = source->category;
    s->peaks = source->peaks;
    return s;
}

#pragma once
#include "Library.h"

// The SAMPLE EDIT settings of one pad. The edited sound is rendered once on the
// message thread whenever a setting changes, so the audio thread only plays it back.
struct SampleEditSettings
{
    float start = 0.0f, end = 1.0f;       // 0..1 of the source
    float fadeInMs = 0.0f, fadeOutMs = 0.0f;
    float stretch = 1.0f;                 // 0.5 .. 2 (length, pitch unchanged)
    bool reverse = false, normalize = false, snap = true;

    bool operator== (const SampleEditSettings& o) const
    {
        return start == o.start && end == o.end && fadeInMs == o.fadeInMs && fadeOutMs == o.fadeOutMs
            && stretch == o.stretch && reverse == o.reverse && normalize == o.normalize && snap == o.snap;
    }
    bool operator!= (const SampleEditSettings& o) const { return ! (*this == o); }
    bool isNeutral() const { return *this == SampleEditSettings(); }
};

namespace SampleEdit
{
    SamplePtr render (const SamplePtr& source, const SampleEditSettings&);

    // Where the edited region really starts and ends (after SNAP), 0..1 of the source.
    std::pair<float, float> snappedRegion (const SampleData&, const SampleEditSettings&);
}

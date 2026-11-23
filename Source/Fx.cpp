#include "Fx.h"

void Biquad::setShelf (bool high, double sampleRate, float freq, float gainDb)
{
    lastGain = gainDb;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / 2.0 * std::sqrt (2.0);
    const double sA = 2.0 * std::sqrt (A) * alpha;
    double B0, B1, B2, A0, A1, A2;
    if (high)
    {
        B0 = A * ((A + 1) + (A - 1) * cw + sA);  B1 = -2 * A * ((A - 1) + (A + 1) * cw);  B2 = A * ((A + 1) + (A - 1) * cw - sA);
        A0 = (A + 1) - (A - 1) * cw + sA;        A1 = 2 * ((A - 1) - (A + 1) * cw);        A2 = (A + 1) - (A - 1) * cw - sA;
    }
    else
    {
        B0 = A * ((A + 1) - (A - 1) * cw + sA);  B1 = 2 * A * ((A - 1) - (A + 1) * cw);   B2 = A * ((A + 1) - (A - 1) * cw - sA);
        A0 = (A + 1) + (A - 1) * cw + sA;        A1 = -2 * ((A - 1) + (A + 1) * cw);       A2 = (A + 1) + (A - 1) * cw - sA;
    }
    b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
}

void Biquad::setPeak (double sampleRate, float freq, float q, float gainDb)
{
    lastGain = gainDb;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double cw = std::cos (w0);
    const double A0 = 1 + alpha / A;
    b0 = (float) ((1 + alpha * A) / A0); b1 = (float) (-2 * cw / A0); b2 = (float) ((1 - alpha * A) / A0);
    a1 = (float) (-2 * cw / A0);         a2 = (float) ((1 - alpha / A) / A0);
}

// ---------------------------------------------------------------------------
const juce::StringArray& FxChain::ids()
{
    static const juce::StringArray list { "dlyOn", "dlySync", "dlyFb", "dlyMix",
                                          "fltOn", "fltType", "fltCut", "fltRes", "fltEnv",
                                          "eqOn", "eqLow", "eqMid", "eqHigh",
                                          "revOn", "revSize", "revDecay", "revTone", "revMix",
                                          "cmpOn", "cmpThr", "cmpRatio", "cmpAtk", "cmpRel",
                                          "trnOn", "trnPunch", "trnSus" };
    return list;
}

void FxChain::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                             const juce::String& prefix, const juce::String& n, bool withSpace)
{
    using namespace juce;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto f = [&] (const char* id, const String& name, NormalisableRange<float> range, float def,
                  std::function<String (float, int)> text = nullptr, const String& label = {})
    {
        auto a = AudioParameterFloatAttributes().withLabel (label);
        if (text) a = a.withStringFromValueFunction (text);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { prefix + id, 1 }, n + name, range, def, a));
    };
    auto b = [&] (const char* id, const String& name, bool def = false)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { prefix + id, 1 }, n + name, def));
    };
    auto c = [&] (const char* id, const String& name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { prefix + id, 1 }, n + name, choices, def));
    };

    b ("dlyOn", "Delay On", withSpace);
    c ("dlySync", "Delay Time", { "1/4", "1/8", "1/8D", "1/16" }, 2);
    f ("dlyFb", "Delay Feedback", { 0.0f, 0.9f, 0.01f }, withSpace ? 0.25f : 0.35f, pct);
    f ("dlyMix", "Delay Mix", { 0.0f, 1.0f, 0.01f }, withSpace ? 0.16f : 0.3f, pct);

    b ("fltOn", "Filter On");
    c ("fltType", "Filter Type", { "LP", "HP", "BP" }, 0);
    f ("fltCut", "Filter Cutoff", { 20.0f, 20000.0f, 1.0f, 0.25f }, 8000.0f, [] (float v, int) { return v >= 1000 ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    f ("fltRes", "Filter Reso", { 0.0f, 1.0f, 0.01f }, 0.1f, pct);
    f ("fltEnv", "Filter Env", { -1.0f, 1.0f, 0.01f }, 0.0f, pct);

    b ("eqOn", "EQ On");
    f ("eqLow", "EQ Low", { -12.0f, 12.0f, 0.1f }, 0.0f, nullptr, "dB");
    f ("eqMid", "EQ Mid", { -12.0f, 12.0f, 0.1f }, 0.0f, nullptr, "dB");
    f ("eqHigh", "EQ High", { -12.0f, 12.0f, 0.1f }, 0.0f, nullptr, "dB");

    b ("revOn", "Reverb On", withSpace);
    f ("revSize", "Reverb Size", { 0.0f, 1.0f, 0.01f }, 0.5f, pct);
    f ("revDecay", "Reverb Decay", { 0.0f, 1.0f, 0.01f }, withSpace ? 0.42f : 0.5f, pct);
    f ("revTone", "Reverb Tone", { 0.0f, 1.0f, 0.01f }, withSpace ? 0.6f : 0.5f, pct);
    f ("revMix", "Reverb Mix", { 0.0f, 1.0f, 0.01f }, withSpace ? 0.2f : 0.25f, pct);

    b ("cmpOn", "Comp On");
    f ("cmpThr", "Comp Threshold", { -40.0f, 0.0f, 0.1f }, -12.0f, nullptr, "dB");
    f ("cmpRatio", "Comp Ratio", { 1.0f, 10.0f, 0.1f, 0.5f }, 3.0f, [] (float v, int) { return String (v, 1) + ":1"; });
    f ("cmpAtk", "Comp Attack", { 0.1f, 50.0f, 0.1f, 0.4f }, 5.0f, nullptr, "ms");
    f ("cmpRel", "Comp Release", { 10.0f, 500.0f, 1.0f, 0.4f }, 120.0f, nullptr, "ms");

    b ("trnOn", "Transient On");
    f ("trnPunch", "Transient Punch", { -1.0f, 1.0f, 0.01f }, 0.0f, pct);
    f ("trnSus", "Transient Sustain", { -1.0f, 1.0f, 0.01f }, 0.0f, pct);
}

FxParams FxChain::bind (juce::AudioProcessorValueTreeState& s, const juce::String& prefix)
{
    auto g = [&] (const char* id) { auto* p = s.getRawParameterValue (prefix + id); jassert (p != nullptr); return p; };
    return { g ("dlyOn"), g ("dlySync"), g ("dlyFb"), g ("dlyMix"),
             g ("fltOn"), g ("fltType"), g ("fltCut"), g ("fltRes"), g ("fltEnv"),
             g ("eqOn"), g ("eqLow"), g ("eqMid"), g ("eqHigh"),
             g ("revOn"), g ("revSize"), g ("revDecay"), g ("revTone"), g ("revMix"),
             g ("cmpOn"), g ("cmpThr"), g ("cmpRatio"), g ("cmpAtk"), g ("cmpRel"),
             g ("trnOn"), g ("trnPunch"), g ("trnSus") };
}

void FxChain::prepare (double sampleRate)
{
    sr = sampleRate;
    svf.prepare ({ sampleRate, 512, 2 });
    delayLine.setSize (2, (int) (sampleRate * 3.0));
    wet.setSize (2, 512);
    reverb.setSampleRate (sampleRate);
    reset();
}

void FxChain::reset()
{
    tFast = tSlow = tSusFast = tSusSlow = 0;
    svf.reset(); fltFollow = 0; lastType = -1;
    low.reset(); mid.reset(); high.reset();
    cmpEnv = 0; gainReductionDb.store (0.0f);
    delayLine.clear(); delayWrite = 0; dampL = dampR = 0;
    reverb.reset();
}

void FxChain::process (const FxParams& p, float* L, float* R, int num, double bpm)
{
    if (wet.getNumSamples() < num)
        return; // never happens: the processor works in chunks of 256

    // ---- TRANSIENT: fast vs slow envelope
    if (p.trnOn->load() > 0.5f)
    {
        const float punch = p.trnPunch->load(), sus = p.trnSus->load();
        const float fa = std::exp (-1.0f / (0.0007f * (float) sr)), sa = std::exp (-1.0f / (0.02f * (float) sr));
        const float fr = std::exp (-1.0f / (0.03f * (float) sr)),  srl = std::exp (-1.0f / (0.3f * (float) sr));
        for (int i = 0; i < num; ++i)
        {
            const float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            tFast = x > tFast ? fa * tFast + (1 - fa) * x : fr * tFast + (1 - fr) * x;
            tSlow = x > tSlow ? sa * tSlow + (1 - sa) * x : fr * tSlow + (1 - fr) * x;
            tSusFast = x > tSusFast ? fa * tSusFast + (1 - fa) * x : fr * tSusFast + (1 - fr) * x;
            tSusSlow = x > tSusSlow ? fa * tSusSlow + (1 - fa) * x : srl * tSusSlow + (1 - srl) * x;
            const float att = std::pow ((tFast + 1.0e-5f) / (tSlow + 1.0e-5f), punch * 1.5f);
            const float tail = std::pow ((tSusSlow + 1.0e-5f) / (tSusFast + 1.0e-5f), sus * 1.2f);
            const float g = juce::jlimit (0.2f, 4.0f, att * tail);
            L[i] *= g; R[i] *= g;
        }
    }

    // ---- FILTER with envelope follower
    if (p.fltOn->load() > 0.5f)
    {
        const int type = (int) p.fltType->load();
        if (type != lastType)
        {
            svf.setType (type == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                       : type == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                                   : juce::dsp::StateVariableTPTFilterType::bandpass);
            lastType = type;
        }
        svf.setResonance (0.5f + 9.0f * p.fltRes->load() * p.fltRes->load());
        const float cut = p.fltCut->load(), envAmt = p.fltEnv->load();
        const float att = std::exp (-1.0f / (0.002f * (float) sr)), rel = std::exp (-1.0f / (0.15f * (float) sr));
        const float nyq = (float) sr * 0.45f;
        for (int i = 0; i < num; i += 16)
        {
            const int end = juce::jmin (num, i + 16);
            for (int k = i; k < end; ++k)
            {
                const float x = juce::jmax (std::abs (L[k]), std::abs (R[k]));
                fltFollow = x > fltFollow ? att * fltFollow + (1 - att) * x : rel * fltFollow + (1 - rel) * x;
            }
            const float octaves = envAmt * juce::jmin (1.0f, fltFollow * 2.5f) * 5.0f;
            svf.setCutoffFrequency (juce::jlimit (20.0f, nyq, cut * std::pow (2.0f, octaves)));
            for (int k = i; k < end; ++k)
            {
                L[k] = svf.processSample (0, L[k]);
                R[k] = svf.processSample (1, R[k]);
            }
        }
    }
    else if (lastType != -1) { svf.reset(); lastType = -1; }

    // ---- EQUALIZER (low shelf 120 Hz, peak 1.2 kHz, high shelf 7.5 kHz)
    if (p.eqOn->load() > 0.5f)
    {
        const float gl = p.eqLow->load(), gm = p.eqMid->load(), gh = p.eqHigh->load();
        if (std::abs (gl - low.lastGain) > 1.0e-4f)  low.setShelf (false, sr, 120.0f, gl);
        if (std::abs (gm - mid.lastGain) > 1.0e-4f)  mid.setPeak (sr, 1200.0f, 0.8f, gm);
        if (std::abs (gh - high.lastGain) > 1.0e-4f) high.setShelf (true, sr, 7500.0f, gh);
        for (int i = 0; i < num; ++i)
        {
            L[i] = high.process (0, mid.process (0, low.process (0, L[i])));
            R[i] = high.process (1, mid.process (1, low.process (1, R[i])));
        }
    }
    else if (low.lastGain < 999.0f) { low.reset(); mid.reset(); high.reset(); }

    // ---- COMPRESSOR
    if (p.cmpOn->load() > 0.5f)
    {
        const float thr = p.cmpThr->load(), ratio = p.cmpRatio->load();
        const float att = std::exp (-1.0f / (p.cmpAtk->load() * 0.001f * (float) sr));
        const float rel = std::exp (-1.0f / (p.cmpRel->load() * 0.001f * (float) sr));
        const float makeup = juce::Decibels::decibelsToGain (-thr * (1.0f - 1.0f / ratio) * 0.5f);
        float maxGr = 0.0f;
        for (int i = 0; i < num; ++i)
        {
            const float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            cmpEnv = x > cmpEnv ? att * cmpEnv + (1 - att) * x : rel * cmpEnv + (1 - rel) * x;
            const float over = juce::Decibels::gainToDecibels (cmpEnv, -100.0f) - thr;
            const float grDb = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            maxGr = juce::jmax (maxGr, grDb);
            const float g = juce::Decibels::decibelsToGain (-grDb) * makeup;
            L[i] *= g; R[i] *= g;
        }
        gainReductionDb.store (juce::jmax (maxGr, gainReductionDb.load() * 0.9f));
    }
    else gainReductionDb.store (0.0f);

    // ---- DELAY (synced to the DAW tempo)
    if (p.dlyOn->load() > 0.5f)
    {
        static constexpr double beats[] = { 1.0, 0.5, 0.75, 0.25 };
        const int sync = juce::jlimit (0, 3, (int) p.dlySync->load());
        const int size = delayLine.getNumSamples();
        const int d = juce::jlimit (1, size - 1, (int) (beats[sync] * 60.0 / bpm * sr));
        const float fb = p.dlyFb->load(), mix = p.dlyMix->load();
        auto* bl = delayLine.getWritePointer (0);
        auto* br = delayLine.getWritePointer (1);
        for (int i = 0; i < num; ++i)
        {
            const int r = (delayWrite - d + size) % size;
            const float wl = bl[r], wr = br[r];
            dampL += 0.4f * (wl - dampL);
            dampR += 0.4f * (wr - dampR);
            bl[delayWrite] = L[i] + dampL * fb;
            br[delayWrite] = R[i] + dampR * fb;
            delayWrite = (delayWrite + 1) % size;
            L[i] += wl * mix; R[i] += wr * mix;
        }
    }

    // ---- REVERB
    if (p.revOn->load() > 0.5f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.25f + 0.73f * p.revDecay->load();
        rp.width = 0.4f + 0.6f * p.revSize->load();
        rp.damping = 0.9f - 0.8f * p.revTone->load();
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
        if (rp.roomSize != lastRev.roomSize || rp.width != lastRev.width || rp.damping != lastRev.damping)
        {
            reverb.setParameters (rp);
            lastRev = rp;
        }
        auto* wL = wet.getWritePointer (0);
        auto* wR = wet.getWritePointer (1);
        std::copy (L, L + num, wL);
        std::copy (R, R + num, wR);
        reverb.processStereo (wL, wR, num);
        const float mix = p.revMix->load();
        const float dry = 1.0f - 0.5f * mix;
        for (int i = 0; i < num; ++i)
        {
            L[i] = L[i] * dry + wL[i] * mix * 0.7f;
            R[i] = R[i] * dry + wR[i] * mix * 0.7f;
        }
    }
}

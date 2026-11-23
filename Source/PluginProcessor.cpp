#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    void resetParam (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    }

    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.85f) return x;
        return std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f), x);
    }

    constexpr const char* editIds[] = { "start", "end", "fadeIn", "fadeOut", "offset", "stretch", "reverse", "normalize", "snap" };
}

// ---------------------------------------------------------------------------
EraProcessor::EraProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ERA", createLayout())
{
    Library::get(); // decode the built-in sounds now, not on the audio thread

    for (int p = 0; p < numPads; ++p)
    {
        auto g = [&] (const char* w) { return apvts.getRawParameterValue (pid (p, w)); };
        pp[(size_t) p] = { g ("vol"), g ("pan"), g ("tune"), g ("attack"), g ("decay"), g ("mute"), g ("solo"),
                           g ("start"), g ("end"), g ("fadeIn"), g ("fadeOut"), g ("offset"), g ("stretch"),
                           g ("reverse"), g ("normalize"), g ("snap"),
                           g ("layer"), g ("blend"), g ("ltune"), g ("lattack"), g ("ldecay"), g ("lvol") };
        pendingTriggers[(size_t) p].store (0.0f);
        hitCounts[(size_t) p].store (0);
        padLevels[(size_t) p].store (0.0f);

        const auto sounds = Library::get().inCategory (p);
        if (! sounds.empty())
            setPadSample (p, sounds.front());
    }
    for (int c = 0; c <= numPads; ++c)
        fxp[(size_t) c] = FxChain::bind (apvts, fxPrefix (c));

    masterP = apvts.getRawParameterValue ("master");
    widthP  = apvts.getRawParameterValue ("width");
    limitP  = apvts.getRawParameterValue ("limit");

    timerCallback();
    startTimerHz (20);
}

EraProcessor::~EraProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout EraProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto ms  = [] (float v, int) { return String (roundToInt (v)) + " ms"; };

    for (int p = 0; p < numPads; ++p)
    {
        const String name = String (padInfo[(size_t) p].name) + " ";
        auto f = [&] (const char* id, const String& label, NormalisableRange<float> r, float def,
                      std::function<String (float, int)> text = nullptr, const String& unit = {})
        {
            auto a = AudioParameterFloatAttributes().withLabel (unit);
            if (text) a = a.withStringFromValueFunction (text);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid (p, id), 1 }, name + label, r, def, a));
        };
        auto b = [&] (const char* id, const String& label, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid (p, id), 1 }, name + label, def));
        };

        f ("vol", "Volume", { 0.0f, 2.0f, 0.01f }, 1.0f, [] (float v, int) { return v <= 0.001f ? String ("-inf dB") : String (Decibels::gainToDecibels (v), 1) + " dB"; });
        f ("pan", "Pan", { -1.0f, 1.0f, 0.01f }, 0.0f, [] (float v, int) { return std::abs (v) < 0.01f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100.0f)); });
        f ("tune", "Tune", { -24.0f, 24.0f, 0.01f }, 0.0f, [] (float v, int) { return String (v, 1) + " st"; });
        f ("attack", "Attack", { 0.0f, 200.0f, 0.1f, 0.4f }, 0.0f, ms);
        f ("decay", "Decay", { 0.0f, 1.0f, 0.01f }, 1.0f, [] (float v, int) { return v >= 0.995f ? String ("Full") : String (roundToInt (v * 100.0f)) + " %"; });
        b ("mute", "Mute", false);
        b ("solo", "Solo", false);

        f ("start", "Start", { 0.0f, 1.0f, 0.0001f }, 0.0f, pct);
        f ("end", "End", { 0.0f, 1.0f, 0.0001f }, 1.0f, pct);
        f ("fadeIn", "Fade In", { 0.0f, 500.0f, 0.1f, 0.4f }, 0.0f, ms);
        f ("fadeOut", "Fade Out", { 0.0f, 1000.0f, 0.1f, 0.4f }, 0.0f, ms);
        f ("offset", "Offset", { 0.0f, 100.0f, 0.1f }, 0.0f, ms);
        f ("stretch", "Stretch", { 0.5f, 2.0f, 0.01f, 0.63f }, 1.0f, [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
        b ("reverse", "Reverse", false);
        b ("normalize", "Normalize", false);
        b ("snap", "Snap", true);

        b ("layer", "Layer On", false);
        f ("blend", "Layer Blend", { 0.0f, 1.0f, 0.01f }, 0.5f, pct);
        f ("ltune", "Layer Tune", { -24.0f, 24.0f, 0.01f }, 0.0f, [] (float v, int) { return String (v, 1) + " st"; });
        f ("lattack", "Layer Attack", { 0.0f, 200.0f, 0.1f, 0.4f }, 0.0f, ms);
        f ("ldecay", "Layer Decay", { 0.0f, 1.0f, 0.01f }, 1.0f, [] (float v, int) { return v >= 0.995f ? String ("Full") : String (roundToInt (v * 100.0f)) + " %"; });
        f ("lvol", "Layer Volume", { 0.0f, 2.0f, 0.01f }, 1.0f, [] (float v, int) { return v <= 0.001f ? String ("-inf dB") : String (Decibels::gainToDecibels (v), 1) + " dB"; });

        FxChain::addParameters (layout, fxPrefix (p), name);
    }
    FxChain::addParameters (layout, fxPrefix (masterChain), "Master ");

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master Volume",
                                                       NormalisableRange<float> (0.0f, 2.0f, 0.01f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Master Width",
                                                       NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "limit", 1 }, "Master Limit",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return "+" + String (v * 12.0f, 1) + " dB"; })));
    return layout;
}

bool EraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EraProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fadeLength = juce::jmax (16, (int) (sampleRate * 0.004));
    for (auto& v : voices) { v.active = false; v.data.reset(); }

    padBus.setSize (numPads * 2, chunk);
    previewBus.setSize (2, chunk);

    for (auto& c : chains)
        c.prepare (sampleRate);
    for (size_t p = 0; p < (size_t) numPads; ++p)
    {
        gainL[p].reset (sampleRate, 0.02);
        gainR[p].reset (sampleRate, 0.02);
    }
    masterGain.reset (sampleRate, 0.02);
    widthS.reset (sampleRate, 0.05);
    limiterEnv = 0.0f;
}

// ---------------------------------------------------------------------------
// Pads
void EraProcessor::setPadSample (int pad, SamplePtr s)
{
    const juce::ScopedLock sl (retireLock);
    auto old = std::atomic_exchange (&padSamples[(size_t) pad], std::move (s));
    if (old != nullptr)
        retired.push_back (std::move (old)); // freed by the timer, never on the audio thread
}

bool EraProcessor::assignSample (int pad, SamplePtr s)
{
    if (pad < 0 || pad >= numPads || ! canAssign (pad, s))
        return false;
    setPadSample (pad, std::move (s));
    timerCallback();
    return true;
}

void EraProcessor::assignLayer (int pad, SamplePtr s)
{
    if (pad < 0 || pad >= numPads)
        return;
    const juce::ScopedLock sl (retireLock);
    auto old = std::atomic_exchange (&layerSamples[(size_t) pad], std::move (s));
    if (old != nullptr)
        retired.push_back (std::move (old));
}

void EraProcessor::stepSound (int pad, int direction)
{
    if (auto next = Library::get().neighbour (getPadSample (pad), pad, direction))
        assignSample (pad, next);
}

void EraProcessor::preview (SamplePtr s)
{
    std::atomic_store (&previewSample, std::move (s));
}

void EraProcessor::resetSampleEdit (int pad)
{
    for (auto* id : editIds)
        resetParam (apvts, pid (pad, id));
}

void EraProcessor::resetFx (int chain)
{
    for (const auto& id : FxChain::ids())
        resetParam (apvts, fxPrefix (chain) + id);
}

SampleEditSettings EraProcessor::getEditSettings (int pad) const
{
    const auto& p = pp[(size_t) pad];
    SampleEditSettings e;
    e.start = p.start->load();
    e.end = p.end->load();
    e.fadeInMs = p.fadeIn->load();
    e.fadeOutMs = p.fadeOut->load();
    e.stretch = p.stretch->load();
    e.reverse = p.reverse->load() > 0.5f;
    e.normalize = p.normalize->load() > 0.5f;
    e.snap = p.snap->load() > 0.5f;
    return e;
}

// Message thread: render the edited sounds and free the ones nobody plays any more.
void EraProcessor::timerCallback()
{
    const juce::ScopedLock sl (retireLock);
    for (int p = 0; p < numPads; ++p)
    {
        const auto src = getPadSample (p);
        const auto settings = getEditSettings (p);
        if (src == renderedFrom[(size_t) p] && settings == renderedSettings[(size_t) p] && src != nullptr)
            continue;

        renderedFrom[(size_t) p] = src;
        renderedSettings[(size_t) p] = settings;
        auto old = std::atomic_exchange (&editedSamples[(size_t) p], SampleEdit::render (src, settings));
        if (old != nullptr)
            retired.push_back (std::move (old));
    }

    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const SamplePtr& s) { return s.use_count() <= 1; }),
                   retired.end());
}

// ---------------------------------------------------------------------------
// Voices
void EraProcessor::startOneVoice (int pad, SamplePtr data, float gain, float tune, float attackMs, float decay, int delaySamples)
{
    if (data == nullptr || data->buffer.getNumSamples() < 2 || gain <= 0.0001f)
        return;

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.order < target->order) target = &v;
    }

    target->data = std::move (data);
    target->position = 0.0;
    target->increment = (target->data->sampleRate / sr) * std::pow (2.0, tune / 12.0);
    target->gain = gain;
    target->env = 1.0f;
    target->envCoef = decay >= 0.995f ? 1.0f : (float) std::exp (-1.0 / (0.02 * std::pow (100.0, (double) decay) * sr));
    target->attackSamples = (int) (attackMs * 0.001 * sr);
    target->age = 0;
    target->startDelay = delaySamples;
    target->pad = pad;
    target->fadeRemaining = 0;
    target->order = ++voiceCounter;
    target->active = true;
}

void EraProcessor::startVoice (int pad, float velocity)
{
    const float vel = juce::jmap (juce::jlimit (0.0f, 1.0f, velocity), 0.15f, 1.0f);
    const auto& prm = pp[(size_t) pad];
    auto main = std::atomic_load (&editedSamples[(size_t) pad]);
    auto layer = prm.layer->load() > 0.5f ? getLayerSample (pad) : nullptr;

    // Blend: 0 = A only, 50 % = both full, 100 % = B only
    const float b = prm.blend->load();
    const float gainA = layer != nullptr ? juce::jmin (1.0f, 2.0f * (1.0f - b)) : 1.0f;
    const float gainB = juce::jmin (1.0f, 2.0f * b) * prm.lVol->load();
    const int offset = (int) (prm.offset->load() * 0.001 * sr);

    startOneVoice (pad, main, vel * gainA, prm.tune->load(), prm.attack->load(), prm.decay->load(), offset);
    if (layer != nullptr)
        startOneVoice (pad, layer, vel * gainB, prm.lTune->load(), prm.lAttack->load(), prm.lDecay->load(), 0);

    if (main != nullptr || layer != nullptr)
        hitCounts[(size_t) pad].fetch_add (1);
}

void EraProcessor::renderVoices (int from, int to)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const auto& buf = v.data->buffer;
        const int len = buf.getNumSamples();
        const float* l = buf.getReadPointer (0);
        const float* r = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
        float* outL = v.pad < numPads ? padBus.getWritePointer (v.pad * 2)     : previewBus.getWritePointer (0);
        float* outR = v.pad < numPads ? padBus.getWritePointer (v.pad * 2 + 1) : previewBus.getWritePointer (1);

        for (int i = from; i < to; ++i)
        {
            if (v.startDelay > 0) { --v.startDelay; continue; }

            const int idx = (int) v.position;
            if (idx + 1 >= len || v.env < 0.0003f) { v.active = false; break; }

            const float frac = (float) (v.position - idx);
            float g = v.gain * v.env;
            v.env *= v.envCoef;
            if (v.age < v.attackSamples) g *= (float) v.age / (float) v.attackSamples;
            ++v.age;

            if (v.fadeRemaining > 0)
            {
                g *= (float) v.fadeRemaining / (float) fadeLength;
                if (--v.fadeRemaining == 0) v.active = false;
            }

            const int o = i - chunkStart;
            outL[o] += (l[idx] + frac * (l[idx + 1] - l[idx])) * g;
            outR[o] += (r[idx] + frac * (r[idx + 1] - r[idx])) * g;
            v.position += v.increment;
            if (! v.active) break;
        }
    }
}

void EraProcessor::mixChunk (juce::AudioBuffer<float>& out, int start, int num)
{
    bool anySolo = false;
    for (const auto& p : pp)
        anySolo = anySolo || p.solo->load() > 0.5f;

    auto* oL = out.getWritePointer (0, start);
    auto* oR = out.getWritePointer (1, start);

    for (size_t p = 0; p < (size_t) numPads; ++p)
    {
        const auto& prm = pp[p];
        const bool audible = prm.mute->load() < 0.5f && (! anySolo || prm.solo->load() > 0.5f);
        const float vol = audible ? prm.vol->load() : 0.0f;
        const float angle = (prm.pan->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[p].setTargetValue (vol * std::cos (angle) * juce::MathConstants<float>::sqrt2);
        gainR[p].setTargetValue (vol * std::sin (angle) * juce::MathConstants<float>::sqrt2);

        float* L = padBus.getWritePointer ((int) p * 2);
        float* R = padBus.getWritePointer ((int) p * 2 + 1);
        chains[p].process (fxp[p], L, R, num, bpm);

        float peak = 0.0f;
        for (int i = 0; i < num; ++i)
        {
            const float l = L[i] * gainL[p].getNextValue();
            const float r = R[i] * gainR[p].getNextValue();
            oL[i] += l; oR[i] += r;
            peak = juce::jmax (peak, std::abs (l), std::abs (r));
        }
        if (peak > padLevels[p].load())
            padLevels[p].store (peak);
    }

    chains[(size_t) masterChain].process (fxp[(size_t) masterChain], oL, oR, num, bpm);

    // Width (mid/side), then LIMIT pushes the level into a smooth peak limiter.
    widthS.setTargetValue (widthP->load());
    const float drive = juce::Decibels::decibelsToGain (limitP->load() * 12.0f);
    const float ceiling = 0.944f; // -0.5 dB
    const float rel = std::exp (-1.0f / (0.08f * (float) sr));
    auto* pL = previewBus.getReadPointer (0);
    auto* pR = previewBus.getReadPointer (1);
    float peak = 0.0f;
    for (int i = 0; i < num; ++i)
    {
        const float w = widthS.getNextValue();
        const float mid = 0.5f * (oL[i] + oR[i]), side = 0.5f * (oL[i] - oR[i]) * w;
        float l = (mid + side) * drive, r = (mid - side) * drive;

        const float a = juce::jmax (std::abs (l), std::abs (r));
        limiterEnv = a > limiterEnv ? a : rel * limiterEnv + (1.0f - rel) * a;
        if (limiterEnv > ceiling) { const float g = ceiling / limiterEnv; l *= g; r *= g; }

        const float m = masterGain.getNextValue();
        l = softClip ((l + pL[i]) * m);
        r = softClip ((r + pR[i]) * m);
        oL[i] = l; oR[i] = r;
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    if (peak > masterLevel.load())
        masterLevel.store (peak);
}

void EraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *b);

    std::array<Event, 256> events;
    size_t numEvents = 0;
    auto add = [&] (Event e) { if (numEvents < events.size()) events[numEvents++] = e; };

    for (int p = 0; p < numPads; ++p)
    {
        const float v = pendingTriggers[(size_t) p].exchange (0.0f);
        if (v > 0.0f) add ({ 0, p, v });
    }

    if (auto s = std::atomic_exchange (&previewSample, SamplePtr()))
    {
        for (auto& v : voices)
            if (v.active && v.pad == numPads && v.fadeRemaining == 0)
                v.fadeRemaining = fadeLength;
        startOneVoice (numPads, std::move (s), 0.9f, 0.0f, 0.0f, 1.0f, 0);
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            midiCount.fetch_add (1);
            for (int p = 0; p < numPads; ++p)
                if (msg.getNoteNumber() == padInfo[(size_t) p].note)
                    add ({ juce::jlimit (0, n - 1, meta.samplePosition), p, msg.getFloatVelocity() });
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& v : voices)
                if (v.active && v.fadeRemaining == 0) v.fadeRemaining = fadeLength;
    }

    std::stable_sort (events.begin(), events.begin() + (long) numEvents,
                      [] (const Event& a, const Event& b) { return a.pos < b.pos; });
    masterGain.setTargetValue (masterP->load());

    size_t e = 0;
    for (chunkStart = 0; chunkStart < n; chunkStart += chunk)
    {
        const int chunkEnd = juce::jmin (n, chunkStart + chunk);
        padBus.clear();
        previewBus.clear();

        int cursor = chunkStart;
        while (e < numEvents && events[e].pos < chunkEnd)
        {
            const auto& ev = events[e++];
            if (ev.pos > cursor) { renderVoices (cursor, ev.pos); cursor = ev.pos; }
            startVoice (ev.pad, ev.velocity);
        }
        renderVoices (cursor, chunkEnd);
        mixChunk (buffer, chunkStart, chunkEnd - chunkStart);
    }
    chunkStart = 0;

    // Safe on the audio thread: the library, the pads or `retired` always hold another reference.
    for (auto& v : voices)
        if (! v.active && v.data != nullptr)
            v.data.reset();
}

// ---------------------------------------------------------------------------
// State
void EraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("ERASTATE");
    root.setAttribute ("version", "1.0");
    root.addChildElement (apvts.copyState().createXml().release());
    auto* pads = root.createNewChildElement ("PADS");
    for (int p = 0; p < numPads; ++p)
    {
        auto* e = pads->createNewChildElement ("PAD");
        e->setAttribute ("i", p);
        if (auto s = getPadSample (p))   e->setAttribute ("sample", s->id);
        if (auto s = getLayerSample (p)) e->setAttribute ("layer", s->id);
    }
    copyXmlToBinary (root, destData);
}

void EraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("ERASTATE"))
        return;

    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    if (auto* pads = xml->getChildByName ("PADS"))
        for (auto* e : pads->getChildWithTagNameIterator ("PAD"))
        {
            const int p = e->getIntAttribute ("i", -1);
            if (p < 0 || p >= numPads)
                continue;
            if (auto s = Library::get().find (e->getStringAttribute ("sample")); canAssign (p, s))
                setPadSample (p, s);
            assignLayer (p, Library::get().find (e->getStringAttribute ("layer")));
        }
    timerCallback();
}

juce::AudioProcessorEditor* EraProcessor::createEditor() { return new EraEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EraProcessor(); }

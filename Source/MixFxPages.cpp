#include "PluginEditor.h"
#include <complex>

using namespace Theme;

// ===========================================================================
Fader::Fader (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    addAndMakeVisible (slider);
}

void Fader::resized() { slider.setBounds (getLocalBounds()); }

// ===========================================================================
MixPage::MixPage (MainView& m)
    : main (m), width (m.proc.apvts, "WIDTH", 44), limit (m.proc.apvts, "LIMIT", 44), masterFader (m.proc.apvts, "master")
{
    for (int p = 0; p < numPads; ++p)
    {
        auto* k = pans.add (new Knob (m.proc.apvts, "PAN", 44));
        k->bind (EraProcessor::pid (p, "pan"));
        addAndMakeVisible (k);
        auto* f = faders.add (new Fader (m.proc.apvts, EraProcessor::pid (p, "vol")));
        f->slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (f);
        auto* mu = mutes.add (new ParamToggle (m.proc.apvts, "M"));
        auto* so = solos.add (new ParamToggle (m.proc.apvts, "S"));
        mu->bind (EraProcessor::pid (p, "mute"));
        so->bind (EraProcessor::pid (p, "solo"));
        addAndMakeVisible (mu);
        addAndMakeVisible (so);
    }
    width.bind ("width");
    limit.bind ("limit");
    addAndMakeVisible (width);
    addAndMakeVisible (limit);
    masterFader.slider.getProperties().set ("master", true);
    masterFader.slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (masterFader);
}

juce::Rectangle<int> MixPage::strip (int i) const
{
    const int w = (getWidth() - 32 - 8 * numPads) / (numPads + 1);
    return { 16 + i * (w + 8), 44, w, getHeight() - 44 - 16 };
}

void MixPage::resized()
{
    for (int i = 0; i <= numPads; ++i)
    {
        auto r = strip (i).reduced (6, 12);
        r.removeFromTop (24);
        auto knobs = r.removeFromTop (64);
        auto buttons = r.removeFromBottom (26);
        r.removeFromBottom (28);
        auto faderArea = r.reduced (0, 8);
        const int fx = faderArea.getCentreX() - 22;
        if (i < numPads)
        {
            pans[i]->setBounds (knobs.withSizeKeepingCentre (64, 64));
            faders[i]->setBounds (fx, faderArea.getY(), 30, faderArea.getHeight());
            auto b = buttons.withSizeKeepingCentre (64, 26);
            mutes[i]->setBounds (b.removeFromLeft (30));
            b.removeFromLeft (4);
            solos[i]->setBounds (b);
        }
        else
        {
            width.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
            limit.setBounds (knobs);
            masterFader.setBounds (fx, faderArea.getY(), 30, faderArea.getHeight());
        }
    }
}

void MixPage::tick()
{
    for (int p = 0; p < numPads; ++p)
        levels[(size_t) p] = juce::jmax (main.proc.takePadLevel (p), levels[(size_t) p] * 0.85f);
    levels[numPads] = juce::jmax (main.proc.takeMasterLevel(), levels[numPads] * 0.85f);
    repaint();
}

void MixPage::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), "MIXER");
    g.setColour (faint);
    g.setFont (mono (11.0f));
    g.drawText ("PAN - VOLUME - MUTE / SOLO", getLocalBounds().reduced (16, 12).removeFromTop (16), juce::Justification::centredRight);

    for (int i = 0; i <= numPads; ++i)
    {
        const bool master = i == numPads;
        auto r = strip (i).toFloat();
        g.setColour (master ? bg.interpolatedWith (lime, 0.07f) : panel2);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (master ? lime.withAlpha (0.4f) : juce::Colour (0xff262a34));
        g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 2.0f);

        auto in = r.reduced (6.0f, 12.0f);
        g.setColour (master ? lime : (i == main.getSelectedPad() ? lime : text));
        drawSpaced (g, master ? "MASTER" : padInfo[(size_t) i].name, in.removeFromTop (20.0f), sans (12.0f), 0.12f, juce::Justification::centred);

        // meter next to the fader
        juce::Component* f = master ? static_cast<juce::Component*> (&masterFader) : faders[i];
        auto fb = f->getBounds().toFloat();
        auto meter = juce::Rectangle<float> (fb.getRight() + 10.0f, fb.getY() + 10.0f, 8.0f, fb.getHeight() - 20.0f);
        const int segs = 24;
        const float sh = meter.getHeight() / (float) segs;
        const float db = juce::Decibels::gainToDecibels (levels[(size_t) i], -60.0f);
        const float lit = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        for (int s = 0; s < segs; ++s)
        {
            const float pos = (float) s / (float) segs;
            auto seg = juce::Rectangle<float> (meter.getX(), meter.getBottom() - (float) (s + 1) * sh + 1.5f, meter.getWidth(), sh - 3.0f);
            g.setColour (pos < lit ? (pos < 0.7f ? lime : pos < 0.85f ? amber : red) : line);
            g.fillRoundedRectangle (seg, 1.0f);
        }

        // volume in dB under the fader
        const auto* p = main.proc.apvts.getParameter (master ? juce::String ("master") : EraProcessor::pid (i, "vol"));
        const float v = p->convertFrom0to1 (p->getValue());
        g.setColour (master ? lime : dim);
        g.setFont (mono (11.0f));
        g.drawText (v <= 0.001f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB",
                    juce::Rectangle<float> (r.getX(), fb.getBottom() + 8.0f, r.getWidth(), 16.0f), juce::Justification::centred);
    }
}

// ===========================================================================
FxModule::FxModule (MainView& m, const juce::String& t, const juce::String& on, juce::StringArray knobIds, juce::StringArray captions)
    : main (m), title (t), onId (on), ids (knobIds), onButton (m.proc.apvts, "OFF")
{
    onButton.getProperties().set ("fontSize", 10.0f);
    onButton.onStateChange = [this] { onButton.setButtonText (onButton.getToggleState() ? "ON" : "OFF"); repaint(); };
    addAndMakeVisible (onButton);
    for (int i = 0; i < knobIds.size(); ++i)
        addAndMakeVisible (knobs.add (new Knob (m.proc.apvts, captions[i], 60)));
}

void FxModule::bind (int chain)
{
    const auto prefix = EraProcessor::fxPrefix (chain);
    onButton.bind (prefix + onId);
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->bind (prefix + ids[i]);
    repaint();
}

void FxModule::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), title);
    if (! onButton.getToggleState())
    {
        g.setColour (faint);
        g.setFont (mono (9.5f));
        g.drawText ("BYPASSED", getLocalBounds().reduced (16, 14).removeFromBottom (14), juce::Justification::centredRight);
    }
    if (drawExtra)
        drawExtra (g, extraArea);
}

void FxModule::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    onButton.setBounds (r.removeFromTop (28).removeFromRight (62));
    r.removeFromTop (10);
    if (extraHeight > 0)
    {
        auto e = r.removeFromTop (extraHeight);
        extraArea = e.toFloat();
        if (extra != nullptr) extra->setBounds (e);
        r.removeFromTop (12);
    }
    auto knobRow = r.withSizeKeepingCentre (r.getWidth(), 80);
    const int w = knobRow.getWidth() / juce::jmax (1, knobs.size());
    for (auto* k : knobs)
        k->setBounds (knobRow.removeFromLeft (w));
}

// ===========================================================================
namespace
{
    // dB response of the three EQ bands at a frequency (for the curve on the EQ module)
    float eqResponseDb (float freq, float lowDb, float midDb, float highDb)
    {
        const double sr = 48000.0;
        Biquad bands[3];
        bands[0].setShelf (false, sr, 120.0f, lowDb);
        bands[1].setPeak (sr, 1200.0f, 0.8f, midDb);
        bands[2].setShelf (true, sr, 7500.0f, highDb);
        const std::complex<double> z = std::polar (1.0, juce::MathConstants<double>::twoPi * freq / sr);
        double mag = 1.0;
        for (auto& b : bands)
        {
            const auto zi = 1.0 / z, zi2 = zi * zi;
            const auto num = (double) b.b0 + (double) b.b1 * zi + (double) b.b2 * zi2;
            const auto den = 1.0 + (double) b.a1 * zi + (double) b.a2 * zi2;
            mag *= std::abs (num / den);
        }
        return (float) juce::Decibels::gainToDecibels (mag);
    }
}

FxPage::FxPage (MainView& m)
    : main (m),
      sync (m.proc.apvts, { "1/4", "1/8", "1/8D", "1/16" }),
      filterType (m.proc.apvts, { "LP", "HP", "BP" }),
      delay (m, "DELAY", "dlyOn", { "dlyFb", "dlyMix" }, { "FEEDBACK", "MIX" }),
      filter (m, "FILTER", "fltOn", { "fltCut", "fltRes", "fltEnv" }, { "CUTOFF", "RESO", "ENV" }),
      eq (m, "EQUALIZER", "eqOn", { "eqLow", "eqMid", "eqHigh" }, { "LOW", "MID", "HIGH" }),
      reverb (m, "REVERB", "revOn", { "revSize", "revDecay", "revTone", "revMix" }, { "SIZE", "DECAY", "TONE", "MIX" }),
      comp (m, "COMPRESSOR", "cmpOn", { "cmpThr", "cmpRatio", "cmpAtk", "cmpRel" }, { "THRESH", "RATIO", "ATTACK", "RELEASE" }),
      transient (m, "TRANSIENT", "trnOn", { "trnPunch", "trnSus" }, { "PUNCH", "SUSTAIN" })
{
    for (int c = 0; c <= numPads; ++c)
    {
        auto* b = targets.add (new juce::TextButton (c < numPads ? juce::String (padInfo[(size_t) c].name) : juce::String ("MASTER")));
        b->getProperties().set ("chip", true);
        b->getProperties().set ("fontSize", 10.5f);
        b->onClick = [this, c] { main.setFxTarget (c); };
        addAndMakeVisible (b);
    }
    resetButton.getProperties().set ("fontSize", 10.0f);
    resetButton.onClick = [this] { main.proc.resetFx (main.getFxTarget()); };
    addAndMakeVisible (resetButton);

    delay.extra = &sync;       delay.extraHeight = 30;     delay.addAndMakeVisible (sync);
    filter.extra = &filterType; filter.extraHeight = 30;   filter.addAndMakeVisible (filterType);
    eq.extraHeight = 120;
    eq.drawExtra = [this] (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (lcd);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (line);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * std::log10 (f / 20.0f) / 3.0f;
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        const auto pre = EraProcessor::fxPrefix (main.getFxTarget());
        auto val = [&] (const char* id) { return main.proc.apvts.getRawParameterValue (pre + id)->load(); };
        const bool on = val ("eqOn") > 0.5f;
        const float lo = val ("eqLow"), mi = val ("eqMid"), hi = val ("eqHigh");
        juce::Path curve, area;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float f = 20.0f * std::pow (1000.0f, t);
            const float db = on ? eqResponseDb (f, lo, mi, hi) : 0.0f;
            const float y = r.getCentreY() - juce::jlimit (-15.0f, 15.0f, db) / 15.0f * (r.getHeight() * 0.45f);
            const float x = r.getX() + t * r.getWidth();
            if (i == 0) { curve.startNewSubPath (x, y); area.startNewSubPath (x, r.getBottom()); }
            else curve.lineTo (x, y);
            area.lineTo (x, y);
        }
        area.lineTo (r.getRight(), r.getBottom());
        area.closeSubPath();
        g.setColour (lime.withAlpha (on ? 0.08f : 0.03f));
        g.fillPath (area);
        g.setColour (on ? lime : off);
        g.strokePath (curve, juce::PathStrokeType (2.5f));
        for (auto [f, db] : { std::pair<float, float> { 60.0f, lo }, { 1200.0f, mi }, { 9000.0f, hi } })
        {
            const float x = r.getX() + r.getWidth() * std::log10 (f / 20.0f) / 3.0f;
            const float y = r.getCentreY() - (on ? juce::jlimit (-15.0f, 15.0f, eqResponseDb (f, lo, mi, hi)) : 0.0f) / 15.0f * (r.getHeight() * 0.45f);
            g.setColour (on ? lime : off);
            g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre ({ x, y }));
        }
    };
    comp.extraHeight = 20;
    comp.drawExtra = [this] (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (dim);
        g.setFont (sans (9.0f));
        g.drawText ("GR", r.removeFromLeft (24.0f), juce::Justification::centredLeft);
        auto txt = r.removeFromRight (56.0f);
        g.setColour (amber);
        g.setFont (mono (10.0f));
        g.drawText ("-" + juce::String (gr, 1) + " dB", txt, juce::Justification::centredRight);
        const int segs = 16;
        const float w = (r.getWidth() - 8.0f) / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            g.setColour ((float) i < gr / 20.0f * (float) segs ? amber : line);
            g.fillRoundedRectangle (r.getX() + (float) i * w, r.getCentreY() - 4.0f, w - 3.0f, 8.0f, 2.0f);
        }
    };

    for (auto* mod : { &delay, &filter, &eq, &reverb, &comp, &transient })
        addAndMakeVisible (mod);
}

void FxPage::refresh()
{
    const int t = main.getFxTarget();
    for (int i = 0; i < targets.size(); ++i)
        targets[i]->setToggleState (i == t, juce::dontSendNotification);
    const auto pre = EraProcessor::fxPrefix (t);
    sync.bind (pre + "dlySync");
    filterType.bind (pre + "fltType");
    for (auto* mod : { &delay, &filter, &eq, &reverb, &comp, &transient })
        mod->bind (t);
    repaint();
}

void FxPage::tick()
{
    gr = juce::jmax (main.proc.getGainReduction (main.getFxTarget()), gr * 0.85f);
    comp.repaint();
    eq.repaint();
}

void FxPage::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), "FX RACK");
    g.setColour (faint);
    g.setFont (mono (11.0f));
    g.drawText ("SIGNAL: TRANSIENT > FILTER > EQ > COMP > DELAY > REVERB", getLocalBounds().reduced (16, 12).removeFromTop (16),
                juce::Justification::centredRight);
    g.setColour (faint);
    drawSpaced (g, "EDIT FX FOR", { 16, 46, 100, 30 }, sans (10.0f), 0.2f, juce::Justification::centredLeft);
    g.setColour (line);
    const auto mb = targets[numPads]->getBounds();
    g.fillRect (mb.getX() - 10, mb.getY() + 4, 1, mb.getHeight() - 8);
}

void FxPage::resized()
{
    int x = 116;
    for (int i = 0; i < targets.size(); ++i)
    {
        if (i == numPads) x += 14;
        const int w = i == numPads ? 86 : 74;
        targets[i]->setBounds (x, 46, w, 30);
        x += w + 6;
    }
    resetButton.setBounds (getWidth() - 16 - 100, 44, 100, 34);

    const int top = 92, gap = 12;
    const int w = (getWidth() - 32 - 2 * gap) / 3;
    const int h = (getHeight() - top - 16 - gap) / 2;
    FxModule* mods[] = { &delay, &filter, &eq, &reverb, &comp, &transient };
    for (int i = 0; i < 6; ++i)
        mods[i]->setBounds (16 + (i % 3) * (w + gap), top + (i / 3) * (h + gap), w, h);
}

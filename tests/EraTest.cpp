// Offline test: plays every pad, pushes every FX and SAMPLE EDIT setting to extremes,
// checks the output is clean, and saves screenshots of the three pages.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::cout << "FAIL: " << msg << std::endl; ++failures; } } while (0)

static void setP (EraProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    if (prm == nullptr) { std::cout << "missing param " << id << std::endl; ++failures; return; }
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static juce::AudioBuffer<float> render (EraProcessor& p, int pad, double seconds, float vel = 1.0f)
{
    const int block = 512, total = (int) (48000 * seconds);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (pos == 0 && pad >= 0) midi.addEvent (juce::MidiMessage::noteOn (10, padInfo[(size_t) pad].note, vel), 17);
        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> b (2, n);
        p.processBlock (b, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, n);
    }
    return out;
}

static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

static void save (const juce::AudioBuffer<float>& b, const juce::String& name)
{
    juce::File f ("/tmp/era_test/" + name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 48000, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File ("/tmp/era_test").createDirectory();

    std::cout << "Library: " << Library::get().all().size() << " sounds" << std::endl;
    for (int c = 0; c < numCategories; ++c)
        CHECK (! Library::get().inCategory (c).empty(), "empty category " << categories[(size_t) c].key);

    EraProcessor p;
    p.prepareToPlay (48000, 512);
    pump (100);

    // 1. every pad sounds
    for (int pad = 0; pad < numPads; ++pad)
    {
        auto b = render (p, pad, 1.5);
        const float peak = b.getMagnitude (0, b.getNumSamples());
        std::cout << padInfo[(size_t) pad].name << " peak " << peak << std::endl;
        CHECK (finite (b), "nan on pad " << pad);
        CHECK (peak > 0.05f && peak <= 1.0f, "level on pad " << pad << " = " << peak);
        save (b, juce::String ("pad_") + padInfo[(size_t) pad].name);
    }

    // 2. silence when nothing plays
    {
        auto b = render (p, -1, 0.5);
        CHECK (b.getMagnitude (0, b.getNumSamples()) < 1.0e-4f, "not silent");
    }

    // 3. SAMPLE EDIT on the snare: cut, reverse, stretch, fades, normalize
    const auto before = render (p, 1, 1.0);
    setP (p, EraProcessor::pid (1, "start"), 0.05f);
    setP (p, EraProcessor::pid (1, "end"), 0.6f);
    setP (p, EraProcessor::pid (1, "reverse"), 1.0f);
    setP (p, EraProcessor::pid (1, "stretch"), 2.0f);
    setP (p, EraProcessor::pid (1, "fadeIn"), 30.0f);
    setP (p, EraProcessor::pid (1, "fadeOut"), 80.0f);
    setP (p, EraProcessor::pid (1, "normalize"), 1.0f);
    setP (p, EraProcessor::pid (1, "offset"), 50.0f);
    pump (200);
    {
        auto b = render (p, 1, 1.5);
        CHECK (finite (b), "nan after sample edit");
        // offset 50 ms: nothing in the first 45 ms
        CHECK (b.getMagnitude (0, (int) (48000 * 0.045)) < 1.0e-4f, "offset not applied");
        CHECK (b.getMagnitude (0, b.getNumSamples()) > 0.05f, "edited snare silent");
        save (b, "snare_edited");
    }
    for (float st : { 0.5f, 0.73f, 1.37f })
    {
        setP (p, EraProcessor::pid (1, "stretch"), st);
        pump (120);
        auto b = render (p, 1, 1.5);
        CHECK (finite (b), "nan at stretch " << st);
    }
    p.resetSampleEdit (1);
    pump (120);
    {
        auto b = render (p, 1, 1.0);
        float diff = 0;
        for (int i = 0; i < b.getNumSamples(); ++i) diff = juce::jmax (diff, std::abs (b.getSample (0, i) - before.getSample (0, i)));
        CHECK (diff < 1.0e-3f, "reset did not restore the snare, diff " << diff);
    }

    // 4. LAYER: snare + clap
    setP (p, EraProcessor::pid (1, "layer"), 1.0f);
    p.assignLayer (1, Library::get().inCategory (6).front());
    setP (p, EraProcessor::pid (1, "blend"), 0.5f);
    {
        auto b = render (p, 1, 1.0);
        CHECK (finite (b), "nan with layer");
        save (b, "snare_plus_clap");
    }
    setP (p, EraProcessor::pid (1, "blend"), 1.0f);   // B only
    setP (p, EraProcessor::pid (1, "ltune"), 7.0f);
    {
        auto b = render (p, 1, 1.0);
        CHECK (b.getMagnitude (0, b.getNumSamples()) > 0.05f, "layer B silent");
    }
    setP (p, EraProcessor::pid (1, "layer"), 0.0f);
    setP (p, EraProcessor::pid (1, "blend"), 0.5f);

    // 5. every FX on, extreme settings, on a pad and on the master
    for (int chain : { 0, (int) EraProcessor::masterChain })
    {
        const auto pre = EraProcessor::fxPrefix (chain);
        for (auto* on : { "dlyOn", "fltOn", "eqOn", "revOn", "cmpOn", "trnOn" }) setP (p, pre + on, 1.0f);
        setP (p, pre + "dlyFb", 0.9f);  setP (p, pre + "dlyMix", 1.0f);  setP (p, pre + "dlySync", 3.0f);
        setP (p, pre + "fltRes", 1.0f); setP (p, pre + "fltEnv", 1.0f);  setP (p, pre + "fltCut", 200.0f);
        setP (p, pre + "eqLow", 12.0f); setP (p, pre + "eqMid", -12.0f); setP (p, pre + "eqHigh", 12.0f);
        setP (p, pre + "revDecay", 1.0f); setP (p, pre + "revMix", 1.0f);
        setP (p, pre + "cmpThr", -40.0f); setP (p, pre + "cmpRatio", 10.0f);
        setP (p, pre + "trnPunch", 1.0f); setP (p, pre + "trnSus", 1.0f);
        float maxGr = 0.0f;
        for (int type = 0; type < 3; ++type)
        {
            setP (p, pre + "fltType", (float) type);
            auto b = render (p, 0, 0.05);
            maxGr = juce::jmax (maxGr, p.getGainReduction (chain));
            b = render (p, -1, 2.0);
            CHECK (finite (b), "nan with fx chain " << chain << " type " << type);
            CHECK (b.getMagnitude (0, b.getNumSamples()) <= 1.0f, "clipping with fx chain " << chain);
            if (type == 0) save (b, "kick_fx_chain_" + juce::String (chain));
        }
        std::cout << "chain " << chain << " GR " << maxGr << " dB" << std::endl;
        CHECK (maxGr > 1.0f, "compressor did nothing on chain " << chain);
        p.resetFx (chain);
    }

    // 6. mute / solo / limit / width
    setP (p, EraProcessor::pid (0, "mute"), 1.0f);
    render (p, -1, 3.0);
    CHECK (render (p, 0, 0.5).getMagnitude (0, 24000) < 1.0e-4f, "mute");
    setP (p, EraProcessor::pid (0, "mute"), 0.0f);
    setP (p, EraProcessor::pid (2, "solo"), 1.0f);
    render (p, -1, 0.2);
    CHECK (render (p, 0, 0.5).getMagnitude (0, 24000) < 1.0e-4f, "solo should silence the kick");
    setP (p, EraProcessor::pid (2, "solo"), 0.0f);
    setP (p, "limit", 1.0f); setP (p, "width", 2.0f);
    {
        auto b = render (p, 0, 1.0);
        CHECK (b.getMagnitude (0, b.getNumSamples()) <= 1.0f, "limit clips");
    }
    setP (p, "limit", 0.0f); setP (p, "width", 1.0f);

    // 7. save / load state
    p.stepSound (3, 1);
    juce::MemoryBlock state;
    p.getStateInformation (state);
    EraProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (q.getPadSample (3) == p.getPadSample (3), "state did not restore pad sound");

    // 8. screenshots of the three pages (+ layer view)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* view = dynamic_cast<MainView*> (ed->getChildComponent (0));
        CHECK (view != nullptr, "no main view");
        auto shot = [&] (const juce::String& name)
        {
            pump (150);
            auto img = view->createComponentSnapshot (view->getLocalBounds(), true, 1.0f);
            juce::File f ("/tmp/era_test/" + name + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        view->selectPad (1, false);
        shot ("home");
        setP (p, EraProcessor::pid (1, "layer"), 1.0f);
        view->setEditingLayerB (true);
        shot ("home_layer");
        setP (p, EraProcessor::pid (1, "layer"), 0.0f);
        view->setEditingLayerB (false);
        setP (p, EraProcessor::pid (1, "start"), 0.08f);
        setP (p, EraProcessor::pid (1, "end"), 0.7f);
        setP (p, EraProcessor::pid (1, "fadeOut"), 120.0f);
        shot ("home_edit");
        view->showPage (1);
        render (p, 0, 0.2);
        shot ("mix");
        setP (p, EraProcessor::fxPrefix (1) + "eqOn", 1.0f);
        setP (p, EraProcessor::fxPrefix (1) + "eqLow", 6.0f);
        setP (p, EraProcessor::fxPrefix (1) + "eqHigh", -4.0f);
        setP (p, EraProcessor::fxPrefix (1) + "dlyOn", 1.0f);
        view->showPage (2);
        shot ("fx");
        for (int a = 1; a < Theme::numThemes; ++a)
        {
            Theme::applyTheme (a);
            view->showPage (0);
            view->repaintEverything();
            shot ("accent_" + juce::String (a));
            if (a == 3) { view->showPage (1); shot ("pink_mix"); view->showPage (2); shot ("pink_fx"); }
        }
        Theme::applyTheme (0);
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURES") << std::endl;
    return failures == 0 ? 0 : 1;
}

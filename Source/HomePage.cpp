#include "PluginEditor.h"

using namespace Theme;

namespace
{
    void drawMiniWave (juce::Graphics& g, const SampleData& s, juce::Rectangle<float> r, juce::Colour c)
    {
        const int bars = juce::jmax (8, (int) (r.getWidth() / 3.0f));
        const float bw = r.getWidth() / (float) bars;
        g.setColour (c);
        for (int i = 0; i < bars; ++i)
        {
            const size_t idx = (size_t) (i * (int) s.peaks.size() / bars);
            const float h = juce::jmax (1.0f, s.peaks[idx] * r.getHeight());
            g.fillRect (r.getX() + (float) i * bw, r.getCentreY() - h * 0.5f, bw * 0.6f, h);
        }
    }

    juce::String shortName (const SamplePtr& s)
    {
        if (s == nullptr) return "empty";
        const auto inner = s->name.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false);
        return inner.isNotEmpty() ? inner : s->name;
    }
}

// ===========================================================================
CategoryList::CategoryList (MainView& m) : main (m) {}

juce::Rectangle<int> CategoryList::rowBounds (int row) const
{
    return { 10, 40 + row * 32, getWidth() - 20, 28 };
}

void CategoryList::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), "CATEGORIES");
    for (int row = 0; row <= numCategories; ++row)
    {
        const int cat = row - 1;
        auto r = rowBounds (row).toFloat();
        const bool sel = main.getLibraryCategory() == cat;
        if (sel)
        {
            g.setColour (lime);
            g.fillRoundedRectangle (r, 6.0f);
        }
        const juce::String label = cat < 0 ? juce::String ("Favorites") : juce::String (categories[(size_t) cat].label);
        const int count = cat < 0 ? main.countFavourites() : (int) Library::get().inCategory (cat).size();
        g.setColour (sel ? bg : text);
        g.setFont (sans (12.5f));
        auto t = r.reduced (10.0f, 0.0f);
        if (cat < 0)
        {
            juce::Path star;
            star.addStar (t.removeFromLeft (14.0f).getCentre(), 5, 2.6f, 6.0f);
            g.fillPath (star);
            t.removeFromLeft (6.0f);
        }
        g.drawText (label, t, juce::Justification::centredLeft);
        g.setColour (sel ? bg.withAlpha (0.7f) : faint);
        g.setFont (mono (10.0f));
        g.drawText (juce::String (count), t, juce::Justification::centredRight);
    }
}

void CategoryList::mouseDown (const juce::MouseEvent& e)
{
    for (int row = 0; row <= numCategories; ++row)
        if (rowBounds (row).contains (e.getPosition()))
        {
            const int cat = row - 1;
            main.setLibraryCategory (cat);
            if (cat >= 0 && ! main.isEditingLayerB())
                main.selectPad (cat, false);
            return;
        }
}

// ===========================================================================
LibraryPanel::LibraryPanel (MainView& m) : main (m)
{
    search.setTextToShowWhenEmpty ("Search sounds...", faint);
    search.setFont (mono (12.0f));
    search.setIndents (12, 8);
    search.onTextChange = [this] { main.searchText = search.getText(); refreshList(); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (30);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    playButton.onClick = [this] { if (current) main.proc.preview (current); };
    favButton.onClick = [this]
    {
        if (current == nullptr) return;
        main.toggleFavourite (current->id);
        if (main.getLibraryCategory() == CategoryList::favourites) refreshList();
        updateButtons();
        list.repaint();
    };
    assignButton.setToggleState (true, juce::dontSendNotification);
    assignButton.onClick = [this] { assign(); };
    for (auto* b : { &playButton, &favButton, &assignButton })
        addAndMakeVisible (b);
}

void LibraryPanel::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), "SAMPLE LIBRARY");
    g.setColour (faint);
    g.setFont (mono (10.0f));
    g.drawText (juce::String ((int) shown.size()) + " sounds", getLocalBounds().reduced (16, 12).removeFromTop (16),
                juce::Justification::centredRight);
}

void LibraryPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    r.removeFromTop (24);
    search.setBounds (r.removeFromTop (32));
    r.removeFromTop (6);
    auto buttons = r.removeFromBottom (35);
    r.removeFromBottom (8);
    list.setBounds (r);
    playButton.setBounds (buttons.removeFromLeft (72));
    buttons.removeFromLeft (8);
    favButton.setBounds (buttons.removeFromLeft (104));
    assignButton.setBounds (buttons.removeFromRight (190));
}

void LibraryPanel::refreshList()
{
    shown.clear();
    const auto q = main.searchText.trim().toLowerCase();
    const int cat = main.getLibraryCategory();
    for (const auto& s : Library::get().all())
    {
        if (q.isNotEmpty())
        {
            if (s->name.toLowerCase().contains (q)) shown.push_back (s);   // search looks in every category
        }
        else if (cat == CategoryList::favourites ? main.isFavourite (s->id) : s->category == cat)
            shown.push_back (s);
    }

    // highlight the sound on the selected pad (or its layer B)
    const int pad = main.getSelectedPad();
    current = main.isEditingLayerB() ? main.proc.getLayerSample (pad) : main.proc.getPadSample (pad);
    list.updateContent();
    list.deselectAllRows();
    for (int i = 0; i < (int) shown.size(); ++i)
        if (shown[(size_t) i] == current)
        {
            list.selectRow (i);
            list.scrollToEnsureRowIsOnscreen (i);
        }
    updateButtons();
    repaint();
}

void LibraryPanel::updateButtons()
{
    favButton.setButtonText (current != nullptr && main.isFavourite (current->id) ? "UNFAVORITE" : "FAVORITE");
    favButton.setEnabled (current != nullptr);
    playButton.setEnabled (current != nullptr);
    const int pad = main.getSelectedPad();
    if (main.isEditingLayerB() && main.isLayerOn (pad))
        assignButton.setButtonText ("ASSIGN TO LAYER B");
    else
        assignButton.setButtonText ("ASSIGN TO " + juce::String (padInfo[(size_t) (current != nullptr ? current->category : pad)].name));
    assignButton.setEnabled (current != nullptr);
}

void LibraryPanel::assign()
{
    if (current == nullptr) return;
    const int pad = main.getSelectedPad();
    auto s = current;
    if (main.isEditingLayerB() && main.isLayerOn (pad))
    {
        main.proc.assignLayer (pad, s);
        main.setEditingLayerB (true);
    }
    else
    {
        main.proc.assignSample (s->category, s);
        if (s->category != pad)
            main.selectPad (s->category, false);
    }
    main.proc.preview (s);
    main.home.control.refresh();
    main.home.pads.repaint();
    main.repaint();
    refreshList();
}

void LibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& s = shown[(size_t) row];
    auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (0.0f, 1.0f);
    if (selected)
    {
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 6.0f);
    }
    r.removeFromLeft (8.0f);
    juce::Path tri;
    auto t = r.removeFromLeft (12.0f).withSizeKeepingCentre (10.0f, 11.0f);
    tri.addTriangle (t.getTopLeft(), t.getBottomLeft(), { t.getRight(), t.getCentreY() });
    g.setColour (lime);
    g.fillPath (tri);
    r.removeFromLeft (10.0f);

    auto starArea = r.removeFromRight (26.0f);
    juce::Path star;
    star.addStar (starArea.getCentre(), 5, 3.0f, 6.5f);
    g.setColour (main.isFavourite (s->id) ? lime : off);
    g.fillPath (star);

    drawMiniWave (g, *s, r.removeFromRight (110.0f).withSizeKeepingCentre (110.0f, 20.0f), lime.withAlpha (0.75f));
    r.removeFromRight (10.0f);
    if (main.getLibraryCategory() == CategoryList::favourites || main.searchText.isNotEmpty())
    {
        g.setColour (faint);
        g.setFont (sans (9.5f));
        g.drawText (juce::String (categories[(size_t) s->category].label).toUpperCase(), r.removeFromRight (50.0f), juce::Justification::centredRight);
    }
    g.setColour (selected ? lime : text);
    g.setFont (sans (12.5f, false));
    g.drawText (s->name, r, juce::Justification::centredLeft);
}

void LibraryPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) shown.size()) return;
    current = shown[(size_t) row];
    if (e.x > list.getVisibleRowWidth() - 30)
    {
        main.toggleFavourite (current->id);
        list.repaint();
    }
    else
        main.proc.preview (current);
    updateButtons();
}

void LibraryPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) shown.size()) return;
    current = shown[(size_t) row];
    assign();
}

// ===========================================================================
SampleEditor::SampleEditor (MainView& m) : main (m) {}

juce::RangedAudioParameter* SampleEditor::param (const char* id) const
{
    return main.proc.apvts.getParameter (EraProcessor::pid (main.getSelectedPad(), id));
}

void SampleEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (lcd);
    g.fillRoundedRectangle (r, 8.0f);
    const int pad = main.getSelectedPad();
    auto s = main.proc.getPadSample (pad);
    if (s == nullptr) return;

    const auto e = main.proc.getEditSettings (pad);
    const auto [a, b] = SampleEdit::snappedRegion (*s, e);
    auto wave = r.reduced (2.0f, 4.0f);
    g.setColour (line);
    g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());

    const int bars = (int) (wave.getWidth() / 4.2f);
    const float bw = wave.getWidth() / (float) bars;
    for (int i = 0; i < bars; ++i)
    {
        const float pos = ((float) i + 0.5f) / (float) bars;
        const float v = s->peaks[(size_t) juce::jlimit (0, (int) s->peaks.size() - 1, (int) (pos * (float) s->peaks.size()))];
        const float h = juce::jmax (1.0f, v * wave.getHeight() * 0.95f);
        const bool inside = pos >= a && pos <= b;
        g.setColour (inside ? lime : off);
        g.fillRect (wave.getX() + (float) i * bw, wave.getCentreY() - h * 0.5f, bw * 0.65f, h);
    }

    const float sx = wave.getX() + a * wave.getWidth(), ex = wave.getX() + b * wave.getWidth();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (r.withRight (sx));
    g.fillRect (r.withLeft (ex));

    // fades (shown on the source, approximately, before STRETCH)
    const float lenSec = (float) s->buffer.getNumSamples() / (float) s->sampleRate;
    const float stretch = juce::jmax (0.5f, e.stretch);
    const float fin  = juce::jmin ((ex - sx) * 0.5f, e.fadeInMs  * 0.001f / stretch / lenSec * wave.getWidth());
    const float fout = juce::jmin ((ex - sx) * 0.5f, e.fadeOutMs * 0.001f / stretch / lenSec * wave.getWidth());
    g.setColour (amber);
    const float dashes[] = { 3.0f, 2.0f };
    if (fin > 1.0f)  g.drawDashedLine ({ sx, r.getBottom(), sx + fin, r.getY() + 3.0f }, dashes, 2, 1.5f);
    if (fout > 1.0f) g.drawDashedLine ({ ex - fout, r.getY() + 3.0f, ex, r.getBottom() }, dashes, 2, 1.5f);

    if (e.reverse)
    {
        g.setColour (amber);
        g.setFont (mono (9.0f));
        g.drawText ("REVERSED", r.reduced (6.0f, 3.0f), juce::Justification::topRight);
    }

    g.setColour (lime);
    g.drawLine (sx, r.getY(), sx, r.getBottom(), 2.0f);
    g.drawLine (ex, r.getY(), ex, r.getBottom(), 2.0f);
    auto hs = juce::Rectangle<float> (sx - 6.0f, r.getY(), 12.0f, 12.0f);
    auto he = juce::Rectangle<float> (ex - 6.0f, r.getBottom() - 12.0f, 12.0f, 12.0f);
    g.fillRoundedRectangle (hs, 2.0f);
    g.fillRoundedRectangle (he, 2.0f);
    g.setColour (bg);
    g.setFont (sans (8.5f));
    g.drawText ("S", hs, juce::Justification::centred);
    g.drawText ("E", he, juce::Justification::centred);
}

void SampleEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto st = main.proc.getEditSettings (main.getSelectedPad());
    const float w = (float) getWidth() - 4.0f;
    const float sx = 2.0f + st.start * w, ex = 2.0f + st.end * w;
    const bool near = std::abs ((float) e.x - sx) < 10.0f || std::abs ((float) e.x - ex) < 10.0f;
    setMouseCursor (near ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SampleEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto st = main.proc.getEditSettings (main.getSelectedPad());
    const float w = (float) getWidth() - 4.0f;
    const float x = (float) e.x;
    dragging = std::abs (x - (2.0f + st.start * w)) <= std::abs (x - (2.0f + st.end * w)) ? param ("start") : param ("end");
    if (dragging != nullptr)
    {
        dragging->beginChangeGesture();
        mouseDrag (e);
    }
}

void SampleEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr) return;
    const auto st = main.proc.getEditSettings (main.getSelectedPad());
    float v = juce::jlimit (0.0f, 1.0f, ((float) e.x - 2.0f) / ((float) getWidth() - 4.0f));
    if (dragging == param ("start")) v = juce::jmin (v, st.end - 0.01f);
    else                             v = juce::jmax (v, st.start + 0.01f);
    dragging->setValueNotifyingHost (dragging->convertTo0to1 (v));
    repaint();
}

void SampleEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragging != nullptr) dragging->endChangeGesture();
    dragging = nullptr;
}

// ===========================================================================
PadControl::PadControl (MainView& m)
    : main (m),
      attack (m.proc.apvts, "ATTACK", 52), decay (m.proc.apvts, "DECAY", 52),
      tune (m.proc.apvts, "TUNE", 52), volume (m.proc.apvts, "VOLUME", 52),
      fadeIn (m.proc.apvts, "FADE IN", 38), fadeOut (m.proc.apvts, "FADE OUT", 38),
      offset (m.proc.apvts, "OFFSET", 38), stretch (m.proc.apvts, "STRETCH", 38),
      reverse (m.proc.apvts, "REVERSE"), normalize (m.proc.apvts, "NORMALIZE"), snap (m.proc.apvts, "SNAP"),
      layer (m.proc.apvts, "LAYER"), editor (m)
{
    for (auto* k : { &attack, &decay, &tune, &volume, &fadeIn, &fadeOut, &offset, &stretch })
        addAndMakeVisible (k);
    for (auto* b : { &reverse, &normalize, &snap })
    {
        b->getProperties().set ("fontSize", 9.0f);
        addAndMakeVisible (b);
    }
    reset.getProperties().set ("fontSize", 9.0f);
    reset.onClick = [this] { main.proc.resetSampleEdit (main.getSelectedPad()); };
    addAndMakeVisible (reset);
    addAndMakeVisible (editor);
    layer.getProperties().set ("fontSize", 12.0f);
    layer.onClick = [this]
    {
        const int pad = main.getSelectedPad();
        if (main.isLayerOn (pad) && main.proc.getLayerSample (pad) == nullptr)
        {
            // start the layer with a clap, a classic raboday stack
            auto claps = Library::get().inCategory (pad == 6 ? 1 : 6);
            if (! claps.empty()) main.proc.assignLayer (pad, claps.front());
        }
        main.setEditingLayerB (main.isLayerOn (pad));
        main.home.pads.repaint();
        main.repaint();
    };
    addAndMakeVisible (layer);

    for (auto* b : { &slotA, &slotB })
    {
        b->getProperties().set ("invisible", true);
        addChildComponent (b);
    }
    slotA.onClick = [this] { main.setEditingLayerB (false); };
    slotB.onClick = [this] { main.setEditingLayerB (true); };

    blend.setSliderStyle (juce::Slider::LinearHorizontal);
    blend.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    blend.onValueChange = [this] { repaint(); };
    addChildComponent (blend);
}

void PadControl::refresh()
{
    const int pad = main.getSelectedPad();
    layerView = main.isLayerOn (pad);
    const bool b = layerView && main.isEditingLayerB();

    attack.bind (EraProcessor::pid (pad, b ? "lattack" : "attack"));
    decay.bind  (EraProcessor::pid (pad, b ? "ldecay" : "decay"));
    tune.bind   (EraProcessor::pid (pad, b ? "ltune" : "tune"));
    volume.bind (EraProcessor::pid (pad, b ? "lvol" : "vol"));
    fadeIn.bind (EraProcessor::pid (pad, "fadeIn"));
    fadeOut.bind (EraProcessor::pid (pad, "fadeOut"));
    offset.bind (EraProcessor::pid (pad, "offset"));
    stretch.bind (EraProcessor::pid (pad, "stretch"));
    reverse.bind (EraProcessor::pid (pad, "reverse"));
    normalize.bind (EraProcessor::pid (pad, "normalize"));
    snap.bind (EraProcessor::pid (pad, "snap"));
    layer.bind (EraProcessor::pid (pad, "layer"));
    blendAttachment.reset();
    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (main.proc.apvts, EraProcessor::pid (pad, "blend"), blend);

    layer.setButtonText (layerView ? "LAYER ON" : "LAYER");
    for (auto* c : std::initializer_list<juce::Component*> { &fadeIn, &fadeOut, &offset, &stretch, &reverse, &normalize, &snap, &reset, &editor })
        c->setVisible (! layerView);
    for (auto* c : std::initializer_list<juce::Component*> { &slotA, &slotB, &blend })
        c->setVisible (layerView);
    resized();
    repaint();
}

void PadControl::tick()
{
    // keep the view in step with automation / host changes
    const int pad = main.getSelectedPad();
    if (main.isLayerOn (pad) != layerView)
    {
        if (! main.isLayerOn (pad)) main.setEditingLayerB (false);
        else refresh();
        main.home.pads.repaint();
        main.repaint();
    }
    if (! layerView)
    {
        juce::String t;
        if (auto s = main.proc.getPadSample (pad))
        {
            const auto [a, b] = SampleEdit::snappedRegion (*s, main.proc.getEditSettings (pad));
            const double len = s->buffer.getNumSamples() / s->sampleRate;
            t = juce::String (a * len, 3) + " - " + juce::String (b * len, 3) + " s";
        }
        if (t != regionText) { regionText = t; repaint (0, 100, getWidth(), 30); }
        editor.repaint();
    }
}

void PadControl::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), "PAD CONTROL");
    const int pad = main.getSelectedPad();
    if (! layerView)
    {
        auto r = juce::Rectangle<float> (16.0f, 112.0f, (float) getWidth() - 32.0f, 14.0f);
        g.setColour (text);
        drawSpaced (g, "SAMPLE EDIT", r, sans (10.0f), 0.2f, juce::Justification::centredLeft);
        g.setColour (lime);
        g.setFont (mono (9.5f));
        g.drawText (regionText, r, juce::Justification::centredRight);
        return;
    }

    const bool b = main.isEditingLayerB();
    g.setColour (faint);
    g.setFont (mono (10.0f));
    g.drawText (juce::String ("EDITING LAYER ") + (b ? "B" : "A"), 16, 34, 200, 14, juce::Justification::centredLeft);

    auto drawSlot = [&] (juce::Rectangle<float> r, const juce::String& tag, const SamplePtr& s, bool on)
    {
        g.setColour (on ? bg.interpolatedWith (lime, 0.07f) : panel2);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (on ? lime : juce::Colour (0xff2c303a));
        g.drawRoundedRectangle (r.reduced (1.0f), 9.0f, 2.0f);
        auto in = r.reduced (10.0f, 0.0f);
        auto badge = in.removeFromLeft (24.0f).withSizeKeepingCentre (24.0f, 24.0f);
        g.setColour (on ? lime : knob);
        g.fillRoundedRectangle (badge, 6.0f);
        g.setColour (on ? bg : text);
        g.setFont (sans (12.0f));
        g.drawText (tag, badge, juce::Justification::centred);
        in.removeFromLeft (10.0f);
        if (s != nullptr)
            drawMiniWave (g, *s, in.removeFromRight (70.0f).withSizeKeepingCentre (70.0f, 18.0f), lime.withAlpha (on ? 0.9f : 0.5f));
        g.setColour (on ? lime : text);
        g.setFont (sans (12.0f));
        g.drawText (s != nullptr ? s->name : juce::String ("Pick a sound in the library"), in, juce::Justification::centredLeft);
    };
    drawSlot (slotA.getBounds().toFloat(), "A", main.proc.getPadSample (pad), ! b);
    drawSlot (slotB.getBounds().toFloat(), "B", main.proc.getLayerSample (pad), b);

    auto br = blend.getBounds().toFloat();
    g.setColour (dim);
    g.setFont (sans (9.0f));
    g.drawText ("A", br.getX(), br.getY() - 16.0f, 20.0f, 12.0f, juce::Justification::centredLeft);
    g.drawText ("B", br.getRight() - 20.0f, br.getY() - 16.0f, 20.0f, 12.0f, juce::Justification::centredRight);
    g.setColour (text);
    drawSpaced (g, "BLEND", { br.getX(), br.getY() - 16.0f, br.getWidth(), 12.0f }, sans (10.0f), 0.2f, juce::Justification::centred);
    const float v = (float) blend.getValue();
    const int pa = juce::roundToInt (juce::jmin (1.0f, 2.0f * (1.0f - v)) * 100.0f), pb = juce::roundToInt (juce::jmin (1.0f, 2.0f * v) * 100.0f);
    g.setColour (lime);
    g.setFont (mono (10.0f));
    g.drawText (juce::String (pa) + "%", br.getX(), br.getBottom() + 2.0f, 60.0f, 14.0f, juce::Justification::centredLeft);
    g.drawText (juce::String (pb) + "%", br.getRight() - 60.0f, br.getBottom() + 2.0f, 60.0f, 14.0f, juce::Justification::centredRight);
}

void PadControl::resized()
{
    const int w = getWidth() - 32;
    auto row = [&] (int y, int h, std::initializer_list<juce::Component*> items)
    {
        const int cw = w / (int) items.size();
        int x = 16;
        for (auto* c : items) { c->setBounds (x, y, cw, h); x += cw; }
    };
    layer.setBounds (16, getHeight() - 16 - 40, w, 40);

    if (! layerView)
    {
        row (36, 72, { &attack, &decay, &tune, &volume });
        editor.setBounds (16, 132, w, 74);
        row (214, 56, { &fadeIn, &fadeOut, &offset, &stretch });
        int x = 16;
        const int bw = (w - 15) / 4;
        for (auto* b : std::initializer_list<juce::Component*> { &reverse, &normalize, &snap, &reset })
        {
            b->setBounds (x, 276, bw, 30);
            x += bw + 5;
        }
    }
    else
    {
        slotA.setBounds (16, 54, w, 42);
        slotB.setBounds (16, 102, w, 42);
        row (156, 72, { &attack, &decay, &tune, &volume });
        blend.setBounds (24, 262, w - 16, 20);
    }
}

// ===========================================================================
PadGrid::PadGrid (MainView& m) : main (m)
{
    for (int p = 0; p < numPads; ++p)
        lastHits[(size_t) p] = main.proc.getHitCount (p);
}

juce::Rectangle<float> PadGrid::cell (int pad) const
{
    auto r = getLocalBounds().toFloat().reduced (24.0f, 16.0f);
    const float cw = r.getWidth() / 4.0f, ch = r.getHeight() / 2.0f;
    return { r.getX() + (float) (pad % 4) * cw, r.getY() + (float) (pad / 4) * ch, cw, ch };
}

void PadGrid::tick()
{
    bool any = false;
    for (int p = 0; p < numPads; ++p)
    {
        const auto h = main.proc.getHitCount (p);
        if (h != lastHits[(size_t) p]) { lastHits[(size_t) p] = h; flash[(size_t) p] = 1.0f; }
        if (flash[(size_t) p] > 0.01f) { flash[(size_t) p] *= 0.82f; any = true; }
        else flash[(size_t) p] = 0.0f;
    }
    if (any) repaint();
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int p = 0; p < numPads; ++p)
        if (cell (p).contains (e.position))
        {
            main.selectPad (p, true);
            return;
        }
}

void PadGrid::drawShape (juce::Graphics& g, int pad, juce::Rectangle<float> area, bool sel, float fl)
{
    const auto c = area.getCentre();
    const juce::Colour fill = sel ? lime : knob.interpolatedWith (lime, fl * 0.6f);
    const juce::Colour sh = sel ? limeDark : shadow;
    const juce::Colour inner = (sel ? bg : juce::Colour (0xff1a1d24));
    const float glow = sel ? 0.35f + 0.4f * fl : 0.5f * fl;

    auto glowAround = [&] (const juce::Path& p)
    {
        if (glow <= 0.01f) return;
        juce::DropShadow ds (lime.withAlpha (glow), 26, {});
        ds.drawForPath (g, p);
    };
    auto body = [&] (const juce::Path& p)
    {
        glowAround (p);
        g.setColour (sh);
        g.fillPath (p, juce::AffineTransform::translation (0.0f, 6.0f));
        g.setColour (fill);
        g.fillPath (p);
    };

    juce::Path p;
    switch (pad)
    {
        case 0: // KICK: big drum with a beater mark
        {
            p.addEllipse (juce::Rectangle<float> (120, 120).withCentre (c));
            body (p);
            g.setColour (inner.withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (44, 44).withCentre (c), 3.0f);
            break;
        }
        case 1: // SNARE: drum head with snare wires
        {
            p.addEllipse (juce::Rectangle<float> (100, 100).withCentre (c));
            body (p);
            g.setColour (inner.withAlpha (0.27f));
            g.drawEllipse (juce::Rectangle<float> (84, 84).withCentre (c), 8.0f);
            g.setColour (inner.withAlpha (0.4f));
            for (int i = -1; i <= 1; ++i)
                g.fillRect (juce::Rectangle<float> (66, 2).withCentre (c.translated (0.0f, (float) i * 10.0f)));
            break;
        }
        case 2: // RIM: a ring
        {
            juce::Path ring;
            ring.addEllipse (juce::Rectangle<float> (88, 88).withCentre (c));
            ring.addEllipse (juce::Rectangle<float> (56, 56).withCentre (c));
            ring.setUsingNonZeroWinding (false);
            body (ring);
            break;
        }
        case 3: // PERCS: hexagon
        {
            const float w = 100, h = 88;
            p.startNewSubPath (c.x - w * 0.25f, c.y - h * 0.5f);
            p.lineTo (c.x + w * 0.25f, c.y - h * 0.5f);
            p.lineTo (c.x + w * 0.5f, c.y);
            p.lineTo (c.x + w * 0.25f, c.y + h * 0.5f);
            p.lineTo (c.x - w * 0.25f, c.y + h * 0.5f);
            p.lineTo (c.x - w * 0.5f, c.y);
            p.closeSubPath();
            body (p);
            break;
        }
        case 4: // VOX: microphone capsule with a grille
        {
            auto r = juce::Rectangle<float> (64, 104).withCentre (c);
            p.addRoundedRectangle (r, 32.0f);
            body (p);
            g.saveState();
            g.reduceClipRegion (p);
            g.setColour (inner.withAlpha (0.27f));
            for (float y = r.getY() + 9.0f; y < r.getBottom(); y += 11.0f)
                g.fillRect (r.getX(), y, r.getWidth(), 2.0f);
            g.restoreState();
            break;
        }
        case 5: // FX: diamond
        {
            p.addRoundedRectangle (juce::Rectangle<float> (74, 74).withCentre ({ 0, 0 }), 14.0f);
            p.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (c));
            body (p);
            g.setColour (inner.withAlpha (0.33f));
            g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre (c));
            break;
        }
        case 6: // CLAP: two hands
        {
            juce::Path a, b;
            a.addEllipse (juce::Rectangle<float> (70, 70).withCentre (c.translated (-21.0f, -11.0f)));
            b.addEllipse (juce::Rectangle<float> (70, 70).withCentre (c.translated (21.0f, 11.0f)));
            body (a);
            g.setColour (inner);
            g.fillPath (b, juce::AffineTransform::translation (-4.0f, 0.0f));
            body (b);
            break;
        }
        default: // TOM: drum seen from the side
        {
            auto shell = juce::Rectangle<float> (104, 56).withCentre (c.translated (0.0f, 14.0f));
            juce::Path sp;
            sp.addRoundedRectangle (shell.getX(), shell.getY(), shell.getWidth(), shell.getHeight(), 18.0f, 18.0f, false, false, true, true);
            glowAround (sp);
            g.setColour (sh);
            g.fillPath (sp, juce::AffineTransform::translation (0.0f, 6.0f));
            g.setColour (sel ? limeDark : juce::Colour (0xff1e2129).interpolatedWith (lime, fl * 0.4f));
            g.fillPath (sp);
            g.saveState();
            g.reduceClipRegion (sp);
            g.setColour (inner.withAlpha (0.2f));
            for (float x = shell.getX() - 60.0f; x < shell.getRight(); x += 18.0f)
                g.drawLine (x, shell.getBottom(), x + 34.0f, shell.getY(), 2.0f);
            g.restoreState();
            auto head = juce::Rectangle<float> (104, 30).withCentre ({ c.x, shell.getY() });
            g.setColour (fill);
            g.fillEllipse (head);
            g.setColour (inner.withAlpha (0.27f));
            g.drawEllipse (head.reduced (3.0f), 4.0f);
            break;
        }
    }
}

void PadGrid::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat());
    for (int p = 0; p < numPads; ++p)
    {
        auto r = cell (p);
        const bool sel = p == main.getSelectedPad();
        auto labels = r.removeFromBottom (40.0f);
        drawShape (g, p, r, sel, flash[(size_t) p]);

        if (main.isLayerOn (p) && main.proc.getLayerSample (p) != nullptr)
        {
            auto badge = juce::Rectangle<float> (40.0f, 20.0f).withCentre ({ r.getCentreX() + 58.0f, r.getCentreY() - 52.0f });
            g.setColour (bg);
            g.fillRoundedRectangle (badge, 10.0f);
            g.setColour (lime);
            g.drawRoundedRectangle (badge.reduced (1.0f), 10.0f, 2.0f);
            g.setFont (sans (10.0f));
            g.drawText ("A+B", badge, juce::Justification::centred);
        }

        g.setColour (sel ? lime : text);
        drawSpaced (g, padInfo[(size_t) p].name, labels.removeFromTop (20.0f), sans (13.0f), 0.14f, juce::Justification::centred);
        g.setColour (faint);
        g.setFont (mono (9.5f));
        g.drawText (juce::String (padInfo[(size_t) p].noteName) + " - " + shortName (main.proc.getPadSample (p)),
                    labels.removeFromTop (16.0f), juce::Justification::centred);
    }
}

// ===========================================================================
HomePage::HomePage (MainView& m) : categories (m), library (m), control (m), pads (m)
{
    for (auto* c : std::initializer_list<juce::Component*> { &categories, &library, &control, &pads })
        addAndMakeVisible (c);
}

void HomePage::resized()
{
    categories.setBounds (0, 0, 178, 390);
    library.setBounds (190, 0, getWidth() - 178 - 12 - 320 - 12, 390);
    control.setBounds (getWidth() - 320, 0, 320, 390);
    pads.setBounds (0, 404, getWidth(), getHeight() - 404);
}

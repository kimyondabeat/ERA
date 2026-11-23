#include "PluginEditor.h"

using namespace Theme;

// ===========================================================================
EraLookAndFeel::EraLookAndFeel() { refreshColours(); }

void EraLookAndFeel::refreshColours()
{
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, lime);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lime);
    setColour (juce::PopupMenu::highlightedTextColourId, onAccent);
    setColour (juce::TextEditor::textColourId, lcd.getPerceivedBrightness() > 0.5f ? juce::Colour (0xff0a0a0b) : juce::Colour (0xffe9ebf0)); // the field is a screen
    setColour (juce::TextEditor::highlightColourId, lime.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, lime);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, lime.withAlpha (0.5f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, onAccent);
}

void EraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h);
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.43f;
    g.setColour (shadow);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c.translated (0.0f, size * 0.04f)));
    g.setColour (s.isMouseOverOrDragging() ? knob.brighter (0.08f) : knob);
    g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));

    const float a = start + pos * (end - start);
    const auto p1 = c.getPointOnCircumference (size * 0.25f, a);
    const auto p2 = c.getPointOnCircumference (size * 0.36f, a);
    g.setColour (lime);
    g.drawLine ({ p1, p2 }, juce::jmax (2.0f, size / 16.0f));
}

void EraLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool master = s.getProperties()["master"];
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (lcd);
        g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
        const auto cap = juce::Rectangle<float> (26.0f, 20.0f).withCentre ({ cx, pos });
        g.setColour (shadow);
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);
        g.setColour (master ? lime : off);
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (master ? lime.brighter (0.5f) : off.brighter (0.3f));
        g.fillRect (cap.withHeight (2.0f));
        return;
    }

    // horizontal: BLEND
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (lcd);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (lime);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    const auto cap = juce::Rectangle<float> (30.0f, 16.0f).withCentre ({ pos, cy });
    g.setColour (limeDark);
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);
    g.setColour (lime);
    g.fillRoundedRectangle (cap, 4.0f);
}

void EraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    if (b.getProperties()["invisible"]) return;
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const bool flat = b.getProperties()["flat"];
    const bool chip = b.getProperties()["chip"];
    const float radius = chip ? r.getHeight() * 0.5f : 8.0f;

    if (flat && ! on)
    {
        if (over) { g.setColour (knob.withAlpha (0.6f)); g.fillRoundedRectangle (r, radius); }
        return;
    }
    if (chip && ! on)
    {
        g.setColour (b.getProperties()["lcd"] ? lime.withAlpha (over ? 0.9f : 0.45f) : (over ? dim : chipEdge));
        g.drawRoundedRectangle (r.reduced (1.0f), radius, 2.0f);
        return;
    }

    const bool pressed = down && ! flat;
    if (! chip)
    {
        g.setColour (on ? limeDark : shadow);
        g.fillRoundedRectangle (r.withTrimmedTop (3.0f), radius);
        r.removeFromBottom (3.0f);
        if (pressed) r.translate (0.0f, 2.0f);
    }
    auto c = on ? lime : knob;
    if (over && b.isEnabled()) c = c.brighter (0.08f);
    if (! b.isEnabled()) c = c.withAlpha (0.5f);
    g.setColour (c);
    g.fillRoundedRectangle (r, radius);
}

void EraLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    if (b.getProperties()["invisible"]) return;
    auto r = b.getLocalBounds().toFloat();
    if (! b.getProperties()["chip"] && ! b.getProperties()["flat"])
    {
        r.removeFromBottom (3.0f);
        if (down) r.translate (0.0f, 2.0f);
    }
    const bool on = b.getToggleState();
    auto c = on ? onAccent : (b.getProperties()["lcd"] ? lime : (b.getProperties()["flat"] ? dim : text));
    if (! b.isEnabled()) c = c.withAlpha (0.4f);
    g.setColour (c);
    const float size = b.getProperties().contains ("fontSize") ? (float) b.getProperties()["fontSize"] : 11.0f;
    drawSpaced (g, b.getButtonText(), r, sans (size), 0.14f, juce::Justification::centred);
}

void EraLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    auto r = juce::Rectangle<float> ((float) w, (float) h);
    g.setColour (lcd);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (lcdEdge);
    g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 2.0f);
}

// ===========================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& c, int sz)
    : state (s), caption (c), size (sz)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setVelocityBasedMode (false);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::bind (const juce::String& paramID)
{
    attachment.reset();
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    repaint();
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withHeight (size).withSizeKeepingCentre (size, size).withY (0));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedTop ((float) size + 3.0f);
    juce::String t = caption;
    bool showValue = slider.isMouseButtonDown();
    if (showValue && attachment != nullptr)
        t = slider.getTextFromValue (slider.getValue());
    g.setColour (showValue ? lime : dim);
    drawSpaced (g, t, r, showValue ? mono (10.0f) : sans (9.0f), showValue ? 0.0f : 0.12f, juce::Justification::centredTop);
}

// ===========================================================================
ChoiceRow::ChoiceRow (juce::AudioProcessorValueTreeState& s, juce::StringArray labels) : state (s)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->getProperties().set ("fontSize", 10.0f);
        b->onClick = [this, i]
        {
            if (auto* p = state.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) i));
                p->endChangeGesture();
            }
        };
        addAndMakeVisible (b);
    }
}

ChoiceRow::~ChoiceRow()
{
    if (id.isNotEmpty())
        state.removeParameterListener (id, this);
}

void ChoiceRow::bind (const juce::String& paramID)
{
    if (id.isNotEmpty())
        state.removeParameterListener (id, this);
    id = paramID;
    state.addParameterListener (id, this);
    refresh();
}

void ChoiceRow::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ChoiceRow> (this)] { if (safe) safe->refresh(); });
}

void ChoiceRow::refresh()
{
    const int v = juce::roundToInt (state.getRawParameterValue (id)->load());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == v, juce::dontSendNotification);
}

void ChoiceRow::resized()
{
    auto r = getLocalBounds();
    const int w = (r.getWidth() - 4 * (buttons.size() - 1)) / juce::jmax (1, buttons.size());
    for (auto* b : buttons)
    {
        b->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (4);
    }
}

// ===========================================================================
MainView::MainView (EraProcessor& p, std::function<void (float)> scaleFn)
    : proc (p), setScale (std::move (scaleFn)),
      master (p.apvts, "MASTER", 54),
      home (*this), mix (*this), fx (*this)
{
    juce::PropertiesFile::Options o;
    o.applicationName = "ERA";
    o.folderName = "ERA";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (o);
    applyTheme (settings->getIntValue ("theme", 0));
    for (auto& f : juce::StringArray::fromLines (settings->getValue ("favourites")))
        if (f.isNotEmpty()) favourites.insert (f);

    for (int i = 0; i < 3; ++i)
    {
        tabs[i].setRadioGroupId (1);
        tabs[i].setClickingTogglesState (false);
        tabs[i].getProperties().set ("flat", true);
        tabs[i].getProperties().set ("fontSize", 13.0f);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    for (auto* b : { &prevSound, &nextSound })
    {
        b->getProperties().set ("chip", true);
        b->getProperties().set ("lcd", true);
        b->getProperties().set ("fontSize", 14.0f);
        addAndMakeVisible (b);
    }
    prevSound.setTooltip ("Previous sound");
    nextSound.setTooltip ("Next sound");
    prevSound.onClick = [this] { stepSound (-1); };
    nextSound.onClick = [this] { stepSound (1); };

    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);
    master.bind ("master");
    addAndMakeVisible (master);

    addChildComponent (home);
    addChildComponent (mix);
    addChildComponent (fx);

    setSize (baseW, baseH);
    selectPad (1, false);
    showPage (0);
    startTimerHz (30);
}

MainView::~MainView()
{
    stopTimer();
    settings->saveIfNeeded();
}

void MainView::stepSound (int direction)
{
    if (editLayerB && isLayerOn (selectedPad))
    {
        auto cur = proc.getLayerSample (selectedPad);
        proc.assignLayer (selectedPad, Library::get().neighbour (cur, cur != nullptr ? cur->category : selectedPad, direction));
    }
    else
    {
        proc.stepSound (selectedPad, direction);
    }
    home.library.refreshList();
    home.control.refresh();
    home.pads.repaint();
    repaint (lcdArea());
}

void MainView::repaintEverything()
{
    std::function<void (juce::Component&)> all = [&] (juce::Component& c)
    {
        c.repaint();
        for (auto* child : c.getChildren()) all (*child);
    };
    all (*this);
}

void MainView::toggleFavourite (const juce::String& id)
{
    if (! favourites.erase (id))
        favourites.insert (id);
    juce::StringArray list;
    for (auto& f : favourites) list.add (f);
    settings->setValue ("favourites", list.joinIntoString ("\n"));
    settings->saveIfNeeded();
    home.categories.repaint();
}

void MainView::selectPad (int pad, bool trigger)
{
    selectedPad = juce::jlimit (0, numPads - 1, pad);
    if (trigger)
        proc.triggerPad (selectedPad, 0.85f);
    if (! isLayerOn (selectedPad))
        editLayerB = false;
    if (libraryCategory != CategoryList::favourites)
        libraryCategory = selectedPad;
    home.categories.repaint();
    home.library.refreshList();
    home.control.refresh();
    home.pads.repaint();
    setFxTarget (selectedPad);
    repaint (lcdArea());
}

void MainView::setEditingLayerB (bool b)
{
    editLayerB = b;
    home.control.refresh();
    home.library.updateButtons();
    repaint (lcdArea());
}

void MainView::setFxTarget (int t)
{
    fxTarget = juce::jlimit (0, numPads, t);
    fx.refresh();
}

void MainView::setLibraryCategory (int c)
{
    libraryCategory = c;
    home.categories.repaint();
    home.library.refreshList();
}

void MainView::showPage (int p)
{
    page = p;
    for (int i = 0; i < 3; ++i)
        tabs[i].setToggleState (i == p, juce::dontSendNotification);
    home.setVisible (p == 0);
    mix.setVisible (p == 1);
    fx.setVisible (p == 2);
    if (p == 2) fx.refresh();
    repaint();
}

void MainView::showSettings()
{
    juce::PopupMenu m, sizes;
    const float current = (float) settings->getDoubleValue ("uiScale", 0.8);
    for (int pct : { 60, 70, 80, 90, 100, 110, 120 })
        sizes.addItem (pct, juce::String (pct) + " %", true, std::abs (current * 100.0f - (float) pct) < 1.0f);
    juce::PopupMenu colours;
    const int currentTheme = settings->getIntValue ("theme", 0);
    for (int i = 0; i < numThemes; ++i)
        colours.addItem (1000 + i, themes[i].name, true, i == currentTheme);
    m.addSectionHeader ("ERA v1.0  -  by kimyondabeat");
    m.addSubMenu ("Interface size", sizes);
    m.addSubMenu ("Interface colour", colours);
    m.setLookAndFeel (&getLookAndFeel());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton),
                     [this] (int r)
                     {
                         if (r <= 0) return;
                         if (r >= 1000)
                         {
                             settings->setValue ("theme", r - 1000);
                             settings->saveIfNeeded();
                             applyTheme (r - 1000);
                             if (auto* l = dynamic_cast<EraLookAndFeel*> (&getLookAndFeel())) l->refreshColours();
                             home.library.refreshColours();
                             home.library.refreshList();
                             repaintEverything();
                             if (auto* ed = getParentComponent()) ed->repaint();
                             return;
                         }
                         settings->setValue ("uiScale", r / 100.0);
                         settings->saveIfNeeded();
                         setScale (r / 100.0f);
                     });
}

void MainView::timerCallback()
{
    uint32_t sum = 0;
    for (int p = 0; p < numPads; ++p) sum += proc.getHitCount (p);
    if (sum != lastHitSum) { lastHitSum = sum; trigLed = 1.0f; }
    if (proc.getMidiCount() != lastMidi) { lastMidi = proc.getMidiCount(); midiLed = 1.0f; }
    trigLed *= 0.8f; midiLed *= 0.8f;
    repaint (708, 64, 260, 40);

    if (home.isVisible()) { home.pads.tick(); home.control.tick(); }
    if (mix.isVisible()) mix.tick();
    if (fx.isVisible()) fx.tick();
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (rail);
    g.fillRect (0, 0, 16, getHeight());
    g.fillRect (getWidth() - 16, 0, 16, getHeight());

    // ---- logo
    g.setColour (text);
    drawSpaced (g, "ERA", { 48, 18, 140, 50 }, sans (46.0f), 0.14f, juce::Justification::centredLeft);
    const float dotX = 48.0f + sans (46.0f).withExtraKerningFactor (0.14f).getStringWidthFloat ("ERA") + 4.0f;
    g.setColour (lime.withAlpha (0.3f));
    g.fillEllipse (dotX - 4, 34, 17, 17);
    g.setColour (lime);
    g.fillEllipse (dotX, 38, 9, 9);
    g.setColour (dim);
    drawSpaced (g, "RABODAY DRUMS", { 48, 68, 200, 14 }, sans (10.0f), 0.24f, juce::Justification::centredLeft);
    g.setColour (lime);
    drawSpaced (g, "by kimyondabeat", { 48, 84, 200, 18 }, sans (14.0f), 0.04f, juce::Justification::centredLeft);

    // ---- LCD
    auto l = lcdArea().toFloat();
    g.setColour (lcd);
    g.fillRoundedRectangle (l, 12.0f);
    g.setColour (lcdEdge);
    g.drawRoundedRectangle (l.reduced (1.5f), 12.0f, 3.0f);
    auto in = l.reduced (20.0f, 10.0f);
    const int p = selectedPad;
    const bool layered = isLayerOn (p) && proc.getLayerSample (p) != nullptr;
    g.setColour (lime.withAlpha (0.7f));
    g.setFont (mono (11.0f));
    g.drawText ("PAD " + juce::String (p + 1).paddedLeft ('0', 2) + " / 08 - MIDI " + padInfo[(size_t) p].noteName
                    + (layered ? " - LAYER" : ""),
                in.removeFromTop (16.0f), juce::Justification::topLeft);
    auto bottom = in.removeFromBottom (40.0f);
    g.setColour (lime.withAlpha (0.25f));
    g.setFont (mono (32.0f).withExtraKerningFactor (0.1f));
    const juce::String big = juce::String (padInfo[(size_t) p].name) + (layered ? "+B" : "");
    g.drawText (big, bottom.translated (0.0f, 1.0f).withTrimmedLeft (1.0f), juce::Justification::bottomLeft);
    g.setColour (lime);
    g.drawText (big, bottom, juce::Justification::bottomLeft);

    auto right = l.reduced (20.0f, 10.0f).withTrimmedLeft (190.0f);
    g.setColour (lime.withAlpha (0.7f));
    g.setFont (mono (11.0f));
    const bool showB = editLayerB && layered;
    g.drawText (showB ? "LAYER B" : (layered ? "LAYER A" : "SOUND"), right.removeFromTop (16.0f), juce::Justification::topRight);
    auto s = showB ? proc.getLayerSample (p) : proc.getPadSample (p);
    g.setColour (lime);
    g.setFont (mono (15.0f));
    g.drawFittedText (s != nullptr ? s->name : juce::String ("(empty)"),
                      right.removeFromBottom (28.0f).withTrimmedRight (40.0f).withTrimmedLeft (36.0f).toNearestInt(),
                      juce::Justification::centred, 1, 0.8f);

    // ---- tab frame + LEDs
    auto nav = juce::Rectangle<float> (708, 22, 260, 46);
    g.setColour (panel);
    g.fillRoundedRectangle (nav, 11.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (nav.reduced (1.0f), 11.0f, 2.0f);
    const char* names[] = { "TRIG", "MIDI", "PWR" };
    const float levels[] = { trigLed, midiLed, 1.0f };
    for (int i = 0; i < 3; ++i)
    {
        const float x = 820.0f + (float) i * 52.0f;
        led (g, { x, 80.0f }, levels[i] > 0.15f);
        g.setColour (dim);
        drawSpaced (g, names[i], { x - 26, 88, 52, 12 }, sans (9.0f), 0.12f, juce::Justification::centred);
    }
}

void MainView::resized()
{
    for (int i = 0; i < 3; ++i)
        tabs[i].setBounds (712 + i * 84, 26, 82, 38);
    auto l = lcdArea();
    prevSound.setBounds (l.getRight() - 20 - 214 + 0, l.getBottom() - 10 - 26, 30, 26);
    nextSound.setBounds (l.getRight() - 20 - 30, l.getBottom() - 10 - 26, 30, 26);
    settingsButton.setBounds (988, 40, 92, 35);
    master.setBounds (1094, 22, 58, 76);

    const auto body = juce::Rectangle<int> (48, 120, 1104, baseH - 120 - 22);
    home.setBounds (body);
    mix.setBounds (body);
    fx.setBounds (body);
}

// ===========================================================================
EraEditor::EraEditor (EraProcessor& p)
    : AudioProcessorEditor (&p), view (p, [this] (float s) { setScale (s); })
{
    lnf.refreshColours(); // the view loaded the saved colour
    setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setScale ((float) view.settings->getDoubleValue ("uiScale", 0.8));
}

EraEditor::~EraEditor()
{
    setLookAndFeel (nullptr);
}

void EraEditor::setScale (float s)
{
    scale = juce::jlimit (0.5f, 2.0f, s);
    view.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (MainView::baseW * scale), juce::roundToInt (MainView::baseH * scale));
}

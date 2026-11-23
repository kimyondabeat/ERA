#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include "PluginProcessor.h"
#include "Theme.h"

class MainView;

// ---------------------------------------------------------------------------
class EraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EraLookAndFeel();
    void refreshColours();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getPopupMenuFont() override { return Theme::sans (14.0f, false); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Theme::sans (11.0f); }
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
};

// ---------------------------------------------------------------------------
// A retro knob with its caption; shows the value while you turn it. Can be re-bound to another parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& caption, int size);
    void bind (const juce::String& paramID);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::Slider slider;
    juce::String caption;
    int size;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A lime button tied to a bool parameter.
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& label) : juce::TextButton (label), state (s) { setClickingTogglesState (true); }
    void bind (const juce::String& paramID)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, *this);
    }
private:
    juce::AudioProcessorValueTreeState& state;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// A row of buttons tied to a choice parameter (1/4 1/8 ... or LP HP BP).
class ChoiceRow : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    ChoiceRow (juce::AudioProcessorValueTreeState&, juce::StringArray labels);
    ~ChoiceRow() override;
    void bind (const juce::String& paramID);
    void resized() override;
private:
    void parameterChanged (const juce::String&, float) override;
    void refresh();
    juce::AudioProcessorValueTreeState& state;
    juce::String id;
    juce::OwnedArray<juce::TextButton> buttons;
};

// ---------------------------------------------------------------------------
class CategoryList : public juce::Component
{
public:
    static constexpr int favourites = -1;
    explicit CategoryList (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> rowBounds (int row) const;
    MainView& main;
};

class LibraryPanel : public juce::Component, private juce::ListBoxModel
{
public:
    explicit LibraryPanel (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshList();
    void updateButtons();
    void refreshColours();
private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void assign();

    MainView& main;
    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton playButton { "PLAY" }, favButton { "FAVORITE" }, assignButton { "ASSIGN" };
    std::vector<SamplePtr> shown;
    SamplePtr current;
};

// Waveform with draggable START / END and the fades drawn on top.
class SampleEditor : public juce::Component
{
public:
    explicit SampleEditor (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter* param (const char* id) const;
    MainView& main;
    juce::RangedAudioParameter* dragging = nullptr;
};

class PadControl : public juce::Component
{
public:
    explicit PadControl (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();      // selected pad or layer view changed
    void tick();         // 30 Hz
private:
    MainView& main;
    Knob attack, decay, tune, volume;
    Knob fadeIn, fadeOut, offset, stretch;
    ParamToggle reverse, normalize, snap, layer;
    juce::TextButton reset { "RESET" };
    SampleEditor editor;
    juce::TextButton slotA, slotB;
    juce::Slider blend;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;
    bool layerView = false;
    juce::String regionText;
};

class PadGrid : public juce::Component
{
public:
    explicit PadGrid (MainView&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();
private:
    juce::Rectangle<float> cell (int pad) const;
    void drawShape (juce::Graphics&, int pad, juce::Rectangle<float> area, bool selected, float flash);
    MainView& main;
    std::array<float, numPads> flash {};
    std::array<uint32_t, numPads> lastHits {};
};

class HomePage : public juce::Component
{
public:
    explicit HomePage (MainView&);
    void resized() override;
    CategoryList categories;
    LibraryPanel library;
    PadControl control;
    PadGrid pads;
};

// ---------------------------------------------------------------------------
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramID);
    void resized() override;
    juce::Slider slider;
private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class MixPage : public juce::Component
{
public:
    explicit MixPage (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    juce::Rectangle<int> strip (int i) const;
    MainView& main;
    juce::OwnedArray<Knob> pans;
    juce::OwnedArray<Fader> faders;
    juce::OwnedArray<ParamToggle> mutes, solos;
    Knob width, limit;
    Fader masterFader;
    std::array<float, numPads + 1> levels {};
};

class FxModule : public juce::Component
{
public:
    FxModule (MainView&, const juce::String& title, const juce::String& onId, juce::StringArray knobIds, juce::StringArray captions);
    void paint (juce::Graphics&) override;
    void resized() override;
    void bind (int chain);
    juce::Component* extra = nullptr; // optional row above the knobs
    int extraHeight = 0;
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawExtra;
    juce::Rectangle<float> extraArea;
private:
    MainView& main;
    juce::String title, onId;
    juce::StringArray ids;
    ParamToggle onButton;
    juce::OwnedArray<Knob> knobs;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (MainView&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void tick();
private:
    MainView& main;
    juce::OwnedArray<juce::TextButton> targets;
    juce::TextButton resetButton { "RESET FX" };
    ChoiceRow sync, filterType;
    FxModule delay, filter, eq, reverb, comp, transient;
    float gr = 0.0f;
};

// ---------------------------------------------------------------------------
class MainView : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseW = 1200, baseH = 960;

    MainView (EraProcessor&, std::function<void (float)> setScale);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPad (int pad, bool trigger);
    int getSelectedPad() const { return selectedPad; }
    bool isEditingLayerB() const { return editLayerB; }
    void setEditingLayerB (bool b);
    bool isLayerOn (int pad) const { return proc.apvts.getRawParameterValue (EraProcessor::pid (pad, "layer"))->load() > 0.5f; }
    int getFxTarget() const { return fxTarget; }
    void setFxTarget (int t);
    int getLibraryCategory() const { return libraryCategory; }
    void setLibraryCategory (int c);
    void showPage (int page);
    void repaintEverything();

    bool isFavourite (const juce::String& id) const { return favourites.count (id) > 0; }
    void toggleFavourite (const juce::String& id);
    int countFavourites() const { return (int) favourites.size(); }
    juce::String searchText;

    EraProcessor& proc;
    std::unique_ptr<juce::PropertiesFile> settings;

private:
    void timerCallback() override;
    void showSettings();
    void stepSound (int direction);
    juce::Rectangle<int> lcdArea() const { return { 268, 22, 420, 80 }; }

    std::function<void (float)> setScale;
    int selectedPad = 1, fxTarget = 1, libraryCategory = 1, page = 0;
    bool editLayerB = false;
    std::set<juce::String> favourites;

    juce::TextButton tabs[3] { juce::TextButton ("HOME"), juce::TextButton ("MIX"), juce::TextButton ("FX") };
    juce::TextButton prevSound { "<" }, nextSound { ">" }, settingsButton { "SETTINGS" };
    Knob master;
    float trigLed = 0.0f, midiLed = 0.0f;
    uint32_t lastMidi = 0, lastHitSum = 0;

public:
    HomePage home;
    MixPage mix;
    FxPage fx;
};

// ---------------------------------------------------------------------------
class EraEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EraEditor (EraProcessor&);
    ~EraEditor() override;
    void resized() override {}
    void paint (juce::Graphics& g) override { g.fillAll (Theme::bg); }

private:
    void timerCallback() override {}
    void setScale (float);
    EraLookAndFeel lnf;
    MainView view;
    float scale = 0.8f;
};

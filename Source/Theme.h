#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace Theme
{
    const juce::Colour bg       { 0xff14161c };
    const juce::Colour rail     { 0xff0f1116 };
    const juce::Colour panel    { 0xff181b22 };
    const juce::Colour panelEdge{ 0xff232731 };
    const juce::Colour panel2   { 0xff1f232b };
    const juce::Colour line     { 0xff2a2e38 };
    const juce::Colour knob     { 0xff252933 };
    const juce::Colour shadow   { 0xff0d0f13 };
    const juce::Colour lcd      { 0xff0b0c0f };
    const juce::Colour lcdEdge  { 0xff242833 };
    const juce::Colour text     { 0xffe9ebf0 };
    const juce::Colour dim      { 0xff9aa0ad };
    const juce::Colour faint    { 0xff7c8290 };
    const juce::Colour off      { 0xff3a3f4b };
    const juce::Colour lime     { 0xffc8f04a };
    const juce::Colour limeDark { 0xff8fae2c };
    const juce::Colour amber    { 0xffffb347 };
    const juce::Colour red      { 0xffff5a4a };

    inline juce::Typeface::Ptr typeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    inline juce::Font sans (float h, bool bold = true)
    {
        static auto b = typeface (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize);
        static auto m = typeface (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
        return juce::Font (bold ? b : m).withHeight (h);
    }

    inline juce::Font mono (float h)
    {
        static auto t = typeface (BinaryData::JetBrainsMonoBold_ttf, BinaryData::JetBrainsMonoBold_ttfSize);
        return juce::Font (t).withHeight (h);
    }

    // Letter-spaced caps, like the design's labels.
    inline void drawSpaced (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f,
                            float spacing, juce::Justification j)
    {
        g.setFont (f.withExtraKerningFactor (spacing));
        g.drawText (s, r, j, false);
    }

    inline void panelBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (1.0f), 14.0f, 2.0f);
        if (title.isNotEmpty())
        {
            g.setColour (dim);
            drawSpaced (g, title, r.reduced (16.0f, 12.0f).removeFromTop (16.0f), sans (11.0f), 0.26f, juce::Justification::centredLeft);
        }
    }

    inline void led (juce::Graphics& g, juce::Point<float> c, bool on, float r = 5.5f)
    {
        if (on)
        {
            g.setColour (lime.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (r * 4, r * 4).withCentre (c));
        }
        g.setColour (on ? lime : off);
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
    }
}

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace Theme
{
    // Every colour of the interface comes from the selected theme (Settings > Interface colour).
    // The accent is still called "lime" after the default theme.
    inline juce::Colour bg, rail, panel, panelEdge, panel2, line, knob, shadow, lcd, lcdEdge,
                        text, dim, faint, off, lime, limeDark, onAccent, chipEdge, inner;
    const juce::Colour amber { 0xffffb347 };
    const juce::Colour red   { 0xffff5a4a };

    struct Palette
    {
        const char* name;
        juce::uint32 bg, rail, panel, panelEdge, panel2, line, knob, shadow, lcd, lcdEdge,
                     text, dim, faint, off, accent, accentDark, onAccent, chipEdge, inner;
    };
    constexpr int numThemes = 4;
    inline const Palette themes[numThemes] = {
        { "Ice Blue - Black & White",
          0xff0a0a0b, 0xff000000, 0xff141416, 0xff242428, 0xff1c1c1f, 0xff2b2b30, 0xff232327, 0xff000000, 0xff050506, 0xff2b2b30,
          0xffffffff, 0xffb8b8bf, 0xff8a8a92, 0xff3c3c42, 0xff7fdcff, 0xff3b9cc4, 0xff04141b, 0xff34343a, 0xff121214 },
        { "Lime",
          0xff14161c, 0xff0f1116, 0xff181b22, 0xff232731, 0xff1f232b, 0xff2a2e38, 0xff252933, 0xff0d0f13, 0xff0b0c0f, 0xff242833,
          0xffe9ebf0, 0xff9aa0ad, 0xff7c8290, 0xff3a3f4b, 0xffc8f04a, 0xff8fae2c, 0xff14161c, 0xff2c303a, 0xff1a1d24 },
        { "Yellow - Black & Grey",
          0xff111111, 0xff0a0a0a, 0xff1f1f1f, 0xff2f2f2f, 0xff292929, 0xff393939, 0xff333333, 0xff080808, 0xff0b0b0b, 0xff353535,
          0xfff2f2f2, 0xffababab, 0xff8a8a8a, 0xff4a4a4a, 0xffffd23f, 0xffb8921a, 0xff1a1400, 0xff444444, 0xff242424 },
        { "Pink - Grey & White",
          0xffd4d4d8, 0xffbdbdc2, 0xfff7f7f8, 0xffcacace, 0xffececef, 0xffd2d2d7, 0xffd9d9de, 0xffb3b3ba, 0xff2a2a30, 0xffa6a6ad,
          0xff1d1d22, 0xff52525a, 0xff66666e, 0xffc2c2c8, 0xffff4f9a, 0xffc22f6f, 0xffffffff, 0xffbdbdc3, 0xffc8c8ce },
    };

    inline void applyTheme (int index)
    {
        const auto& p = themes[juce::jlimit (0, numThemes - 1, index)];
        bg = juce::Colour (p.bg); rail = juce::Colour (p.rail); panel = juce::Colour (p.panel); panelEdge = juce::Colour (p.panelEdge);
        panel2 = juce::Colour (p.panel2); line = juce::Colour (p.line); knob = juce::Colour (p.knob); shadow = juce::Colour (p.shadow);
        lcd = juce::Colour (p.lcd); lcdEdge = juce::Colour (p.lcdEdge); text = juce::Colour (p.text); dim = juce::Colour (p.dim);
        faint = juce::Colour (p.faint); off = juce::Colour (p.off); lime = juce::Colour (p.accent); limeDark = juce::Colour (p.accentDark);
        onAccent = juce::Colour (p.onAccent); chipEdge = juce::Colour (p.chipEdge); inner = juce::Colour (p.inner);
    }

    // so the colours are valid before any settings are read
    inline const bool themeReady = [] { applyTheme (0); return true; }();

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

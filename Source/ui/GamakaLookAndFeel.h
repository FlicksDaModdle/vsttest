#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace palette
{
const juce::Colour background { 0xff1b1216 };
const juce::Colour panel { 0xff281b21 };
const juce::Colour panelEdge { 0xff3d2a32 };
const juce::Colour saffron { 0xffe9a23b };
const juce::Colour kumkum { 0xffc8413a };
const juce::Colour peacock { 0xff2fa3a0 };
const juce::Colour text { 0xfff3e6cc };
const juce::Colour dimText { 0xffa8968a };
} // namespace palette

class GamakaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GamakaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted,
                               bool down) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.0f); }
};

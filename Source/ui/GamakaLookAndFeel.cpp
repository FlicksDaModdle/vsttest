#include "GamakaLookAndFeel.h"

GamakaLookAndFeel::GamakaLookAndFeel()
{
    using namespace palette;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, saffron);
    setColour (juce::ComboBox::backgroundColourId, panel.brighter (0.08f));
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, saffron);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, saffron.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panel.brighter (0.1f));
    setColour (juce::TextButton::buttonOnColourId, kumkum);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, saffron);
    setColour (juce::ToggleButton::tickDisabledColourId, dimText);
    setColour (juce::TabbedComponent::backgroundColourId, background);
    setColour (juce::TabbedComponent::outlineColourId, panelEdge);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, text);
}

void GamakaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                          float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float stroke = juce::jmax (2.5f, r * 0.16f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0.0f, start, end, true);
    g.setColour (palette::panelEdge.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r - stroke * 0.5f, r - stroke * 0.5f, 0.0f, start, angle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (palette::panel.brighter (0.25f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.55f, r * 1.1f, r * 1.1f);
    const auto tip = c.getPointOnCircumference (r * 0.5f, angle);
    g.setColour (palette::text);
    g.drawLine ({ c.getPointOnCircumference (r * 0.15f, angle), tip }, 2.0f);
}

void GamakaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour,
                                              bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : colour;
    if (down)
        fill = fill.brighter (0.2f);
    else if (highlighted)
        fill = fill.brighter (0.08f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? palette::saffron : palette::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void GamakaLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool isMouseOver, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f, 3.0f);
    const bool front = b.isFrontTab();
    g.setColour (front ? palette::panel.brighter (0.12f) : (isMouseOver ? palette::panel.brighter (0.05f) : palette::panel));
    g.fillRoundedRectangle (r, 5.0f);
    if (front)
    {
        g.setColour (palette::saffron);
        g.fillRect (r.removeFromBottom (2.5f));
    }
    g.setColour (front ? palette::text : palette::dimText);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

#pragma once

#include "../PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Scrolling trace of the live pitch over the raga's swara lines.
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay (GamakaProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    GamakaProcessor& proc;
    gamaka::RagaScale scale;
    int lastRaga = -1, lastTuning = -1;
    float centre = 5.0f;
    std::vector<float> trace;
};

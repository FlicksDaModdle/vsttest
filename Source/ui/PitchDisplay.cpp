#include "PitchDisplay.h"
#include "GamakaLookAndFeel.h"

PitchDisplay::PitchDisplay (GamakaProcessor& p) : proc (p)
{
    trace.resize (GamakaProcessor::historySize);
    startTimerHz (30);
}

void PitchDisplay::timerCallback()
{
    const int raga = (int) proc.apvts.getRawParameterValue ("raga")->load();
    const int tuning = (int) proc.apvts.getRawParameterValue ("tuning")->load();
    if (raga != lastRaga || tuning != lastTuning)
    {
        lastRaga = raga;
        lastTuning = tuning;
        scale.set (raga, (gamaka::Tuning) tuning);
    }

    const int w = proc.historyWrite.load (std::memory_order_acquire);
    for (int i = 0; i < GamakaProcessor::historySize; ++i)
        trace[(size_t) i] = proc.pitchHistory[(size_t) ((w + i) % GamakaProcessor::historySize)].load (std::memory_order_relaxed);

    // Follow the recent pitch range smoothly.
    float sum = 0.0f;
    int count = 0;
    for (int i = GamakaProcessor::historySize - 200; i < GamakaProcessor::historySize; ++i)
        if (! std::isnan (trace[(size_t) i]))
        {
            sum += trace[(size_t) i];
            ++count;
        }
    if (count > 0)
        centre += (sum / (float) count - centre) * 0.08f;
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (palette::panel);
    g.fillRoundedRectangle (area, 6.0f);

    const float span = 16.0f;
    const float lo = centre - span * 0.5f, hi = centre + span * 0.5f;
    auto plot = area.reduced (8.0f).withTrimmedLeft (40.0f);
    auto yOf = [&] (float p) { return plot.getBottom() - (p - lo) / (hi - lo) * plot.getHeight(); };

    const int dLo = scale.nearestDegree (lo) - 1, dHi = scale.nearestDegree (hi) + 1;
    for (int d = dLo; d <= dHi; ++d)
    {
        const float p = (float) scale.degreeToPitch (d);
        if (p < lo || p > hi)
            continue;
        const float y = yOf (p);
        const bool sa = std::abs (std::fmod (p + 120.0f, 12.0f)) < 1.0e-3f;
        g.setColour (sa ? palette::saffron.withAlpha (0.45f) : palette::panelEdge.brighter (0.2f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (sa ? palette::saffron : palette::dimText);
        g.setFont (juce::FontOptions (11.0f, sa ? juce::Font::bold : juce::Font::plain));
        g.drawText (scale.degreeLabel (d), juce::Rectangle<float> (area.getX() + 4.0f, y - 8.0f, 40.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    juce::Path path;
    bool started = false;
    const int n = GamakaProcessor::historySize;
    for (int i = 0; i < n; ++i)
    {
        const float v = trace[(size_t) i];
        if (std::isnan (v))
        {
            started = false;
            continue;
        }
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
        const float y = juce::jlimit (plot.getY(), plot.getBottom(), yOf (v));
        if (! started)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
        started = true;
    }
    g.setColour (palette::saffron.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (palette::saffron);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (juce::String ("Live gamaka: ") + gamaka::gamakaName ((gamaka::GamakaMode) proc.activeGamaka.load()),
                area.reduced (12.0f, 8.0f), juce::Justification::topRight);
}

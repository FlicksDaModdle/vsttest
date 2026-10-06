#pragma once

#include "PluginProcessor.h"
#include "ui/GamakaLookAndFeel.h"
#include "ui/PitchDisplay.h"
#include "ui/SketchEditor.h"

class GamakaEditor : public juce::AudioProcessorEditor, private juce::ChangeListener, private juce::Timer
{
public:
    explicit GamakaEditor (GamakaProcessor&);
    ~GamakaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // A tab page made of titled groups of controls laid out in a flow.
    class Page;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshPluginList();
    void openPluginScanner();

    GamakaProcessor& proc;
    GamakaLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<juce::Component*> header;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<Page> performPage, soundPage;
    std::unique_ptr<SketchEditor> sketchEditor;
    std::unique_ptr<PitchDisplay> pitchDisplay;

    std::vector<std::unique_ptr<juce::TextButton>> gamakaButtons;
    juce::Label keyswitchInfo;

    // External VST3
    juce::ComboBox pluginBox;
    juce::TextButton scanButton { "Find VST3 plugins..." }, showUiButton { "Open synth window" };
    juce::Label externalHelp;
    juce::Array<juce::PluginDescription> pluginChoices;
    std::unique_ptr<juce::DocumentWindow> scannerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GamakaEditor)
};

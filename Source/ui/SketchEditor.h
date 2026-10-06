#pragma once

#include "../PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The gamaka piano roll: notes snap to the raga's swaras and each note carries
// its own bezier pitch curve. Plays with the host, previews, and renders to a
// WAV you can drag straight into the DAW.
class SketchEditor : public juce::Component,
                     public juce::DragAndDropContainer,
                     private juce::Timer
{
public:
    explicit SketchEditor (GamakaProcessor& p);
    ~SketchEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class Roll;
    class DragTile;

    void timerCallback() override;
    void commit();      // push the working copy to the processor
    void pushUndo();
    void undo();
    void applyStamp (gamaka::GamakaMode mode);
    double gridBeats() const;
    bool renderIfNeeded (juce::String& error);
    void renderToChosenFile();

    GamakaProcessor& proc;
    gamaka::Sketch sketch;
    gamaka::RagaScale scale;
    int lastRaga = -1, lastTuning = -1;
    std::vector<gamaka::Sketch> undoStack;
    std::vector<int> selection;
    bool dirtySinceRender = true;
    juce::File lastRender;

    std::unique_ptr<Roll> roll;
    std::unique_ptr<DragTile> dragTile;
    juce::ComboBox lengthBox, gridBox, stampBox;
    juce::ToggleButton ragaLock { "Raga lock" }, followHost { "Play with host" };
    juce::TextButton stampButton { "Apply" }, previewButton { "Preview" }, saveButton { "Save WAV..." },
        clearButton { "Clear" }, undoButton { "Undo" };
    juce::Label hint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    friend class Roll;
    friend class DragTile;
};

#pragma once

#include "dsp/ExternalDriver.h"
#include "dsp/Sketch.h"
#include "dsp/Synth.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

enum class SoundSource
{
    Internal = 0,
    External,
    Both
};

class GamakaProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater,
                        private juce::ChangeListener
{
public:
    GamakaProcessor();
    ~GamakaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Gamaka"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    gamaka::SynthParams readParams() const;

    //==============================================================================
    // Live state for the UI (written by the audio thread).
    static constexpr int historySize = 1024;
    static constexpr int historyHop = 128; // samples per history point
    std::array<std::atomic<float>, historySize> pitchHistory;
    std::atomic<int> historyWrite { 0 };
    std::atomic<int> activeGamaka { 0 };
    std::atomic<double> sketchPlayhead { -1.0 }; // beat within the sketch, -1 when stopped
    std::atomic<double> hostBpm { 90.0 };

    //==============================================================================
    // Sketch (the gamaka piano roll). Call on the message thread.
    gamaka::Sketch getSketch() const;
    void setSketch (const gamaka::Sketch& s);
    void setPreviewing (bool shouldPreview);
    bool isPreviewing() const { return previewing.load(); }
    double tempoForSketch() const { return hostBpm.load(); }

    // Renders the whole sketch (through the current sound source) to a WAV file.
    bool renderSketchToFile (const juce::File& file, juce::String& error);

    //==============================================================================
    // External VST3 instrument hosting (message thread).
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::PropertiesFile* getHostSettings() { return appProperties.getUserSettings(); }
    juce::File getDeadMansPedalFile() const;

    bool loadExternal (const juce::PluginDescription& desc, juce::String& error);
    void unloadExternal();
    bool hasExternal() const { return externalLoaded.load(); }
    juce::String getExternalName() const;
    void showExternalEditor();

    juce::ChangeBroadcaster externalChanged;

private:
    struct SketchAction
    {
        int sample;
        gamaka::SketchPlayer::Action action;
        float velocity;
    };

    // Plays the synth for a block, applying MIDI (unless a sketch drives it)
    // and sketch actions at their sample positions.
    static void runSynth (gamaka::Synth& s, float* mono, float* pitchOut, int n, const float* sketchPitch,
                          const std::vector<SketchAction>& actions, const juce::MidiBuffer* midi,
                          juce::MidiBuffer* forwardedControllers);
    // Fills sketchPitch / actions for one block. Returns the beat after the block.
    static double stepSketch (const gamaka::Sketch& sk, gamaka::SketchPlayer& player, double startBeat,
                              double beatsPerSample, int n, bool loop, float* pitch, float& lastPitch,
                              std::vector<SketchAction>& actions);

    bool loadExternalWithState (const juce::PluginDescription& desc, const juce::MemoryBlock* state,
                                juce::String& error);
    void pushHistory (const float* pitch, int numSamples);
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void restoreExternal (const juce::XmlElement& xml);
    void collectRetiredSketch();

    gamaka::Synth synth;
    gamaka::ExternalDriver driver;
    std::vector<gamaka::MidiOutEvent> driverEvents;
    std::vector<float> monoBuffer, pitchBuffer, midiPitchBuffer, sketchPitchBuffer;
    std::vector<SketchAction> sketchActions;
    juce::Reverb reverb;
    int historyCounter = 0;
    int lastBendRange = -1;

    // Sketch: the message thread owns `sketch`; the audio thread owns
    // `audioSketch` and swaps in new versions through `incomingSketch`.
    mutable juce::CriticalSection sketchLock;
    gamaka::Sketch sketch;
    std::unique_ptr<gamaka::Sketch> audioSketch;
    std::atomic<gamaka::Sketch*> incomingSketch { nullptr };
    std::atomic<gamaka::Sketch*> retiredSketch { nullptr };
    gamaka::SketchPlayer sketchPlayer;
    std::atomic<bool> previewing { false };
    std::atomic<bool> previewRestart { false };
    double previewBeat = 0.0;
    double lastSketchBeat = -1.0;
    float lastSketchPitch = 0.0f;
    bool sketchWasActive = false;

    // Hosted instrument
    mutable juce::ApplicationProperties appProperties;
    juce::CriticalSection externalLock;
    std::unique_ptr<juce::AudioPluginInstance> external;
    std::unique_ptr<juce::DocumentWindow> externalWindow;
    juce::PluginDescription externalDesc;
    std::atomic<bool> externalLoaded { false };
    std::atomic<bool> needDriverSetup { true };
    juce::AudioBuffer<float> externalBuffer;
    juce::MidiBuffer externalMidi, midiOutBuffer;
    juce::CriticalSection pendingLock;
    std::unique_ptr<juce::XmlElement> pendingExternalState;
    double preparedRate = 0.0;
    int preparedBlock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GamakaProcessor)
};

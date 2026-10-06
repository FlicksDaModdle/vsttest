#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace gamaka;

namespace
{
const juce::StringArray kampitaDivisions { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T" };
constexpr float kampitaDivisionBeats[] = { 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f };
const juce::StringArray externalBendRanges { "+/-2", "+/-12", "+/-24", "+/-48" };
constexpr int externalBendRangeValues[] = { 2, 12, 24, 48 };

juce::StringArray saNames()
{
    juce::StringArray names;
    for (int n = 48; n <= 72; ++n)
        names.add (juce::MidiMessage::getMidiNoteName (n, true, true, 4));
    return names;
}

juce::StringArray ragaNames()
{
    juce::StringArray names;
    for (int i = 0; i < (int) allRagas().size(); ++i)
        names.add (ragaDisplayName (i));
    return names;
}

juce::StringArray gamakaNames()
{
    juce::StringArray names;
    for (int i = 0; i < (int) GamakaMode::Count; ++i)
        names.add (gamakaName ((GamakaMode) i));
    return names;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GamakaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto toggle = [&] (const char* id, const char* name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto number = [&] (const char* id, const char* name, float lo, float hi, float def, float skewCentre,
                       const char* unit)
    {
        NormalisableRange<float> range (lo, hi);
        if (skewCentre > lo && skewCentre < hi)
            range.setSkewForCentre (skewCentre);
        const String u (unit);
        auto toText = [u] (float v, int)
        {
            if (u == "s" && std::abs (v) < 1.0f)
                return String (roundToInt (v * 1000.0f)) + " ms";
            const float a = std::abs (v);
            const String s = a >= 100.0f ? String (roundToInt (v)) : a >= 10.0f ? String (v, 1) : String (v, 2);
            return u.isEmpty() ? s : s + " " + u;
        };
        auto fromText = [] (const String& t)
        {
            const float v = t.getFloatValue();
            return t.containsIgnoreCase ("ms") ? v / 1000.0f : v;
        };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes()
                                                               .withLabel (unit)
                                                               .withStringFromValueFunction (toText)
                                                               .withValueFromStringFunction (fromText)));
    };
    auto integer = [&] (const char* id, const char* name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    // Raga & tuning
    choice ("engine", "Instrument", { "Veena", "Violin", "Venu" }, 0);
    choice ("raga", "Raga", ragaNames(), 28);
    choice ("sa", "Sa (tonic)", saNames(), 12);
    number ("saFine", "Sa fine tune", -50.0f, 50.0f, 0.0f, 0.0f, "ct");
    choice ("tuning", "Tuning", { "Equal", "Just" }, 0);
    choice ("keyMode", "Key mode", { "Swara (white keys)", "Chromatic, snap to raga", "Chromatic, free" }, 0);

    // Gamaka
    choice ("gamaka", "Gamaka", gamakaNames(), 0);
    toggle ("keyswitches", "Keyswitches", true);
    number ("kampitaRate", "Kampita rate", 1.0f, 12.0f, 5.5f, 5.0f, "Hz");
    toggle ("kampitaSync", "Kampita tempo sync", false);
    choice ("kampitaDiv", "Kampita sync length", kampitaDivisions, 2);
    number ("kampitaDepth", "Kampita depth", 0.0f, 1.2f, 0.85f, 0.0f, "");
    number ("kampitaDwell", "Kampita dwell", 0.0f, 1.0f, 0.5f, 0.0f, "");
    number ("kampitaTilt", "Kampita rise/fall", 0.0f, 1.0f, 0.5f, 0.0f, "");
    integer ("kampitaCycles", "Kampita cycles", 0, 16, 0);
    choice ("kampitaDir", "Kampita direction", { "Up", "Down", "Around" }, 0);

    number ("jaruTime", "Jaru time", 0.02f, 1.0f, 0.18f, 0.2f, "s");
    integer ("jaruSteps", "Jaru distance", 1, 4, 1);
    number ("flickTime", "Flick time", 0.02f, 0.4f, 0.09f, 0.1f, "s");
    number ("onsetDelay", "Onset delay", 0.0f, 1.0f, 0.0f, 0.2f, "s");
    choice ("glideMode", "Glide", { "Off", "Legato", "Always" }, 1);
    number ("glideTime", "Glide time", 0.0f, 1.0f, 0.12f, 0.15f, "s");
    number ("slideShape", "Slide shape", 0.0f, 1.0f, 0.5f, 0.0f, "");
    number ("smoothing", "Pitch smoothing", 0.0f, 0.06f, 0.01f, 0.015f, "s");

    number ("modDepth", "Mod wheel kampita", 0.0f, 1.2f, 1.0f, 0.0f, "");
    number ("bendRange", "Bend range", 1.0f, 12.0f, 2.0f, 0.0f, "");
    choice ("bendMode", "Bend mode", { "Semitones", "Raga steps" }, 1);

    // Sound
    number ("brightness", "Brightness", 0.0f, 1.0f, 0.6f, 0.0f, "");
    number ("decay", "Decay", 0.3f, 15.0f, 4.0f, 3.0f, "s");
    number ("body", "Body", 0.0f, 1.0f, 0.5f, 0.0f, "");
    number ("jivari", "Jivari", 0.0f, 1.0f, 0.35f, 0.0f, "");
    number ("attack", "Attack", 0.001f, 1.0f, 0.03f, 0.08f, "s");
    number ("release", "Release", 0.02f, 3.0f, 0.35f, 0.4f, "s");
    number ("breath", "Breath / bow", 0.0f, 1.0f, 0.25f, 0.0f, "");
    number ("legatoPluck", "Legato pluck", 0.0f, 1.0f, 0.3f, 0.0f, "");
    number ("velSens", "Velocity", 0.0f, 1.0f, 0.7f, 0.0f, "");
    number ("tambura", "Tambura", 0.0f, 1.0f, 0.0f, 0.0f, "");
    choice ("tamburaString", "Tambura 1st string", { "Pa", "Ma", "Ni" }, 0);
    number ("reverb", "Reverb", 0.0f, 1.0f, 0.2f, 0.0f, "");
    number ("roomSize", "Room size", 0.0f, 1.0f, 0.6f, 0.0f, "");
    number ("volume", "Volume", -24.0f, 6.0f, -6.0f, 0.0f, "dB");

    // External instrument
    choice ("source", "Sound source", { "Internal", "External VST3", "Both" }, 0);
    toggle ("followHost", "Sketch plays with host", true);
    choice ("extBend", "External bend range", externalBendRanges, 2);
    toggle ("midiOut", "Send MIDI out", false);

    return layout;
}

//==============================================================================
namespace
{
std::unique_ptr<juce::XmlElement> sketchToXml (const Sketch& sk)
{
    auto xml = std::make_unique<juce::XmlElement> ("Sketch");
    xml->setAttribute ("length", sk.lengthBeats);
    for (const auto& n : sk.notes)
    {
        auto* e = xml->createNewChildElement ("Note");
        e->setAttribute ("start", n.start);
        e->setAttribute ("length", n.length);
        e->setAttribute ("pitch", n.pitch);
        e->setAttribute ("velocity", (double) n.velocity);
        e->setAttribute ("legato", n.legato);
        for (const auto& p : n.curve)
        {
            auto* pe = e->createNewChildElement ("P");
            pe->setAttribute ("t", p.t);
            pe->setAttribute ("o", p.offset);
            pe->setAttribute ("ix", p.inDx);
            pe->setAttribute ("iy", p.inDy);
            pe->setAttribute ("ox", p.outDx);
            pe->setAttribute ("oy", p.outDy);
        }
    }
    return xml;
}

Sketch sketchFromXml (const juce::XmlElement& xml)
{
    Sketch sk;
    sk.lengthBeats = juce::jlimit (1.0, 256.0, xml.getDoubleAttribute ("length", 16.0));
    for (auto* e : xml.getChildWithTagNameIterator ("Note"))
    {
        SketchNote n;
        n.start = e->getDoubleAttribute ("start");
        n.length = juce::jmax (0.01, e->getDoubleAttribute ("length", 1.0));
        n.pitch = e->getDoubleAttribute ("pitch");
        n.velocity = (float) e->getDoubleAttribute ("velocity", 0.8);
        n.legato = e->getBoolAttribute ("legato");
        for (auto* pe : e->getChildWithTagNameIterator ("P"))
            n.curve.push_back ({ pe->getDoubleAttribute ("t"), pe->getDoubleAttribute ("o"),
                                 pe->getDoubleAttribute ("ix"), pe->getDoubleAttribute ("iy"),
                                 pe->getDoubleAttribute ("ox"), pe->getDoubleAttribute ("oy") });
        n.sortCurve();
        sk.notes.push_back (std::move (n));
    }
    sk.sort();
    return sk;
}

class ExternalWindow : public juce::DocumentWindow
{
public:
    ExternalWindow (juce::AudioProcessorEditor* editor, const juce::String& name)
        : DocumentWindow (name, juce::Colours::black, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        setAlwaysOnTop (true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { setVisible (false); }
};

// Transparent below -2 dBFS, then a soft knee that never exceeds 0 dBFS.
void softLimit (juce::AudioBuffer<float>& b, int start, int num)
{
    constexpr float knee = 0.8f, room = 1.0f - knee;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getWritePointer (ch, start);
        for (int i = 0; i < num; ++i)
        {
            const float a = std::abs (d[i]);
            if (a > knee)
                d[i] = std::copysign (knee + room * std::tanh ((a - knee) / room), d[i]);
        }
    }
}

int bendRangeFromParam (float v) { return externalBendRangeValues[juce::jlimit (0, 3, (int) v)]; }
} // namespace

//==============================================================================
GamakaProcessor::GamakaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GamakaState", createLayout())
{
    for (auto& p : pitchHistory)
        p.store (std::numeric_limits<float>::quiet_NaN());

    formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::PropertiesFile::Options options;
    options.applicationName = "Gamaka";
    options.filenameSuffix = ".settings";
    options.folderName = "Gamaka";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);
    if (auto xml = appProperties.getUserSettings()->getXmlValue ("knownPlugins"))
        knownPlugins.recreateFromXml (*xml);
    knownPlugins.addChangeListener (this);

    audioSketch = std::make_unique<Sketch>();
}

GamakaProcessor::~GamakaProcessor()
{
    cancelPendingUpdate();
    knownPlugins.removeChangeListener (this);
    externalWindow.reset();
    external.reset();
    delete incomingSketch.exchange (nullptr);
    delete retiredSketch.exchange (nullptr);
}

juce::File GamakaProcessor::getDeadMansPedalFile() const
{
    return appProperties.getUserSettings()->getFile().getSiblingFile ("RecentlyCrashedPluginsList");
}

void GamakaProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = knownPlugins.createXml())
    {
        appProperties.getUserSettings()->setValue ("knownPlugins", xml.get());
        appProperties.saveIfNeeded();
    }
}

//==============================================================================
void GamakaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    synth.setParams (readParams());
    for (auto* b : { &monoBuffer, &pitchBuffer, &midiPitchBuffer, &sketchPitchBuffer })
        b->assign ((size_t) samplesPerBlock, 0.0f);
    driverEvents.reserve (4096);
    sketchActions.reserve (256);
    externalMidi.ensureSize (8192);
    midiOutBuffer.ensureSize (8192);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    needDriverSetup = true;
    sketchPlayer.reset();

    preparedRate = sampleRate;
    preparedBlock = samplesPerBlock;
    const juce::ScopedLock sl (externalLock);
    if (external != nullptr)
        external->prepareToPlay (sampleRate, samplesPerBlock);
}

void GamakaProcessor::releaseResources()
{
    const juce::ScopedLock sl (externalLock);
    if (external != nullptr)
        external->releaseResources();
}

bool GamakaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

SynthParams GamakaProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&] (const char* id) { return (int) std::lround (v (id)); };

    SynthParams p;
    p.engine = (EngineType) i ("engine");
    p.raga = i ("raga");
    p.saMidiNote = 48 + i ("sa");
    p.saFineCents = v ("saFine");
    p.tuning = (Tuning) i ("tuning");
    p.keyMode = (KeyMode) i ("keyMode");

    p.gamaka = (GamakaMode) i ("gamaka");
    p.keyswitches = v ("keyswitches") > 0.5f;
    p.kampitaRate = v ("kampitaRate");
    p.kampitaSync = v ("kampitaSync") > 0.5f;
    p.kampitaSyncBeats = kampitaDivisionBeats[juce::jlimit (0, 5, i ("kampitaDiv"))];
    p.kampitaDepth = v ("kampitaDepth");
    p.kampitaDwell = v ("kampitaDwell");
    p.kampitaTilt = v ("kampitaTilt");
    p.kampitaCycles = i ("kampitaCycles");
    p.kampitaDir = (KampitaDirection) i ("kampitaDir");

    p.jaruTime = v ("jaruTime");
    p.jaruSteps = i ("jaruSteps");
    p.flickTime = v ("flickTime");
    p.onsetDelay = v ("onsetDelay");
    p.glideMode = (GlideMode) i ("glideMode");
    p.glideTime = v ("glideTime");
    p.slideShape = v ("slideShape");
    p.smoothing = v ("smoothing");

    p.modWheelDepth = v ("modDepth");
    p.bendRange = v ("bendRange");
    p.bendMode = (BendMode) i ("bendMode");

    p.voice.brightness = v ("brightness");
    p.voice.decay = v ("decay");
    p.voice.body = v ("body");
    p.voice.jivari = v ("jivari");
    p.voice.attack = v ("attack");
    p.voice.release = v ("release");
    p.voice.breath = v ("breath");
    p.voice.legatoPluck = v ("legatoPluck");
    p.velocitySens = v ("velSens");
    p.tamburaLevel = v ("tambura");
    p.tamburaString = i ("tamburaString");
    p.gain = juce::Decibels::decibelsToGain (v ("volume"));
    return p;
}

void GamakaProcessor::pushHistory (const float* pitch, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        if (++historyCounter >= historyHop)
        {
            historyCounter = 0;
            const int w = historyWrite.load (std::memory_order_relaxed);
            pitchHistory[(size_t) w].store (synth.isSounding() ? pitch[i] : std::numeric_limits<float>::quiet_NaN(),
                                            std::memory_order_relaxed);
            historyWrite.store ((w + 1) % historySize, std::memory_order_release);
        }
}

//==============================================================================
double GamakaProcessor::stepSketch (const Sketch& sk, SketchPlayer& player, double startBeat, double beatsPerSample,
                                    int n, bool loop, float* pitch, float& lastPitch, std::vector<SketchAction>& actions)
{
    actions.clear();
    const double length = std::max (1.0e-3, sk.lengthBeats);
    double prevLocal = -1.0;
    for (int i = 0; i < n; ++i)
    {
        const double beat = startBeat + i * beatsPerSample;
        const double local = loop ? std::fmod (std::max (0.0, beat), length) : beat;
        if (local < prevLocal - 1.0e-9 && player.isSounding())
        {
            // Looped back to the start: end the note so the first one re-articulates.
            actions.push_back ({ i, SketchPlayer::NoteOff, 0.0f });
            player.reset();
        }
        prevLocal = local;

        double p = lastPitch;
        float vel = 0.8f;
        const auto act = player.step (sk, local, p, vel);
        if (act != SketchPlayer::None)
            actions.push_back ({ i, act, vel });
        lastPitch = (float) p;
        pitch[i] = lastPitch;
    }
    return startBeat + n * beatsPerSample;
}

void GamakaProcessor::runSynth (Synth& s, float* mono, float* pitchOut, int n, const float* sketchPitch,
                                const std::vector<SketchAction>& actions, const juce::MidiBuffer* midi,
                                juce::MidiBuffer* forwardedControllers)
{
    auto midiIt = midi != nullptr ? midi->begin() : juce::MidiBufferIterator {};
    const auto midiEnd = midi != nullptr ? midi->end() : juce::MidiBufferIterator {};
    size_t act = 0;
    int pos = 0;
    while (true)
    {
        int next = n;
        if (midi != nullptr && midiIt != midiEnd)
            next = std::min (next, juce::jlimit (0, n, (*midiIt).samplePosition));
        if (act < actions.size())
            next = std::min (next, actions[act].sample);
        if (next > pos)
        {
            s.render (mono + pos, next - pos, pitchOut + pos, sketchPitch != nullptr ? sketchPitch + pos : nullptr);
            pos = next;
        }
        if (pos >= n)
            break;

        for (; midi != nullptr && midiIt != midiEnd && (*midiIt).samplePosition <= pos; ++midiIt)
        {
            const auto meta = *midiIt;
            const auto msg = meta.getMessage();
            if (sketchPitch != nullptr && (msg.isNoteOnOrOff()))
                continue; // the sketch is playing the notes
            s.handleMidi (meta.data, meta.numBytes);
            if (forwardedControllers != nullptr && msg.isController())
            {
                const int cc = msg.getControllerNumber();
                if (cc == 2 || cc == 7 || cc == 11 || cc == 74)
                    forwardedControllers->addEvent (msg, pos);
            }
        }
        for (; act < actions.size() && actions[act].sample <= pos; ++act)
        {
            if (actions[act].action == SketchPlayer::NoteOn)
                s.patternNoteOn (actions[act].velocity);
            else
                s.patternNoteOff();
        }
    }
}

void GamakaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) monoBuffer.size() < n)
        for (auto* b : { &monoBuffer, &pitchBuffer, &midiPitchBuffer, &sketchPitchBuffer })
            b->resize ((size_t) n);

    // Pick up a newly edited sketch (the old one is freed on the message thread).
    if (retiredSketch.load() == nullptr)
        if (auto* s = incomingSketch.exchange (nullptr))
        {
            retiredSketch.store (audioSketch.release());
            audioSketch.reset (s);
        }

    synth.setParams (readParams());
    bool hostPlaying = false;
    double hostPpq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
            {
                synth.setTempo (*bpm);
                hostBpm = *bpm;
            }
            hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
                hostPpq = *ppq;
        }

    const auto source = (SoundSource) (int) apvts.getRawParameterValue ("source")->load();
    const bool midiOut = apvts.getRawParameterValue ("midiOut")->load() > 0.5f;
    const bool followHost = apvts.getRawParameterValue ("followHost")->load() > 0.5f;
    const bool driveExternal = (externalLoaded && source != SoundSource::Internal) || midiOut;
    synth.setEngineMuted (source == SoundSource::External);

    // Sketch playback: preview, or following the host transport.
    const double bps = hostBpm.load() / 60.0 / getSampleRate();
    const bool hasNotes = audioSketch != nullptr && ! audioSketch->notes.empty();
    bool sketchActive = false;
    if (previewRestart.exchange (false))
    {
        previewBeat = 0.0;
        sketchPlayer.reset();
    }
    if (hasNotes && previewing)
    {
        sketchActive = true;
        previewBeat = stepSketch (*audioSketch, sketchPlayer, previewBeat, bps, n, false, sketchPitchBuffer.data(),
                                  lastSketchPitch, sketchActions);
        sketchPlayhead = previewBeat;
        if (previewBeat > audioSketch->lengthBeats + 0.25)
            previewing = false;
    }
    else if (hasNotes && followHost && hostPlaying)
    {
        sketchActive = true;
        if (! sketchWasActive || std::abs (hostPpq - lastSketchBeat) > 0.05)
            sketchPlayer.reset(); // transport started or jumped
        lastSketchBeat = stepSketch (*audioSketch, sketchPlayer, hostPpq, bps, n, true, sketchPitchBuffer.data(),
                                     lastSketchPitch, sketchActions);
        sketchPlayhead = std::fmod (lastSketchBeat, std::max (1.0e-3, audioSketch->lengthBeats));
    }
    else
    {
        sketchActions.clear();
        sketchPlayhead = -1.0;
        if (sketchWasActive)
        {
            synth.patternNoteOff();
            sketchPlayer.reset();
        }
    }
    sketchWasActive = sketchActive;

    externalMidi.clear();
    synth.beginBlock();
    runSynth (synth, monoBuffer.data(), pitchBuffer.data(), n, sketchActive ? sketchPitchBuffer.data() : nullptr,
              sketchActions, &midiMessages, &externalMidi);

    pushHistory (pitchBuffer.data(), n);
    activeGamaka = (int) synth.activeGamaka();

    // Turn the pitch curve into MIDI for the external instrument / MIDI out.
    const int bendRange = bendRangeFromParam (apvts.getRawParameterValue ("extBend")->load());
    if (bendRange != lastBendRange)
    {
        lastBendRange = bendRange;
        driver.setBendRange (bendRange);
        needDriverSetup = true;
    }
    driverEvents.clear();
    if (driveExternal)
    {
        if (needDriverSetup.exchange (false))
        {
            driver.writeReset (driverEvents);
            driver.writeSetup (driverEvents);
        }
        const double base = 69.0 + 12.0 * std::log2 (synth.saFrequency() / 440.0);
        for (int i = 0; i < n; ++i)
            midiPitchBuffer[(size_t) i] = (float) (base + pitchBuffer[(size_t) i]);
        driver.process (synth, midiPitchBuffer.data(), n, driverEvents);
    }
    else if (driver.currentNote() >= 0)
        driver.writeReset (driverEvents);

    for (const auto& e : driverEvents)
        externalMidi.addEvent (e.data, e.size, e.sample);

    midiOutBuffer.clear();
    if (midiOut)
        midiOutBuffer.addEvents (externalMidi, 0, n, 0); // before the hosted plugin can modify it

    // Audio
    buffer.clear();
    const int numCh = buffer.getNumChannels();
    if (source != SoundSource::External)
        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, monoBuffer.data(), n);

    if (externalLoaded && source != SoundSource::Internal)
    {
        const juce::ScopedTryLock tl (externalLock);
        if (tl.isLocked() && external != nullptr)
        {
            const int extCh = juce::jmax (2, external->getTotalNumInputChannels(), external->getTotalNumOutputChannels());
            externalBuffer.setSize (extCh, n, false, false, true);
            externalBuffer.clear();
            external->setPlayHead (getPlayHead());
            external->processBlock (externalBuffer, externalMidi);
            const int extOut = juce::jmax (1, external->getTotalNumOutputChannels());
            for (int ch = 0; ch < numCh; ++ch)
                buffer.addFrom (ch, 0, externalBuffer, juce::jmin (ch, extOut - 1), 0, n);
        }
    }

    const float mix = apvts.getRawParameterValue ("reverb")->load();
    if (mix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = apvts.getRawParameterValue ("roomSize")->load();
        rp.damping = 0.45f;
        rp.wetLevel = mix * 0.5f;
        rp.dryLevel = 1.0f - mix * 0.4f;
        reverb.setParameters (rp);
        if (numCh >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else
            reverb.processMono (buffer.getWritePointer (0), n);
    }
    softLimit (buffer, 0, n);

    midiMessages.swapWith (midiOutBuffer);
}

//==============================================================================
Sketch GamakaProcessor::getSketch() const
{
    const juce::ScopedLock sl (sketchLock);
    return sketch;
}

void GamakaProcessor::setSketch (const Sketch& s)
{
    {
        const juce::ScopedLock sl (sketchLock);
        sketch = s;
        sketch.sort();
    }
    collectRetiredSketch();
    delete incomingSketch.exchange (new Sketch (s));
}

void GamakaProcessor::collectRetiredSketch() { delete retiredSketch.exchange (nullptr); }

void GamakaProcessor::setPreviewing (bool shouldPreview)
{
    collectRetiredSketch();
    if (shouldPreview)
        previewRestart = true;
    previewing = shouldPreview;
}

bool GamakaProcessor::renderSketchToFile (const juce::File& file, juce::String& error)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const Sketch sk = getSketch();
    if (sk.notes.empty())
    {
        error = "The sketch has no notes yet.";
        return false;
    }

    const double sr = preparedRate > 0.0 ? preparedRate : 48000.0;
    const int block = 512;
    const double bpm = hostBpm.load();
    const auto params = readParams();
    const auto source = (SoundSource) (int) apvts.getRawParameterValue ("source")->load();

    Synth offline;
    offline.prepare (sr);
    offline.setParams (params);
    offline.setTempo (bpm);
    offline.setEngineMuted (source == SoundSource::External);

    // A private copy of the hosted instrument, so the live one keeps playing.
    std::unique_ptr<juce::AudioPluginInstance> ext;
    ExternalDriver drv;
    if (externalLoaded && source != SoundSource::Internal)
    {
        ext = formatManager.createPluginInstance (externalDesc, sr, block, error);
        if (ext == nullptr)
            return false;
        juce::MemoryBlock state;
        {
            const juce::ScopedLock sl (externalLock);
            if (external != nullptr)
                external->getStateInformation (state);
        }
        ext->enableAllBuses();
        if (state.getSize() > 0)
            ext->setStateInformation (state.getData(), (int) state.getSize());
        ext->setNonRealtime (true);
        ext->prepareToPlay (sr, block);
        drv.setBendRange (bendRangeFromParam (apvts.getRawParameterValue ("extBend")->load()));
    }

    const int total = (int) ((sk.lengthBeats * 60.0 / bpm + 4.0) * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();

    std::vector<float> mono (block), pitchOut (block), pitchIn (block), midiPitch (block);
    std::vector<SketchAction> actions;
    std::vector<MidiOutEvent> events;
    juce::AudioBuffer<float> extBuf;
    juce::MidiBuffer extMidi;
    SketchPlayer player;
    float lastPitch = 0.0f;
    double beat = 0.0;
    const double bps = bpm / 60.0 / sr;
    const double base = 69.0 + 12.0 * std::log2 (offline.saFrequency() / 440.0);

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        beat = stepSketch (sk, player, beat, bps, n, false, pitchIn.data(), lastPitch, actions);
        offline.beginBlock();
        runSynth (offline, mono.data(), pitchOut.data(), n, pitchIn.data(), actions, nullptr, nullptr);
        if (source != SoundSource::External)
            for (int ch = 0; ch < 2; ++ch)
                out.addFrom (ch, pos, mono.data(), n);

        if (ext != nullptr)
        {
            events.clear();
            if (pos == 0)
                drv.writeSetup (events);
            for (int i = 0; i < n; ++i)
                midiPitch[(size_t) i] = (float) (base + pitchOut[(size_t) i]);
            drv.process (offline, midiPitch.data(), n, events);
            extMidi.clear();
            for (const auto& e : events)
                extMidi.addEvent (e.data, e.size, e.sample);
            extBuf.setSize (juce::jmax (2, ext->getTotalNumInputChannels(), ext->getTotalNumOutputChannels()), n,
                            false, false, true);
            extBuf.clear();
            ext->processBlock (extBuf, extMidi);
            const int extOut = juce::jmax (1, ext->getTotalNumOutputChannels());
            for (int ch = 0; ch < 2; ++ch)
                out.addFrom (ch, pos, extBuf, juce::jmin (ch, extOut - 1), 0, n);
        }
    }
    if (ext != nullptr)
        ext->releaseResources();

    const float mix = apvts.getRawParameterValue ("reverb")->load();
    if (mix > 0.001f)
    {
        juce::Reverb rv;
        rv.setSampleRate (sr);
        juce::Reverb::Parameters rp;
        rp.roomSize = apvts.getRawParameterValue ("roomSize")->load();
        rp.damping = 0.45f;
        rp.wetLevel = mix * 0.5f;
        rp.dryLevel = 1.0f - mix * 0.4f;
        rv.setParameters (rp);
        rv.processStereo (out.getWritePointer (0), out.getWritePointer (1), total);
    }
    softLimit (out, 0, total);

    // Trim the silent tail (but never inside the sketch itself).
    int last = (int) (sk.lengthBeats * 60.0 / bpm * sr);
    for (int i = total - 1; i > last; --i)
        if (std::abs (out.getSample (0, i)) > 1.0e-4f || std::abs (out.getSample (1, i)) > 1.0e-4f)
        {
            last = i;
            break;
        }
    const int length = std::min (total, last + (int) (0.05 * sr));

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (sr)
                                                   .withNumChannels (2)
                                                   .withBitsPerSample (24));
    if (writer == nullptr)
    {
        error = "Couldn't create a WAV writer.";
        return false;
    }
    return writer->writeFromAudioSampleBuffer (out, 0, length);
}

//==============================================================================
bool GamakaProcessor::loadExternal (const juce::PluginDescription& desc, juce::String& error)
{
    return loadExternalWithState (desc, nullptr, error);
}

bool GamakaProcessor::loadExternalWithState (const juce::PluginDescription& desc, const juce::MemoryBlock* state,
                                             juce::String& error)
{
    JUCE_ASSERT_MESSAGE_THREAD
    const double rate = preparedRate > 0.0 ? preparedRate : 48000.0;
    const int block = preparedBlock > 0 ? preparedBlock : 512;

    auto instance = formatManager.createPluginInstance (desc, rate, block, error);
    if (instance == nullptr)
        return false;

    instance->enableAllBuses();
    if (state != nullptr && state->getSize() > 0)
        instance->setStateInformation (state->getData(), (int) state->getSize());
    instance->prepareToPlay (rate, block);

    externalWindow.reset();
    {
        const juce::ScopedLock sl (externalLock);
        std::swap (external, instance);
        externalDesc = desc;
        externalLoaded = true;
    }
    needDriverSetup = true;
    if (instance != nullptr)
        instance->releaseResources();
    instance.reset();
    externalChanged.sendChangeMessage();
    return true;
}

void GamakaProcessor::unloadExternal()
{
    JUCE_ASSERT_MESSAGE_THREAD
    externalWindow.reset();
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (externalLock);
        std::swap (external, old);
        externalLoaded = false;
        externalDesc = {};
    }
    needDriverSetup = true;
    old.reset();
    externalChanged.sendChangeMessage();
}

juce::String GamakaProcessor::getExternalName() const
{
    return externalLoaded ? externalDesc.name : juce::String();
}

void GamakaProcessor::showExternalEditor()
{
    JUCE_ASSERT_MESSAGE_THREAD
    if (external == nullptr)
        return;
    if (externalWindow == nullptr)
    {
        juce::AudioProcessorEditor* ed = external->createEditorAndMakeActive();
        if (ed == nullptr)
            ed = new juce::GenericAudioProcessorEditor (*external);
        externalWindow = std::make_unique<ExternalWindow> (ed, externalDesc.name + " (played by Gamaka)");
    }
    externalWindow->setVisible (true);
    externalWindow->toFront (true);
}

//==============================================================================
void GamakaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return;
    {
        const juce::ScopedLock sl (sketchLock);
        xml->addChildElement (sketchToXml (sketch).release());
    }
    {
        const juce::ScopedLock sl (externalLock);
        if (external != nullptr)
        {
            auto* ext = xml->createNewChildElement ("External");
            ext->addChildElement (externalDesc.createXml().release());
            juce::MemoryBlock state;
            external->getStateInformation (state);
            ext->setAttribute ("state", state.toBase64Encoding());
        }
    }
    copyXmlToBinary (*xml, destData);
}

void GamakaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    if (auto* sk = xml->getChildByName ("Sketch"))
    {
        auto restored = sketchFromXml (*sk);
        {
            const juce::ScopedLock sl (sketchLock);
            sketch = restored;
        }
        delete incomingSketch.exchange (new Sketch (std::move (restored)));
        xml->removeChildElement (sk, true);
    }

    std::unique_ptr<juce::XmlElement> ext;
    if (auto* e = xml->getChildByName ("External"))
    {
        ext = std::make_unique<juce::XmlElement> (*e);
        xml->removeChildElement (e, true);
    }
    else
        ext = std::make_unique<juce::XmlElement> ("External"); // none saved: unload any current one

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    {
        const juce::ScopedLock sl (pendingLock);
        pendingExternalState = std::move (ext);
    }
    if (juce::MessageManager::existsAndIsCurrentThread())
        handleAsyncUpdate();
    else
        triggerAsyncUpdate();
}

void GamakaProcessor::handleAsyncUpdate()
{
    std::unique_ptr<juce::XmlElement> ext;
    {
        const juce::ScopedLock sl (pendingLock);
        ext = std::move (pendingExternalState);
    }
    if (ext != nullptr)
        restoreExternal (*ext);
}

void GamakaProcessor::restoreExternal (const juce::XmlElement& xml)
{
    auto* descXml = xml.getChildElement (0);
    if (descXml == nullptr)
    {
        if (externalLoaded)
            unloadExternal();
        return;
    }
    juce::PluginDescription desc;
    if (! desc.loadFromXml (*descXml))
        return;
    juce::MemoryBlock state;
    state.fromBase64Encoding (xml.getStringAttribute ("state"));
    juce::String error;
    if (! loadExternalWithState (desc, &state, error))
        juce::Logger::writeToLog ("Gamaka: could not reload " + desc.name + ": " + error);
}

juce::AudioProcessorEditor* GamakaProcessor::createEditor() { return new GamakaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GamakaProcessor(); }

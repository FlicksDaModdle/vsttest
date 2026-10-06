#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
void prefSize (juce::Component& c, int w, int h)
{
    c.getProperties().set ("w", w);
    c.getProperties().set ("h", h);
}

struct Knob : juce::Component
{
    Knob (APVTS& state, const juce::String& id, const juce::String& name, const juce::String& tip = {})
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        slider.setTooltip (tip);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.5f));
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        prefSize (*this, 76, 96);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

struct Choice : juce::Component
{
    Choice (APVTS& state, const juce::String& id, const juce::String& name, int width = 130)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
            box.addItemList (p->choices, 1);
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::FontOptions (12.5f));
        label.setColour (juce::Label::textColourId, palette::dimText);
        addAndMakeVisible (box);
        addAndMakeVisible (label);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
        prefSize (*this, width, 44);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        box.setBounds (r.removeFromTop (24));
    }
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

struct Toggle : juce::ToggleButton
{
    Toggle (APVTS& state, const juce::String& id, const juce::String& name, int width = 140) : ToggleButton (name)
    {
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, *this);
        prefSize (*this, width, 26);
    }
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};
} // namespace

//==============================================================================
class GamakaEditor::Page : public juce::Component
{
public:
    struct Group
    {
        juce::String title;
        std::vector<juce::Component*> items;
        float widthFraction;
        juce::Rectangle<int> bounds;
    };
    struct Row
    {
        std::vector<Group> groups;
        int height;
    };

    std::vector<Row> rows;
    juce::Component* top = nullptr; // optional full-width component above the rows
    int topHeight = 0;

    void add (juce::Component& c) { addAndMakeVisible (c); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (palette::background);
        for (auto& row : rows)
            for (auto& grp : row.groups)
            {
                auto r = grp.bounds.toFloat();
                g.setColour (palette::panel);
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (palette::panelEdge);
                g.drawRoundedRectangle (r, 6.0f, 1.0f);
                g.setColour (palette::saffron);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                g.drawText (grp.title.toUpperCase(), r.reduced (10.0f, 4.0f).removeFromTop (18.0f),
                            juce::Justification::topLeft);
            }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        if (top != nullptr)
        {
            top->setBounds (area.removeFromTop (topHeight));
            area.removeFromTop (8);
        }
        for (auto& row : rows)
        {
            auto rowArea = area.removeFromTop (row.height);
            area.removeFromTop (8);
            const int total = rowArea.getWidth();
            for (auto& grp : row.groups)
            {
                grp.bounds = rowArea.removeFromLeft ((int) (total * grp.widthFraction)).reduced (4, 0);
                flow (grp);
            }
        }
    }

private:
    static void flow (Group& grp)
    {
        auto inner = grp.bounds.reduced (10).withTrimmedTop (18);
        int x = inner.getX(), y = inner.getY(), rowH = 0;
        for (auto* c : grp.items)
        {
            const int w = c->getProperties().getWithDefault ("w", 80);
            const int h = c->getProperties().getWithDefault ("h", 90);
            if (x + w > inner.getRight() && x > inner.getX())
            {
                x = inner.getX();
                y += rowH + 6;
                rowH = 0;
            }
            c->setBounds (x, y, w, h);
            x += w + 6;
            rowH = juce::jmax (rowH, h);
        }
    }
};

//==============================================================================
GamakaEditor::GamakaEditor (GamakaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    auto make = [this] (auto* c) -> auto&
    {
        owned.emplace_back (c);
        return *c;
    };

    // Header
    for (auto* c : std::initializer_list<juce::Component*> {
             &make (new Choice (s, "engine", "Instrument", 110)), &make (new Choice (s, "raga", "Raga", 280)),
             &make (new Choice (s, "sa", "Sa (shruti)", 80)), &make (new Choice (s, "tuning", "Tuning", 90)),
             &make (new Choice (s, "keyMode", "Keyboard", 190)) })
    {
        header.push_back (c);
        addAndMakeVisible (c);
    }

    // Sketch tab
    sketchEditor = std::make_unique<SketchEditor> (proc);

    // Perform tab
    performPage = std::make_unique<Page>();
    pitchDisplay = std::make_unique<PitchDisplay> (proc);
    performPage->top = pitchDisplay.get();
    performPage->topHeight = 220;
    performPage->add (*pitchDisplay);

    std::vector<juce::Component*> gamakaItems;
    for (int m = 0; m < (int) gamaka::GamakaMode::Count; ++m)
    {
        auto b = std::make_unique<juce::TextButton> (gamaka::gamakaName ((gamaka::GamakaMode) m));
        b->setTooltip (gamaka::gamakaDescription ((gamaka::GamakaMode) m));
        b->onClick = [this, m]
        {
            auto* param = proc.apvts.getParameter ("gamaka");
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) m));
            param->endChangeGesture();
        };
        prefSize (*b, 104, 30);
        performPage->add (*b);
        gamakaItems.push_back (b.get());
        gamakaButtons.push_back (std::move (b));
    }
    auto& ks = make (new Toggle (s, "keyswitches", "Keyswitches", 120));
    performPage->add (ks);
    gamakaItems.push_back (&ks);
    keyswitchInfo.setText ("MIDI notes 24-32 pick the gamaka for the next note (FL Studio: C2-G#2, Plain to Orikkai). "
                           "Mod wheel / aftertouch adds kampita to any note.",
                           juce::dontSendNotification);
    keyswitchInfo.setColour (juce::Label::textColourId, palette::dimText);
    keyswitchInfo.setFont (juce::FontOptions (12.5f));
    prefSize (keyswitchInfo, 900, 20);
    performPage->add (keyswitchInfo);
    gamakaItems.push_back (&keyswitchInfo);

    auto knobs = [&] (Page& page, std::initializer_list<juce::Component*> items)
    {
        std::vector<juce::Component*> v (items);
        for (auto* c : v)
            page.add (*c);
        return v;
    };

    performPage->rows.push_back ({ { { "Live gamaka (played from the keyboard)", gamakaItems, 1.0f, {} } }, 110 });
    performPage->rows.push_back (
        { { { "Kampita",
              knobs (*performPage,
                     { &make (new Knob (s, "kampitaRate", "Rate", "Oscillations per second")),
                       &make (new Knob (s, "kampitaDepth", "Depth", "1 = reaches the neighbouring raga note")),
                       &make (new Knob (s, "kampitaDwell", "Dwell", "How long it lingers at each turning point")),
                       &make (new Knob (s, "kampitaTilt", "Rise/Fall", "Fraction of each cycle spent rising")),
                       &make (new Knob (s, "kampitaCycles", "Cycles", "0 = keep oscillating")),
                       &make (new Choice (s, "kampitaDir", "Direction", 96)),
                       &make (new Toggle (s, "kampitaSync", "Tempo sync", 100)),
                       &make (new Choice (s, "kampitaDiv", "Sync length", 96)) }),
              0.42f, {} },
            { "Slides & flicks",
              knobs (*performPage,
                     { &make (new Knob (s, "jaruTime", "Jaru time")),
                       &make (new Knob (s, "jaruSteps", "Jaru from", "How many raga notes away a jaru starts")),
                       &make (new Knob (s, "flickTime", "Flick time", "Nokku / odukkal / sphurita / orikkai speed")),
                       &make (new Knob (s, "onsetDelay", "Delay", "Hold the note before the gamaka starts")),
                       &make (new Knob (s, "glideTime", "Glide", "Legato slide time between notes")),
                       &make (new Knob (s, "slideShape", "Shape", "Linear - S-curve - fast then settle")),
                       &make (new Knob (s, "smoothing", "Smooth")),
                       &make (new Choice (s, "glideMode", "Glide mode", 96)) }),
              0.38f, {} },
            { "Controllers",
              knobs (*performPage,
                     { &make (new Knob (s, "modDepth", "Mod wheel", "Kampita depth at full mod wheel")),
                       &make (new Knob (s, "bendRange", "Bend range")),
                       &make (new Choice (s, "bendMode", "Bend mode", 110)) }),
              0.20f, {} } },
          230 });

    // Sound & VST3 tab
    soundPage = std::make_unique<Page>();
    soundPage->rows.push_back (
        { { { "Voice",
              knobs (*soundPage,
                     { &make (new Knob (s, "brightness", "Brightness")), &make (new Knob (s, "decay", "Decay", "Veena string sustain")),
                       &make (new Knob (s, "body", "Body")), &make (new Knob (s, "jivari", "Jivari", "Veena bridge buzz")),
                       &make (new Knob (s, "attack", "Attack", "Violin / venu")), &make (new Knob (s, "release", "Release")),
                       &make (new Knob (s, "breath", "Bow/Breath")),
                       &make (new Knob (s, "legatoPluck", "Legato pluck", "Re-articulation on legato notes")),
                       &make (new Knob (s, "velSens", "Velocity")) }),
              0.62f, {} },
            { "Tambura & output",
              knobs (*soundPage,
                     { &make (new Knob (s, "tambura", "Tambura", "Sruti drone tuned to Sa")),
                       &make (new Choice (s, "tamburaString", "1st string", 80)),
                       &make (new Knob (s, "reverb", "Reverb")), &make (new Knob (s, "roomSize", "Room")),
                       &make (new Knob (s, "volume", "Volume")), &make (new Knob (s, "saFine", "Sa fine")) }),
              0.38f, {} } },
          240 });

    pluginBox.setTextWhenNothingSelected ("Choose a VST3 instrument...");
    pluginBox.onChange = [this]
    {
        const int id = pluginBox.getSelectedId();
        if (id == 1)
            proc.unloadExternal();
        else if (id >= 2 && id - 2 < pluginChoices.size())
        {
            juce::String error;
            if (! proc.loadExternal (pluginChoices[id - 2], error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load plugin", error);
        }
    };
    prefSize (pluginBox, 300, 26);
    scanButton.onClick = [this] { openPluginScanner(); };
    prefSize (scanButton, 160, 26);
    showUiButton.onClick = [this] { proc.showExternalEditor(); };
    prefSize (showUiButton, 150, 26);
    externalHelp.setText ("Gamaka plays the loaded synth with one note per articulation plus a pitch-bend stream "
                          "carrying the gamaka. Set the synth to mono / legato and its pitch-bend range to the value "
                          "below (Gamaka also sends RPN 0, which many synths follow automatically). "
                          "\"Send MIDI out\" also outputs this MIDI - in FL Studio set Gamaka's MIDI output port and "
                          "the target synth's input port to the same number.",
                          juce::dontSendNotification);
    externalHelp.setColour (juce::Label::textColourId, palette::dimText);
    externalHelp.setFont (juce::FontOptions (12.5f));
    externalHelp.setJustificationType (juce::Justification::topLeft);
    prefSize (externalHelp, 1080, 54);

    soundPage->rows.push_back (
        { { { "External VST3 instrument",
              knobs (*soundPage, { &pluginBox, &scanButton, &showUiButton,
                                   &make (new Choice (s, "source", "Sound source", 130)),
                                   &make (new Choice (s, "extBend", "Synth bend range", 110)),
                                   &make (new Toggle (s, "midiOut", "Send MIDI out", 130)), &externalHelp }),
              1.0f, {} } },
          150 });

    tabs.setTabBarDepth (34);
    tabs.addTab ("Sketch", palette::background, sketchEditor.get(), false);
    tabs.addTab ("Perform", palette::background, performPage.get(), false);
    tabs.addTab ("Sound & VST3", palette::background, soundPage.get(), false);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    proc.knownPlugins.addChangeListener (this);
    proc.externalChanged.addChangeListener (this);
    refreshPluginList();

    setResizable (true, true);
    setResizeLimits (1000, 640, 2000, 1400);
    AudioProcessorEditor::setSize (1180, 760);
    startTimerHz (15);
}

GamakaEditor::~GamakaEditor()
{
    proc.knownPlugins.removeChangeListener (this);
    proc.externalChanged.removeChangeListener (this);
    scannerWindow.reset();
    tabs.clearTabs();
    setLookAndFeel (nullptr);
}

void GamakaEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::background);
    auto r = getLocalBounds().removeFromTop (58).reduced (14, 6);
    g.setColour (palette::saffron);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GAMAKA", r.removeFromLeft (150), juce::Justification::centredLeft);
    g.setColour (palette::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Carnatic gamaka sketcher", getLocalBounds().removeFromTop (58).reduced (14, 6).withTrimmedTop (32),
                juce::Justification::topLeft);
}

void GamakaEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (58).reduced (12, 6);
    top.removeFromLeft (190);
    for (auto* c : header)
    {
        const int w = c->getProperties().getWithDefault ("w", 120);
        c->setBounds (top.removeFromLeft (w).withSizeKeepingCentre (w, 44));
        top.removeFromLeft (10);
    }
    tabs.setBounds (r);
}

void GamakaEditor::timerCallback()
{
    const int active = proc.activeGamaka.load();
    for (int i = 0; i < (int) gamakaButtons.size(); ++i)
        gamakaButtons[(size_t) i]->setToggleState (i == active, juce::dontSendNotification);
    showUiButton.setEnabled (proc.hasExternal());
}

void GamakaEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshPluginList(); }

void GamakaEditor::refreshPluginList()
{
    pluginChoices.clear();
    pluginBox.clear (juce::dontSendNotification);
    pluginBox.addItem ("None (built-in voices only)", 1);
    for (const auto& d : proc.knownPlugins.getTypes())
        if (d.isInstrument && d.name != "Gamaka")
            pluginChoices.add (d);
    for (int i = 0; i < pluginChoices.size(); ++i)
        pluginBox.addItem (pluginChoices[i].name + "  (" + pluginChoices[i].manufacturerName + ")", i + 2);

    if (proc.hasExternal())
    {
        const auto name = proc.getExternalName();
        for (int i = 0; i < pluginChoices.size(); ++i)
            if (pluginChoices[i].name == name)
                pluginBox.setSelectedId (i + 2, juce::dontSendNotification);
    }
    else
        pluginBox.setSelectedId (1, juce::dontSendNotification);
}

void GamakaEditor::openPluginScanner()
{
    struct ScannerWindow : juce::DocumentWindow
    {
        ScannerWindow (GamakaEditor& e)
            : DocumentWindow ("Gamaka - VST3 plugins", palette::background, DocumentWindow::closeButton), editor (e)
        {
            setUsingNativeTitleBar (true);
            auto* list = new juce::PluginListComponent (e.proc.formatManager, e.proc.knownPlugins,
                                                        e.proc.getDeadMansPedalFile(), e.proc.getHostSettings());
            list->setSize (640, 480);
            setContentOwned (list, true);
            setResizable (true, false);
            setAlwaysOnTop (true);
            centreWithSize (640, 480);
            setVisible (true);
        }
        void closeButtonPressed() override
        {
            juce::Component::SafePointer<GamakaEditor> safe (&editor);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->scannerWindow.reset(); });
        }
        GamakaEditor& editor;
    };

    if (scannerWindow == nullptr)
        scannerWindow = std::make_unique<ScannerWindow> (*this);
    scannerWindow->toFront (true);
}

# Gamaka

A VST3 instrument for drawing and playing **Carnatic gamakas**. It works like a piano roll where every
note carries its own bezier pitch curve (in the spirit of MIDI Sketch), but it's built around ragas:
notes and curve points snap to the raga's swaras, and gamakas like kampita, jaru and nokku are one click
away because Gamaka knows each note's raga neighbours.

You can play the result with the built-in veena / violin / venu voices or through any VST3 synth you own,
render it to a WAV, and drag that straight into FL Studio (or any DAW).

## What's inside

**Sketch tab: the gamaka piano roll**
- Rows are labelled with swaras (`S R2 G3 M2 P D2 N3 S'`, with `.` for mandra and `'` for tara), and swaras
  outside the raga are greyed out. With **Raga lock** on, everything snaps to the raga.
- Click to draw a note and drag to set its length.
- **Right-click → Gamaka** (or select notes and press `1`–`9`) stamps a shape that's sized to the raga:
  | Key | Gamaka | Shape |
  |---|---|---|
  | 1 | Plain | steady note |
  | 2 | Kampita | oscillates between the note and its upper raga neighbour |
  | 3 | Jaru up (etra jaru) | slides up into the note from the raga note below |
  | 4 | Jaru down (irakka jaru) | slides down into the note from above |
  | 5 | Nokku | starts on the upper note and pushes down onto the note |
  | 6 | Odukkal | pushes up onto the note from below |
  | 7 | Sphurita | dips to the lower note and back |
  | 8 | Pratyahata | flicks to the upper note and back |
  | 9 | Orikkai | flicks up to the upper note at the end of the note |
- **Double-click a note** to add a bezier point (double-click a point to remove it). Drag the round
  point to move it, and drag the square handles to shape the curve. Handles stay smooth unless you hold
  **Alt**, which breaks the tangent and also turns off snapping.
- Mark a note **Legato** (right-click) to make it continue the previous note's sound instead of re-plucking.
- `Ctrl+Z` undo, `Ctrl+D` duplicate, `Del` delete, `Space` preview.
- **Play with host**: the sketch plays in sync with the DAW transport and loops over its length. While it
  plays it takes over the instrument, so incoming MIDI notes are ignored.
- **Drag WAV into your DAW**: click the tile to render, or just start dragging and it renders first. Drop
  the file onto FL Studio's playlist or a sampler. Renders are saved in `Documents/Gamaka Renders` so the
  files stay where FL expects them. **Save WAV…** lets you choose the location yourself.

**Perform tab: live gamakas from a keyboard or FL's piano roll**
- With **Swara** key mode, the white keys play the raga: C = S, D = R, E = G, … with C4 (MIDI 60) as
  middle Sa. Any Sa can be chosen as the shruti. Black keys play chromatic anya swaras. There are also
  two chromatic modes: snap-to-raga and free.
- **Keyswitches** MIDI 24–32 (FL Studio: C2–G#2) choose the gamaka for the following notes, in the same
  order as the table above.
- Legato notes glide, and a slide's shape runs from linear to S-curve to fast-then-settle. Holding one
  note and tapping another gives a raga-aware trill.
- The **mod wheel / aftertouch** adds kampita to any note. Pitch bend can move in **raga steps**
  instead of semitones.
- A live pitch trace is drawn over the raga's swara lines.

**Sound & VST3 tab**
- Built-in voices:
  - **Veena**: a waveguide string, so one pluck rings through the whole slide, with jivari bridge buzz.
  - **Violin**: bowed.
  - **Venu**: flute with breath.
- A **tambura** drone (Pa/Ma/Ni first string) and reverb.
- **External VST3 instrument**: click **Find VST3 plugins…**, scan, and pick your synth (Serum, Vital,
  Pigments…). Set **Sound source** to *External VST3* or *Both*. Gamaka plays it with one note per
  articulation plus a dense pitch-bend stream that carries the gamaka.
  - Set the synth to **mono/legato** and its pitch-bend range to match **Synth bend range** (±24 by
    default). Gamaka also sends RPN 0, which many synths pick up automatically.
  - Renders and drag-out go through the hosted synth too, using a private copy so playback isn't
    interrupted.
  - **Send MIDI out** outputs the same MIDI. In FL Studio, set Gamaka's *MIDI output port* and the target
    plugin's *input port* to the same number to drive a synth on another channel.

72 melakartas (computed from the katapayadi numbering) and 20 common janya ragas are included, with
equal or just (5-limit) intonation.

## Installing

Download the build for your OS from the latest **Actions → Build VST3** run (artifact `Gamaka-VST3-…`)
and copy `Gamaka.vst3` to:
- **Windows:** `C:\Program Files\Common Files\VST3\`
- **macOS:** `~/Library/Audio/Plug-Ins/VST3/`. The build isn't signed; if macOS blocks it, run
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Gamaka.vst3`.

In FL Studio, open *Options → Manage plugins → Find more plugins*, then add **Gamaka** to the channel rack.

## Building from source

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release        # downloads JUCE 8 automatically
cmake --build build --config Release --target Gamaka_VST3 gamaka_tests
ctest --test-dir build -C Release
```

To use an existing JUCE checkout, pass `-DJUCE_DIR=/path/to/JUCE`. The DSP core in `Source/dsp`
(ragas, gamaka engine, voices, sketch model, external-synth driver) is plain C++ with no JUCE dependency,
and `tests/core_tests.cpp` checks it end to end. The tests cover gamaka pitch trajectories against raga
neighbours, legato and keyswitches, tuning accuracy of every voice, the MIDI pitch-bend driver, and
sketch rendering. Run `gamaka_tests <dir>` to also write demo WAVs.

## License

Gamaka is built with [JUCE](https://juce.com), which is available under the AGPLv3 or a commercial
JUCE license. Distributing builds of Gamaka therefore requires following one of those licenses.

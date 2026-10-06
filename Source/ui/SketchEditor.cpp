#include "SketchEditor.h"
#include "GamakaLookAndFeel.h"

using namespace gamaka;

namespace
{
constexpr double gridValues[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
constexpr int barCounts[] = { 1, 2, 4, 8, 16, 32 };
constexpr int visibleRows = 26;
} // namespace

//==============================================================================
class SketchEditor::Roll : public juce::Component
{
public:
    explicit Roll (SketchEditor& o) : owner (o) {}

    static constexpr int keysWidth = 58;
    double topPitch = 18.5;

    float rowH() const { return (float) getHeight() / (float) visibleRows; }
    float yOf (double pitch) const { return (float) ((topPitch - pitch) * rowH()); }
    double pitchAt (float y) const { return topPitch - y / rowH(); }
    float xOf (double beat) const
    {
        return keysWidth + (float) (beat / owner.sketch.lengthBeats) * (float) (getWidth() - keysWidth);
    }
    double beatAt (float x) const
    {
        return (x - keysWidth) / (double) juce::jmax (1, getWidth() - keysWidth) * owner.sketch.lengthBeats;
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        auto& sk = owner.sketch;
        const auto& scale = owner.scale;
        const float rh = rowH();
        g.fillAll (palette::panel);

        // Rows: raga swaras highlighted, other semitones dimmed.
        const int kLo = (int) std::floor (topPitch - visibleRows) - 1, kHi = (int) std::ceil (topPitch) + 1;
        for (int k = kLo; k <= kHi; ++k)
        {
            const double p = scale.chromaticPitch (k);
            const bool inRaga = std::abs (scale.snap (p) - p) < 1.0e-6;
            const bool sa = ((k % 12) + 12) % 12 == 0;
            const float y = yOf ((double) k) - rh * 0.5f;
            g.setColour (inRaga ? (sa ? palette::saffron.withAlpha (0.16f) : palette::panel.brighter (0.1f))
                                : palette::background.withAlpha (0.55f));
            g.fillRect (0.0f, y, (float) getWidth(), rh);
            g.setColour (palette::background.withAlpha (0.6f));
            g.drawHorizontalLine ((int) (y + rh), 0.0f, (float) getWidth());

            // Key column labels
            g.setColour (inRaga ? (sa ? palette::saffron : palette::text) : palette::dimText.withAlpha (0.5f));
            g.setFont (juce::FontOptions (juce::jmin (13.0f, rh * 0.75f), sa ? juce::Font::bold : juce::Font::plain));
            const auto label = inRaga ? juce::String (scale.degreeLabel (scale.nearestDegree (p))) : juce::String ("-");
            g.drawText (label, juce::Rectangle<float> (4.0f, y, (float) keysWidth - 8.0f, rh),
                        juce::Justification::centredLeft);
        }
        g.setColour (palette::panelEdge);
        g.fillRect (keysWidth - 2, 0, 2, getHeight());

        // Beat grid
        const double grid = owner.gridBeats();
        for (double b = 0.0; b <= sk.lengthBeats + 1.0e-9; b += grid)
        {
            const bool bar = std::abs (std::fmod (b + 1.0e-9, 4.0)) < 1.0e-6;
            const bool beat = std::abs (b - std::round (b)) < 1.0e-6;
            g.setColour (bar ? palette::dimText.withAlpha (0.55f)
                             : beat ? palette::panelEdge.brighter (0.25f) : palette::panelEdge.withAlpha (0.5f));
            g.drawVerticalLine ((int) xOf (b), 0.0f, (float) getHeight());
        }

        // Notes and their curves
        for (int i = 0; i < (int) sk.notes.size(); ++i)
        {
            const auto& n = sk.notes[(size_t) i];
            const bool selected = isSelected (i);
            const auto r = noteRect (n);
            g.setColour ((selected ? palette::kumkum : palette::peacock).withAlpha (0.45f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (selected ? palette::kumkum.brighter (0.4f) : palette::peacock.brighter (0.2f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            if (n.legato)
            {
                g.setColour (palette::text.withAlpha (0.7f));
                g.fillEllipse (r.getX() - 3.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
            }
            g.setColour (palette::text);
            g.setFont (juce::FontOptions (juce::jmin (12.0f, rh * 0.7f)));
            if (r.getWidth() > 22.0f)
                g.drawText (scale.degreeLabel (scale.nearestDegree (n.pitch)), r.reduced (4.0f, 0.0f),
                            juce::Justification::centredLeft);

            juce::Path curve;
            const float x0 = xOf (n.start), x1 = xOf (n.end());
            curve.startNewSubPath (x0, yOf (n.pitch + n.offsetAt (0.0)));
            for (float x = x0 + 2.0f; x <= x1 + 0.01f; x += 2.0f)
                curve.lineTo (x, yOf (n.pitch + n.offsetAt (juce::jmin (n.length, beatAt (x) - n.start))));
            g.setColour (palette::saffron.withAlpha (0.25f));
            g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (selected ? palette::saffron.brighter (0.3f) : palette::saffron);
            g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Control points and bezier handles of the focused note
        if (const int f = focusNote(); f >= 0)
        {
            const auto& n = sk.notes[(size_t) f];
            for (int i = 0; i < (int) n.curve.size(); ++i)
            {
                const auto pt = pointPos (n, i);
                const auto in = handlePos (n, i, true), out = handlePos (n, i, false);
                g.setColour (palette::text.withAlpha (0.6f));
                g.drawLine ({ in, pt }, 1.0f);
                g.drawLine ({ pt, out }, 1.0f);
                g.setColour (palette::peacock.brighter (0.4f));
                g.fillRect (juce::Rectangle<float> (7.0f, 7.0f).withCentre (in));
                g.fillRect (juce::Rectangle<float> (7.0f, 7.0f).withCentre (out));
                const bool active = i == focusPoint;
                g.setColour (active ? palette::kumkum : palette::saffron);
                g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt));
                g.setColour (palette::background);
                g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt), 1.5f);
            }
        }

        // Playhead
        const double ph = owner.proc.sketchPlayhead.load();
        if (ph >= 0.0)
        {
            g.setColour (palette::text.withAlpha (0.85f));
            g.drawVerticalLine ((int) xOf (ph), 0.0f, (float) getHeight());
        }

        if (sk.notes.empty())
        {
            g.setColour (palette::dimText);
            g.setFont (juce::FontOptions (15.0f));
            g.drawFittedText ("Click to draw a swara (drag to set its length).\n"
                              "Right-click a note to stamp a gamaka (kampita, jaru, nokku...).\n"
                              "Double-click a note to add a bezier point; drag the square handles to shape it.",
                              getLocalBounds().withTrimmedLeft (keysWidth).reduced (40), juce::Justification::centred, 4);
        }
    }

    //==========================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        bool edge = false;
        const int n = hitNote (e.position, edge);
        setMouseCursor (n >= 0 && edge ? juce::MouseCursor::LeftRightResizeCursor
                        : n >= 0       ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        auto& sk = owner.sketch;
        drag = Drag::None;

        // Bezier handles / points of the focused note take priority.
        if (const int f = focusNote(); f >= 0)
        {
            const auto& n = sk.notes[(size_t) f];
            for (int i = 0; i < (int) n.curve.size(); ++i)
            {
                if (e.position.getDistanceFrom (pointPos (n, i)) < 8.0f)
                {
                    focusPoint = i;
                    if (e.mods.isPopupMenu())
                        return showPointMenu (f, i);
                    owner.pushUndo();
                    drag = Drag::Point;
                    dragNote = f;
                    repaint();
                    return;
                }
                for (bool in : { true, false })
                    if (e.position.getDistanceFrom (handlePos (n, i, in)) < 7.0f)
                    {
                        owner.pushUndo();
                        focusPoint = i;
                        drag = in ? Drag::InHandle : Drag::OutHandle;
                        dragNote = f;
                        return;
                    }
            }
        }

        bool edge = false;
        const int hit = hitNote (e.position, edge);
        if (hit >= 0)
        {
            if (e.mods.isShiftDown())
            {
                auto it = std::find (owner.selection.begin(), owner.selection.end(), hit);
                if (it != owner.selection.end())
                    owner.selection.erase (it);
                else
                    owner.selection.push_back (hit);
            }
            else if (! isSelected (hit) || e.mods.isPopupMenu())
                owner.selection = { hit };
            else
            {
                // bring to front as the focused note
                owner.selection.erase (std::find (owner.selection.begin(), owner.selection.end(), hit));
                owner.selection.push_back (hit);
            }
            focusPoint = -1;
            if (e.mods.isPopupMenu())
            {
                repaint();
                return showNoteMenu (hit);
            }
            owner.pushUndo();
            dragNote = hit;
            drag = edge ? Drag::Resize : Drag::Move;
            grabOffset = beatAt (e.position.x) - sk.notes[(size_t) hit].start;
            repaint();
            return;
        }

        if (e.mods.isPopupMenu() || e.position.x < keysWidth)
        {
            owner.selection.clear();
            repaint();
            return;
        }

        // Draw a new note.
        owner.pushUndo();
        SketchNote n;
        n.start = juce::jlimit (0.0, sk.lengthBeats - owner.gridBeats(), snapFloor (beatAt (e.position.x), e.mods));
        n.length = juce::jmin (lastLength, sk.lengthBeats - n.start);
        n.pitch = snapPitch (pitchAt (e.position.y), e.mods);
        sk.notes.push_back (n);
        owner.selection = { (int) sk.notes.size() - 1 };
        focusPoint = -1;
        dragNote = owner.selection.back();
        drag = Drag::Resize;
        owner.commit();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& sk = owner.sketch;
        if (drag == Drag::None || dragNote < 0 || dragNote >= (int) sk.notes.size())
            return;
        auto& n = sk.notes[(size_t) dragNote];

        switch (drag)
        {
            case Drag::Move:
            {
                n.start = juce::jlimit (0.0, juce::jmax (0.0, sk.lengthBeats - n.length),
                                        snapRound (beatAt (e.position.x) - grabOffset, e.mods));
                n.pitch = snapPitch (pitchAt (e.position.y), e.mods);
                break;
            }
            case Drag::Resize:
            {
                const double end = juce::jlimit (n.start, sk.lengthBeats, snapRound (beatAt (e.position.x), e.mods));
                n.length = juce::jmax (e.mods.isAltDown() ? 0.02 : owner.gridBeats(), end - n.start);
                lastLength = n.length;
                break;
            }
            case Drag::Point:
            {
                auto& p = n.curve[(size_t) focusPoint];
                p.t = juce::jlimit (0.0, n.length, beatAt (e.position.x) - n.start);
                const double abs = pitchAt (e.position.y);
                p.offset = (e.mods.isAltDown() ? abs : snapPitch (abs, e.mods)) - n.pitch;
                focusPoint = sortCurveTracking (n, focusPoint);
                break;
            }
            case Drag::InHandle:
            case Drag::OutHandle:
            {
                auto& p = n.curve[(size_t) focusPoint];
                const double dx = beatAt (e.position.x) - (n.start + p.t);
                const double dy = pitchAt (e.position.y) - (n.pitch + p.offset);
                const bool in = drag == Drag::InHandle;
                (in ? p.inDx : p.outDx) = in ? juce::jmin (0.0, dx) : juce::jmax (0.0, dx);
                (in ? p.inDy : p.outDy) = dy;
                if (! e.mods.isAltDown()) // keep the tangent smooth (Alt breaks it)
                {
                    (in ? p.outDx : p.inDx) = -(in ? p.inDx : p.outDx);
                    (in ? p.outDy : p.inDy) = -(in ? p.inDy : p.outDy);
                }
                break;
            }
            case Drag::None:
                break;
        }
        owner.commit();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == Drag::Move || drag == Drag::Resize)
            sortKeepingSelection();
        drag = Drag::None;
        owner.commit();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto& sk = owner.sketch;
        const int f = focusNote();
        if (f < 0)
            return;
        auto& n = sk.notes[(size_t) f];

        for (int i = 0; i < (int) n.curve.size(); ++i)
            if (e.position.getDistanceFrom (pointPos (n, i)) < 8.0f)
            {
                owner.pushUndo();
                n.curve.erase (n.curve.begin() + i);
                focusPoint = -1;
                owner.commit();
                return;
            }

        const double t = beatAt (e.position.x) - n.start;
        if (t < 0.0 || t > n.length)
            return;
        owner.pushUndo();
        if (n.curve.empty() && t > 0.02)
            n.curve.push_back ({ 0.0, 0.0, -0.05, 0.0, 0.05, 0.0 }); // anchor the start on the note
        CurvePoint p;
        p.t = t;
        const double abs = pitchAt (e.position.y);
        p.offset = (e.mods.isAltDown() ? abs : snapPitch (abs, e.mods)) - n.pitch;
        const double h = juce::jlimit (0.02, 0.25, n.length * 0.1);
        p.inDx = -h;
        p.outDx = h;
        n.curve.push_back (p);
        focusPoint = sortCurveTracking (n, (int) n.curve.size() - 1);
        owner.commit();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        topPitch = juce::jlimit (-12.0 + visibleRows, 36.0, topPitch + w.deltaY * 6.0);
        repaint();
    }

    int focusNote() const
    {
        if (owner.selection.empty())
            return -1;
        const int f = owner.selection.back();
        return f < (int) owner.sketch.notes.size() ? f : -1;
    }

    int focusPoint = -1;

    void deleteSelection()
    {
        auto& sk = owner.sketch;
        const int f = focusNote();
        owner.pushUndo();
        if (f >= 0 && focusPoint >= 0 && focusPoint < (int) sk.notes[(size_t) f].curve.size())
        {
            auto& c = sk.notes[(size_t) f].curve;
            c.erase (c.begin() + focusPoint);
            focusPoint = -1;
        }
        else
        {
            auto sel = owner.selection;
            std::sort (sel.rbegin(), sel.rend());
            for (int i : sel)
                if (i < (int) sk.notes.size())
                    sk.notes.erase (sk.notes.begin() + i);
            owner.selection.clear();
        }
        owner.commit();
    }

    void duplicateSelection()
    {
        auto& sk = owner.sketch;
        if (owner.selection.empty())
            return;
        owner.pushUndo();
        double first = 1.0e9, last = 0.0;
        for (int i : owner.selection)
        {
            first = juce::jmin (first, sk.notes[(size_t) i].start);
            last = juce::jmax (last, sk.notes[(size_t) i].end());
        }
        const double shift = last - first;
        std::vector<int> fresh;
        for (int i : owner.selection)
        {
            auto copy = sk.notes[(size_t) i];
            copy.start += shift;
            if (copy.end() <= sk.lengthBeats + 1.0e-9)
            {
                sk.notes.push_back (copy);
                fresh.push_back ((int) sk.notes.size() - 1);
            }
        }
        owner.selection = fresh;
        sortKeepingSelection();
        owner.commit();
    }

private:
    enum class Drag { None, Move, Resize, Point, InHandle, OutHandle };

    // Sorts a note's curve by time and returns where point `index` ended up.
    static int sortCurveTracking (SketchNote& n, int index)
    {
        std::vector<int> order ((size_t) n.curve.size());
        for (int i = 0; i < (int) order.size(); ++i)
            order[(size_t) i] = i;
        std::stable_sort (order.begin(), order.end(),
                          [&n] (int a, int b) { return n.curve[(size_t) a].t < n.curve[(size_t) b].t; });
        std::vector<CurvePoint> sorted;
        int found = -1;
        for (int i = 0; i < (int) order.size(); ++i)
        {
            sorted.push_back (n.curve[(size_t) order[(size_t) i]]);
            if (order[(size_t) i] == index)
                found = i;
        }
        n.curve = std::move (sorted);
        return found;
    }

    bool isSelected (int i) const
    {
        return std::find (owner.selection.begin(), owner.selection.end(), i) != owner.selection.end();
    }

    juce::Rectangle<float> noteRect (const SketchNote& n) const
    {
        const float rh = rowH();
        return { xOf (n.start), yOf (n.pitch) - rh * 0.5f + 1.0f, juce::jmax (4.0f, xOf (n.end()) - xOf (n.start)), rh - 2.0f };
    }

    juce::Point<float> pointPos (const SketchNote& n, int i) const
    {
        const auto& p = n.curve[(size_t) i];
        return { xOf (n.start + p.t), yOf (n.pitch + p.offset) };
    }

    juce::Point<float> handlePos (const SketchNote& n, int i, bool in) const
    {
        const auto& p = n.curve[(size_t) i];
        return { xOf (n.start + p.t + (in ? p.inDx : p.outDx)), yOf (n.pitch + p.offset + (in ? p.inDy : p.outDy)) };
    }

    int hitNote (juce::Point<float> pos, bool& edge) const
    {
        const auto& sk = owner.sketch;
        for (int i = (int) sk.notes.size() - 1; i >= 0; --i)
        {
            const auto& n = sk.notes[(size_t) i];
            const auto r = noteRect (n);
            const double t = beatAt (pos.x) - n.start;
            const bool onCurve = t >= 0.0 && t <= n.length && std::abs (yOf (n.pitch + n.offsetAt (t)) - pos.y) < 6.0f;
            if (r.contains (pos) || onCurve)
            {
                edge = pos.x > r.getRight() - 6.0f;
                return i;
            }
        }
        return -1;
    }

    double snapFloor (double beat, const juce::ModifierKeys& m) const
    {
        const double g = owner.gridBeats();
        return m.isAltDown() ? beat : std::floor (beat / g + 1.0e-6) * g;
    }
    double snapRound (double beat, const juce::ModifierKeys& m) const
    {
        const double g = owner.gridBeats();
        return m.isAltDown() ? beat : std::round (beat / g) * g;
    }
    double snapPitch (double p, const juce::ModifierKeys&) const
    {
        return owner.ragaLock.getToggleState() ? owner.scale.snap (p)
                                               : owner.scale.chromaticPitch ((int) std::lround (p));
    }

    void sortKeepingSelection()
    {
        auto& sk = owner.sketch;
        std::vector<int> order ((size_t) sk.notes.size());
        for (int i = 0; i < (int) order.size(); ++i)
            order[(size_t) i] = i;
        std::stable_sort (order.begin(), order.end(),
                          [&] (int a, int b) { return sk.notes[(size_t) a].start < sk.notes[(size_t) b].start; });
        std::vector<SketchNote> sorted;
        std::vector<int> newIndex (order.size());
        for (int i = 0; i < (int) order.size(); ++i)
        {
            sorted.push_back (sk.notes[(size_t) order[(size_t) i]]);
            newIndex[(size_t) order[(size_t) i]] = i;
        }
        sk.notes = std::move (sorted);
        for (auto& s : owner.selection)
            s = newIndex[(size_t) s];
        if (dragNote >= 0 && dragNote < (int) newIndex.size())
            dragNote = newIndex[(size_t) dragNote];
    }

    void showNoteMenu (int index)
    {
        auto& n = owner.sketch.notes[(size_t) index];
        juce::PopupMenu stamps;
        for (int m = 0; m < (int) GamakaMode::Count; ++m)
            stamps.addItem (100 + m, juce::String (m + 1) + "  " + gamakaName ((GamakaMode) m) + "  -  "
                                         + gamakaDescription ((GamakaMode) m));
        juce::PopupMenu vel;
        for (int v : { 40, 60, 80, 100, 127 })
            vel.addItem (200 + v, juce::String (v), true, std::abs (n.velocity * 127.0f - (float) v) < 2.0f);

        juce::PopupMenu menu;
        menu.addSubMenu ("Gamaka", stamps);
        menu.addItem (1, "Legato (no re-pluck)", true, n.legato);
        menu.addSubMenu ("Velocity", vel);
        menu.addItem (2, "Duplicate   (Ctrl+D)");
        menu.addItem (3, "Delete   (Del)");

        juce::Component::SafePointer<Roll> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe, index] (int r)
                            {
                                if (safe == nullptr || r == 0 || index >= (int) safe->owner.sketch.notes.size())
                                    return;
                                auto& o = safe->owner;
                                if (r >= 100 && r < 200)
                                    return o.applyStamp ((GamakaMode) (r - 100));
                                if (r == 2)
                                    return safe->duplicateSelection();
                                if (r == 3)
                                    return safe->deleteSelection();
                                o.pushUndo();
                                for (int i : o.selection)
                                {
                                    auto& note = o.sketch.notes[(size_t) i];
                                    if (r == 1)
                                        note.legato = ! o.sketch.notes[(size_t) index].legato;
                                    else if (r >= 200)
                                        note.velocity = (float) (r - 200) / 127.0f;
                                }
                                o.commit();
                            });
    }

    void showPointMenu (int noteIndex, int pointIndex)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Smooth (flat bezier handles)");
        menu.addItem (2, "Sharp corner");
        menu.addItem (3, "Delete point");
        juce::Component::SafePointer<Roll> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe, noteIndex, pointIndex] (int r)
                            {
                                if (safe == nullptr || r == 0)
                                    return;
                                auto& o = safe->owner;
                                if (noteIndex >= (int) o.sketch.notes.size())
                                    return;
                                auto& c = o.sketch.notes[(size_t) noteIndex].curve;
                                if (pointIndex >= (int) c.size())
                                    return;
                                o.pushUndo();
                                auto& p = c[(size_t) pointIndex];
                                const double h = 0.1;
                                if (r == 1)
                                    p.inDx = -h, p.outDx = h, p.inDy = p.outDy = 0.0;
                                else if (r == 2)
                                    p.inDx = p.outDx = p.inDy = p.outDy = 0.0;
                                else
                                {
                                    c.erase (c.begin() + pointIndex);
                                    safe->focusPoint = -1;
                                }
                                o.commit();
                            });
    }

    SketchEditor& owner;
    Drag drag = Drag::None;
    int dragNote = -1;
    double grabOffset = 0.0;
    double lastLength = 1.0;
};

//==============================================================================
class SketchEditor::DragTile : public juce::Component
{
public:
    explicit DragTile (SketchEditor& o) : owner (o) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (isMouseOver() ? palette::saffron.withAlpha (0.3f) : palette::saffron.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (palette::saffron);
        g.drawRoundedRectangle (r, 5.0f, 1.5f);
        g.setColour (palette::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (status.isNotEmpty() ? status : juce::String ("Drag WAV into your DAW"), getLocalBounds().reduced (6, 0),
                    juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override { started = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 4)
            return;
        started = true;
        juce::String error;
        if (! owner.renderIfNeeded (error))
        {
            setStatus (error);
            return;
        }
        setStatus ("Dragging " + owner.lastRender.getFileName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ owner.lastRender.getFullPathName() }, false,
                                                                     this, [this] { setStatus ({}); });
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() >= 4)
            return;
        juce::String error;
        if (owner.renderIfNeeded (error))
            setStatus ("Rendered: " + owner.lastRender.getFileName() + " - drag me");
        else
            setStatus (error);
    }

    void setStatus (const juce::String& s)
    {
        status = s;
        repaint();
    }

private:
    SketchEditor& owner;
    juce::String status;
    bool started = false;
};

//==============================================================================
SketchEditor::SketchEditor (GamakaProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    sketch = proc.getSketch();

    roll = std::make_unique<Roll> (*this);
    dragTile = std::make_unique<DragTile> (*this);
    dragTile->setComponentID ("renderTile");
    addAndMakeVisible (*roll);
    addAndMakeVisible (*dragTile);

    for (int b : barCounts)
        lengthBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    lengthBox.setSelectedId (juce::jmax (1, (int) std::lround (sketch.lengthBeats / 4.0)), juce::dontSendNotification);
    if (lengthBox.getSelectedId() == 0)
        lengthBox.setSelectedId (4, juce::dontSendNotification);
    lengthBox.onChange = [this]
    {
        pushUndo();
        sketch.lengthBeats = 4.0 * lengthBox.getSelectedId();
        sketch.notes.erase (std::remove_if (sketch.notes.begin(), sketch.notes.end(),
                                            [this] (const SketchNote& n) { return n.start >= sketch.lengthBeats; }),
                            sketch.notes.end());
        for (auto& n : sketch.notes)
            n.length = juce::jmin (n.length, sketch.lengthBeats - n.start);
        selection.clear();
        commit();
    };

    const juce::StringArray grids { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    gridBox.addItemList (grids, 1);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this] { roll->repaint(); };

    for (int m = 0; m < (int) GamakaMode::Count; ++m)
        stampBox.addItem (gamakaName ((GamakaMode) m), m + 1);
    stampBox.setSelectedId (2, juce::dontSendNotification);
    stampButton.onClick = [this] { applyStamp ((GamakaMode) (stampBox.getSelectedId() - 1)); };
    stampButton.setTooltip ("Stamp the gamaka onto the selected notes (or press 1-9)");

    ragaLock.setToggleState (true, juce::dontSendNotification);
    ragaLock.setTooltip ("Snap notes and curve points to the raga's swaras (hold Alt to place freely)");
    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "followHost", followHost);
    followHost.setTooltip ("Play the sketch in sync with the DAW transport (looping over its length)");

    previewButton.setClickingTogglesState (false);
    previewButton.onClick = [this] { proc.setPreviewing (! proc.isPreviewing()); };
    saveButton.onClick = [this] { renderToChosenFile(); };
    undoButton.onClick = [this] { undo(); };
    clearButton.onClick = [this]
    {
        pushUndo();
        sketch.notes.clear();
        selection.clear();
        commit();
    };

    hint.setText ("Del: delete   Ctrl+D: duplicate   Ctrl+Z: undo   1-9: gamaka   Space: preview   Alt: no snap",
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, palette::dimText);
    hint.setFont (juce::FontOptions (12.0f));

    for (juce::Component* c : std::initializer_list<juce::Component*> { &lengthBox, &gridBox, &stampBox, &ragaLock,
                                                                          &followHost, &stampButton, &previewButton,
                                                                          &saveButton, &clearButton, &undoButton, &hint })
        addAndMakeVisible (c);

    startTimerHz (30);
}

SketchEditor::~SketchEditor() { proc.setPreviewing (false); }

void SketchEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto bar = r.removeFromTop (30);
    auto place = [&bar] (juce::Component& c, int w)
    {
        c.setBounds (bar.removeFromLeft (w).reduced (2, 0));
        bar.removeFromLeft (4);
    };
    place (lengthBox, 86);
    place (gridBox, 70);
    place (ragaLock, 96);
    place (stampBox, 120);
    place (stampButton, 60);
    place (undoButton, 56);
    place (clearButton, 56);
    place (followHost, 120);
    place (previewButton, 76);
    place (saveButton, 96);
    dragTile->setBounds (bar.reduced (2, 0));

    r.removeFromTop (6);
    hint.setBounds (r.removeFromBottom (20));
    roll->setBounds (r);
}

void SketchEditor::paint (juce::Graphics& g) { g.fillAll (palette::background); }

void SketchEditor::timerCallback()
{
    const int raga = (int) proc.apvts.getRawParameterValue ("raga")->load();
    const int tuning = (int) proc.apvts.getRawParameterValue ("tuning")->load();
    if (raga != lastRaga || tuning != lastTuning)
    {
        lastRaga = raga;
        lastTuning = tuning;
        scale.set (raga, (Tuning) tuning);
        roll->repaint();
    }
    const bool previewing = proc.isPreviewing();
    previewButton.setToggleState (previewing, juce::dontSendNotification);
    previewButton.setButtonText (previewing ? "Stop" : "Preview");
    if (proc.sketchPlayhead.load() >= 0.0 || previewing)
        roll->repaint();
}

double SketchEditor::gridBeats() const { return gridValues[juce::jlimit (0, 5, gridBox.getSelectedId() - 1)]; }

void SketchEditor::commit()
{
    proc.setSketch (sketch);
    dirtySinceRender = true;
    roll->repaint();
}

void SketchEditor::pushUndo()
{
    undoStack.push_back (sketch);
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
}

void SketchEditor::undo()
{
    if (undoStack.empty())
        return;
    sketch = undoStack.back();
    undoStack.pop_back();
    selection.clear();
    roll->focusPoint = -1;
    lengthBox.setSelectedId ((int) std::lround (sketch.lengthBeats / 4.0), juce::dontSendNotification);
    commit();
}

void SketchEditor::applyStamp (GamakaMode mode)
{
    if (selection.empty())
        return;
    pushUndo();
    const double rate = proc.apvts.getRawParameterValue ("kampitaRate")->load();
    const double depth = proc.apvts.getRawParameterValue ("kampitaDepth")->load();
    for (int i : selection)
        if (i < (int) sketch.notes.size())
            stampGamaka (sketch.notes[(size_t) i], mode, scale, proc.tempoForSketch(), rate, depth);
    roll->focusPoint = -1;
    commit();
}

bool SketchEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        roll->deleteSelection();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'D')
    {
        roll->duplicateSelection();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        proc.setPreviewing (! proc.isPreviewing());
        return true;
    }
    const auto c = key.getTextCharacter();
    if (c >= '1' && c <= '9')
    {
        applyStamp ((GamakaMode) (c - '1'));
        return true;
    }
    return false;
}

bool SketchEditor::renderIfNeeded (juce::String& error)
{
    if (! dirtySinceRender && lastRender.existsAsFile())
        return true;

    auto ragaWord = juce::String (ragaDisplayName (lastRaga)).fromFirstOccurrenceOf (" ", false, false);
    if (lastRaga >= numMelakartas())
        ragaWord = ragaDisplayName (lastRaga);
    ragaWord = ragaWord.upToFirstOccurrenceOf (" ", false, false).retainCharacters (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    const auto name = "Gamaka_" + ragaWord + "_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".wav";
    const auto file = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("Gamaka Renders")
                          .getChildFile (name);

    juce::MouseCursor::showWaitCursor();
    const bool ok = proc.renderSketchToFile (file, error);
    juce::MouseCursor::hideWaitCursor();
    if (ok)
    {
        lastRender = file;
        dirtySinceRender = false;
    }
    return ok;
}

void SketchEditor::renderToChosenFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Save the rendered gamaka",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Gamaka Renders"), "*.wav");
    juce::Component::SafePointer<SketchEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              juce::String error;
                              const auto file = fc.getResult().withFileExtension ("wav");
                              if (! safe->proc.renderSketchToFile (file, error))
                                  safe->dragTile->setStatus (error);
                              else
                                  safe->dragTile->setStatus ("Saved " + file.getFileName());
                          });
}

#pragma once

#include <array>
#include <string>
#include <vector>

namespace gamaka
{

// The 16 Carnatic swara-sthana names. R2/G1, R3/G2, D2/N1 and D3/N2 share a pitch.
enum class Swara
{
    S, R1, R2, R3, G1, G2, G3, M1, M2, P, D1, D2, D3, N1, N2, N3
};

int swaraSemitone (Swara s);      // 0..11
int swaraLetter (Swara s);        // 0=S 1=R 2=G 3=M 4=P 5=D 6=N
const char* swaraLabel (Swara s); // "S", "R2", ...

enum class Tuning
{
    Equal = 0,
    Just
};

struct RagaDef
{
    std::string name;
    // Listed in arohana order. When a raga uses two variants of the same swara
    // (e.g. Bhairavi uses D2 and D1) the first one listed is the one played by
    // the white key in Swara key mode.
    std::vector<Swara> swaras;
};

// 72 melakartas followed by a selection of popular janya ragas.
const std::vector<RagaDef>& allRagas();
int numMelakartas(); // the first N entries of allRagas() are melakartas

// Display name, e.g. "29 Dheerasankarabharanam" or "Mohanam (janya of 28)".
std::string ragaDisplayName (int index);

// Tuned pitch of a chromatic semitone index relative to Sa, in (fractional)
// semitones. Equal temperament returns k unchanged.
double tunedSemitone (int k, Tuning tuning);

// A raga's note set laid out across octaves. All pitches are in fractional
// semitones relative to the middle-octave Sa.
class RagaScale
{
public:
    RagaScale();

    void set (int ragaIndex, Tuning tuning);

    int ragaIndex() const { return ragaIdx; }
    int notesPerOctave() const { return (int) pos.size(); }

    double degreeToPitch (int degree) const;
    int nearestDegree (double pitch) const;
    double snap (double pitch) const { return degreeToPitch (nearestDegree (pitch)); }

    // Continuous mapping between pitch and "raga step" space, piecewise linear
    // between raga notes. Used for raga-aware pitch bend.
    double pitchToDegreeF (double pitch) const;
    double degreeFToPitch (double degreeF) const;

    // Pitch of the raga note `steps` away (signed) from the raga note nearest to `pitch`.
    double neighbour (double pitch, int steps) const;

    // White-key (Swara mode) mapping: letter 0..6 = S R G M P D N.
    // Swaras the raga omits snap to the nearest raga note.
    double letterPitch (int letter, int octave) const;

    double chromaticPitch (int semitone) const { return tunedSemitone (semitone, tuning); }

    // Label for a degree, with octave marks: ".P" (mandra), "G3", "S'" (tara).
    std::string degreeLabel (int degree) const;

private:
    int ragaIdx = 0;
    Tuning tuning = Tuning::Equal;
    std::vector<double> pos;           // sorted, in [0, 12)
    std::vector<std::string> labels;   // parallel to pos
    std::array<double, 7> letterPos {}; // pitch for each white-key letter
};

} // namespace gamaka

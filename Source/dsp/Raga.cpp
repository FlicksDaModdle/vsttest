#include "Raga.h"

#include <algorithm>
#include <cmath>

namespace gamaka
{

int swaraSemitone (Swara s)
{
    static constexpr int table[] = { 0, 1, 2, 3, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 10, 11 };
    return table[(int) s];
}

int swaraLetter (Swara s)
{
    static constexpr int table[] = { 0, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 5, 5, 6, 6, 6 };
    return table[(int) s];
}

const char* swaraLabel (Swara s)
{
    static const char* table[] = { "S", "R1", "R2", "R3", "G1", "G2", "G3", "M1", "M2",
                                   "P", "D1", "D2", "D3", "N1", "N2", "N3" };
    return table[(int) s];
}

double tunedSemitone (int k, Tuning tuning)
{
    if (tuning == Tuning::Equal)
        return (double) k;

    // 5-limit ratios commonly used for the 12 Carnatic svarasthanas.
    static const double ratios[12] = { 1.0,       16.0 / 15.0, 9.0 / 8.0, 6.0 / 5.0,
                                       5.0 / 4.0, 4.0 / 3.0,   45.0 / 32.0, 3.0 / 2.0,
                                       8.0 / 5.0, 5.0 / 3.0,   16.0 / 9.0, 15.0 / 8.0 };
    const int octave = (k >= 0) ? k / 12 : -((11 - k) / 12);
    const int pc = k - octave * 12;
    return 12.0 * octave + 12.0 * std::log2 (ratios[pc]);
}

namespace
{
using S = Swara;

const char* melakartaNames[72] = {
    "Kanakangi", "Ratnangi", "Ganamurthi", "Vanaspati", "Manavati", "Tanarupi",
    "Senavati", "Hanumatodi (Todi)", "Dhenuka", "Natakapriya", "Kokilapriya", "Rupavati",
    "Gayakapriya", "Vakulabharanam", "Mayamalavagowla", "Chakravakam", "Suryakantam", "Hatakambari",
    "Jhankaradhwani", "Natabhairavi", "Keeravani", "Kharaharapriya", "Gourimanohari", "Varunapriya",
    "Mararanjani", "Charukesi", "Sarasangi", "Harikambhoji", "Dheerasankarabharanam", "Naganandini",
    "Yagapriya", "Ragavardhini", "Gangeyabhushani", "Vagadheeswari", "Shulini", "Chalanata",
    "Salagam", "Jalarnavam", "Jhalavarali", "Navaneetam", "Pavani", "Raghupriya",
    "Gavambodhi", "Bhavapriya", "Shubhapantuvarali", "Shadvidamargini", "Suvarnangi", "Divyamani",
    "Dhavalambari", "Namanarayani", "Kamavardhini (Pantuvarali)", "Ramapriya", "Gamanashrama", "Vishwambari",
    "Shyamalangi", "Shanmukhapriya", "Simhendramadhyamam", "Hemavati", "Dharmavati", "Neetimati",
    "Kantamani", "Rishabhapriya", "Latangi", "Vachaspati", "Mechakalyani (Kalyani)", "Chitrambari",
    "Sucharitra", "Jyotiswarupini", "Dhatuvardhini", "Nasikabhushani", "Kosalam", "Rasikapriya"
};

// Melakarta n (1..72) is fully determined by its number via the katapayadi scheme.
RagaDef makeMelakarta (int n)
{
    static const S riGa[6][2] = { { S::R1, S::G1 }, { S::R1, S::G2 }, { S::R1, S::G3 },
                                  { S::R2, S::G2 }, { S::R2, S::G3 }, { S::R3, S::G3 } };
    static const S daNi[6][2] = { { S::D1, S::N1 }, { S::D1, S::N2 }, { S::D1, S::N3 },
                                  { S::D2, S::N2 }, { S::D2, S::N3 }, { S::D3, S::N3 } };
    const int m = (n - 1) % 36;
    const S ma = n <= 36 ? S::M1 : S::M2;
    RagaDef r;
    r.name = std::to_string (n) + " " + melakartaNames[n - 1];
    r.swaras = { S::S, riGa[m / 6][0], riGa[m / 6][1], ma, S::P, daNi[m % 6][0], daNi[m % 6][1] };
    return r;
}

struct Janya
{
    const char* name;
    int parent;
    std::vector<Swara> swaras;
};

const std::vector<Janya>& janyas()
{
    static const std::vector<Janya> list = {
        { "Mohanam", 28, { S::S, S::R2, S::G3, S::P, S::D2 } },
        { "Hamsadhwani", 29, { S::S, S::R2, S::G3, S::P, S::N3 } },
        { "Hindolam", 20, { S::S, S::G2, S::M1, S::D1, S::N2 } },
        { "Abhogi", 22, { S::S, S::R2, S::G2, S::M1, S::D2 } },
        { "Madhyamavati", 22, { S::S, S::R2, S::M1, S::P, S::N2 } },
        { "Shuddha Saveri", 29, { S::S, S::R2, S::M1, S::P, S::D2 } },
        { "Shuddha Dhanyasi", 22, { S::S, S::G2, S::M1, S::P, S::N2 } },
        { "Revati", 2, { S::S, S::R1, S::M1, S::P, S::N2 } },
        { "Amritavarshini", 66, { S::S, S::G3, S::M2, S::P, S::N3 } },
        { "Gambhira Nata", 36, { S::S, S::G3, S::M1, S::P, S::N3 } },
        { "Valaji", 28, { S::S, S::G3, S::P, S::D2, S::N2 } },
        { "Bowli", 15, { S::S, S::R1, S::G3, S::P, S::D1 } },
        { "Malahari", 15, { S::S, S::R1, S::G3, S::M1, S::P, S::D1 } },
        { "Sriranjani", 22, { S::S, S::R2, S::G2, S::M1, S::D2, S::N2 } },
        { "Hamsanadam", 60, { S::S, S::R2, S::M2, S::P, S::D3, S::N3 } },
        { "Saveri", 15, { S::S, S::R1, S::G3, S::M1, S::P, S::D1, S::N3 } },
        { "Vasantha", 17, { S::S, S::R1, S::G3, S::M1, S::D1, S::N3 } },
        // Ragas with anya swaras: the first variant of a letter is the white-key one.
        { "Bhairavi", 20, { S::S, S::R2, S::G2, S::M1, S::P, S::D2, S::N2, S::D1 } },
        { "Kambhoji", 28, { S::S, S::R2, S::G3, S::M1, S::P, S::D2, S::N2, S::N3 } },
        { "Begada", 29, { S::S, S::R2, S::G3, S::M1, S::P, S::D2, S::N3, S::N2 } },
    };
    return list;
}
} // namespace

const std::vector<RagaDef>& allRagas()
{
    static const std::vector<RagaDef> ragas = []
    {
        std::vector<RagaDef> r;
        for (int n = 1; n <= 72; ++n)
            r.push_back (makeMelakarta (n));
        for (auto& j : janyas())
            r.push_back ({ std::string (j.name) + " (janya of " + std::to_string (j.parent) + ")", j.swaras });
        return r;
    }();
    return ragas;
}

int numMelakartas() { return 72; }

std::string ragaDisplayName (int index)
{
    const auto& r = allRagas();
    return (index >= 0 && index < (int) r.size()) ? r[(size_t) index].name : std::string ("?");
}

//==============================================================================
namespace
{
int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((b - 1 - a) / b); }
} // namespace

RagaScale::RagaScale() { set (28, Tuning::Equal); }

void RagaScale::set (int ragaIndex, Tuning newTuning)
{
    const auto& ragas = allRagas();
    ragaIdx = std::clamp (ragaIndex, 0, (int) ragas.size() - 1);
    tuning = newTuning;
    const auto& def = ragas[(size_t) ragaIdx];

    struct Entry { double p; std::string label; };
    std::vector<Entry> entries;
    std::array<bool, 7> letterSet {};

    for (auto s : def.swaras)
    {
        const double p = tunedSemitone (swaraSemitone (s), tuning);
        const bool duplicate = std::any_of (entries.begin(), entries.end(),
                                            [p] (const Entry& e) { return std::abs (e.p - p) < 1e-9; });
        if (! duplicate)
            entries.push_back ({ p, swaraLabel (s) });

        const int letter = swaraLetter (s);
        if (! letterSet[(size_t) letter])
        {
            letterSet[(size_t) letter] = true;
            letterPos[(size_t) letter] = p;
        }
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.p < b.p; });
    pos.clear();
    labels.clear();
    for (auto& e : entries)
    {
        pos.push_back (e.p);
        labels.push_back (e.label);
    }

    // Omitted swaras: snap the would-be equal-tempered position of the letter.
    static constexpr int defaultSemitone[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int l = 0; l < 7; ++l)
        if (! letterSet[(size_t) l])
            letterPos[(size_t) l] = snap ((double) defaultSemitone[l]);
}

double RagaScale::degreeToPitch (int degree) const
{
    const int n = (int) pos.size();
    const int octave = floorDiv (degree, n);
    return pos[(size_t) (degree - octave * n)] + 12.0 * octave;
}

double RagaScale::pitchToDegreeF (double pitch) const
{
    const int n = (int) pos.size();
    const double octaveF = std::floor (pitch / 12.0);
    const double within = pitch - 12.0 * octaveF; // [0, 12)
    const int octave = (int) octaveF;

    // Find segment [pos[i], pos[i+1]) containing `within` (pos[n] == 12 + pos[0]).
    int i = n - 1;
    for (int k = 0; k < n; ++k)
        if (within < pos[(size_t) k])
        {
            i = k - 1;
            break;
        }

    double lo, hi;
    if (i < 0) // below pos[0] (only possible if pos[0] > 0, i.e. never with Sa present)
    {
        lo = pos[(size_t) n - 1] - 12.0;
        hi = pos[0];
        return octave * n - 1 + (within - lo) / (hi - lo);
    }
    lo = pos[(size_t) i];
    hi = (i + 1 < n) ? pos[(size_t) i + 1] : pos[0] + 12.0;
    return octave * n + i + (within - lo) / (hi - lo);
}

double RagaScale::degreeFToPitch (double degreeF) const
{
    const double d0 = std::floor (degreeF);
    const double frac = degreeF - d0;
    const double p0 = degreeToPitch ((int) d0);
    const double p1 = degreeToPitch ((int) d0 + 1);
    return p0 + (p1 - p0) * frac;
}

int RagaScale::nearestDegree (double pitch) const
{
    const int d0 = (int) std::floor (pitchToDegreeF (pitch));
    const double e0 = std::abs (degreeToPitch (d0) - pitch);
    const double e1 = std::abs (degreeToPitch (d0 + 1) - pitch);
    return e1 < e0 ? d0 + 1 : d0;
}

double RagaScale::neighbour (double pitch, int steps) const
{
    return degreeToPitch (nearestDegree (pitch) + steps);
}

double RagaScale::letterPitch (int letter, int octave) const
{
    return letterPos[(size_t) std::clamp (letter, 0, 6)] + 12.0 * octave;
}

std::string RagaScale::degreeLabel (int degree) const
{
    const int n = (int) pos.size();
    const int octave = floorDiv (degree, n);
    std::string label = labels[(size_t) (degree - octave * n)];
    for (int o = octave; o < 0; ++o)
        label = "." + label;
    for (int o = 0; o < octave; ++o)
        label += "'";
    return label;
}

} // namespace gamaka

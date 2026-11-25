// CQTHarmonicSuppressor on hand-built spectra, against the rule in HARMONIC-SUPPRESSOR-PLAN.md.
//
// The axis is the constantQ pipeline's: 36 bins an octave, here from C2, so C3 is bin 36 and
// harmonic n of a note lies 36·log2(n) bins above it. Every hill is a parabola three bins wide
// over a flat floor, so parabolic interpolation finds its top exactly where the test puts it,
// and a hill whose top is on a bin reads its own height there.
#include "../src/Analysis/AnalysisPipeline.h"
#include "../src/Analysis/AudioFrame.h"
#include "../src/Analysis/FrequencyAxis.h"
#include "../src/Analysis/Stages/CQTHarmonicSuppressor.h"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <set>
#include <vector>

namespace
{
	constexpr int BINS_PER_OCTAVE = 36;
	// C2 to C8.
	constexpr int BIN_COUNT = 6 * BINS_PER_OCTAVE + 1;
	constexpr int C3_BIN = BINS_PER_OCTAVE;
	constexpr int C4_BIN = 2 * BINS_PER_OCTAVE;
	constexpr int C5_BIN = 3 * BINS_PER_OCTAVE;
	constexpr float FLOOR = 0.001f;
	// How fast a hill falls from its top: a bin out it stands at 0.6 of its height when its top is on a bin.
	constexpr float CURVE = 0.4f;
	// doctest::Approx's margin is this times (1 + the larger value), so it is near enough absolute here.
	constexpr double TOLERANCE = 1e-5;

	float C2Hz()
	{
		return 440.0f * std::pow(2.0f, (36.0f - 69.0f) / 12.0f);
	}

	// Where harmonic n of a note lies, as a fractional bin.
	float HarmonicBin(float rootBin, int n)
	{
		return rootBin + (float)BINS_PER_OCTAVE * std::log2((float)n);
	}

	// The bin nearest harmonic n of a note: where a hill goes when its top is to sit on a bin.
	int NearestBin(int rootBin, int n)
	{
		return (int)std::round(HarmonicBin((float)rootBin, n));
	}

	kfr::univector<float> Floor()
	{
		kfr::univector<float> spectrum(BIN_COUNT);
		std::fill(spectrum.begin(), spectrum.end(), FLOOR);
		return spectrum;
	}

	// A hill whose top, as parabolic interpolation finds it, is at the given fractional bin and the given height above the floor.
	void AddHill(kfr::univector<float>& spectrum, float top, float height)
	{
		const int centre = (int)std::round(top);
		const float offset = top - (float)centre;
		for (int x = -1; x <= 1; x++)
		{
			const float distance = (float)x - offset;
			spectrum[centre + x] = FLOOR + height * (1.0f - CURVE * distance * distance);
		}
	}

	// Partials 1 to last of a note, at 1/k of its fundamental's height, each on the bin nearest
	// its place: the shape the plan's worked examples use.
	void AddSeries(kfr::univector<float>& spectrum, int rootBin, int last)
	{
		for (int k = 1; k <= last; k++)
			AddHill(spectrum, (float)NearestBin(rootBin, k), 1.0f / (float)k);
	}

	kfr::univector<float> Suppress(const kfr::univector<float>& spectrum, const nlohmann::json& settings = nlohmann::json::object())
	{
		Adagio::AudioFrame frame;
		frame.SampleRate = 8000;
		Adagio::PersistentData persistent;
		Adagio::AnalysisContext context{ frame, &persistent };
		context.FreqAxis = std::make_unique<Adagio::LogFrequencyAxis>(C2Hz(), BINS_PER_OCTAVE);
		context.Magnitudes = spectrum;

		Adagio::CQTHarmonicSuppressor stage;
		stage.ApplySettings(settings);
		stage.Execute(&context);

		// The stage shapes the spectrum and nothing else: the bin count and the axis stay as they came.
		REQUIRE(context.Magnitudes.size() == spectrum.size());
		REQUIRE(context.FreqAxis != nullptr);
		CHECK(context.FreqAxis->IsLog());
		CHECK(context.FreqAxis->MinFreq() == C2Hz());
		CHECK(context.FreqAxis->BinsPerOctave() == BINS_PER_OCTAVE);
		return context.Magnitudes;
	}

	// The bins PeakExtractor takes for local maxima: above the bin below, and at least level with the bin above.
	std::set<int> LocalMaxima(const kfr::univector<float>& spectrum)
	{
		std::set<int> maxima;
		for (int i = 1; i + 1 < (int)spectrum.size(); i++)
		{
			if (spectrum[i] > spectrum[i - 1] && spectrum[i] >= spectrum[i + 1])
				maxima.insert(i);
		}
		return maxima;
	}

	// A fully explained harmonic becomes the line between its valleys, here the floor.
	void CheckFlattened(const kfr::univector<float>& after, int top)
	{
		CAPTURE(top);
		for (int bin = top - 1; bin <= top + 1; bin++)
		{
			CAPTURE(bin);
			CHECK(after[bin] == doctest::Approx(FLOOR).epsilon(TOLERANCE));
		}
	}

	void CheckUntouched(const kfr::univector<float>& before, const kfr::univector<float>& after, int top)
	{
		CAPTURE(top);
		for (int bin = top - 1; bin <= top + 1; bin++)
		{
			CAPTURE(bin);
			CHECK(after[bin] == before[bin]);
		}
	}

	// Each bin of the hill around top keeps this share of its height above the floor.
	void CheckHeightShare(const kfr::univector<float>& before, const kfr::univector<float>& after, int top, float share)
	{
		CAPTURE(top);
		for (int bin = top - 1; bin <= top + 1; bin++)
		{
			CAPTURE(bin);
			CHECK(after[bin] - FLOOR == doctest::Approx(share * (before[bin] - FLOOR)).epsilon(TOLERANCE));
		}
	}

	// The stage only ever lowers a bin.
	void CheckNothingRose(const kfr::univector<float>& before, const kfr::univector<float>& after)
	{
		size_t risen = 0;
		for (size_t i = 0; i < before.size(); i++)
		{
			if (after[i] > before[i])
				++risen;
		}
		CHECK(risen == 0);
	}

	// No peak appears that PeakExtractor could pick and the spectrum didn't already have.
	void CheckNoNewMaxima(const kfr::univector<float>& before, const kfr::univector<float>& after)
	{
		const std::set<int> was = LocalMaxima(before);
		for (int bin : LocalMaxima(after))
		{
			CAPTURE(bin);
			CHECK(was.count(bin) == 1);
		}
	}

	size_t CountChanged(const kfr::univector<float>& before, const kfr::univector<float>& after)
	{
		size_t changed = 0;
		for (size_t i = 0; i < before.size(); i++)
		{
			if (after[i] != before[i])
				++changed;
		}
		return changed;
	}
}

TEST_CASE("CQTHarmonicSuppressor: a 1/k series keeps only its fundamental's hill")
{
	// Partials 1 to 9 of C3. HARMONICS 7 explains h2 to h8, each at or under the geometric mean
	// of its neighbours and so well inside KEEP_DB's 6 dB, so each becomes the floor between its
	// valleys. h9 lies beyond HARMONICS; it is there to give h8 its upper neighbour.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);
	const kfr::univector<float> after = Suppress(before);

	CheckUntouched(before, after, C3_BIN);
	for (int n = 2; n <= 8; n++)
	{
		CAPTURE(n);
		CheckFlattened(after, NearestBin(C3_BIN, n));
	}
	CHECK(LocalMaxima(after) == std::set<int>{ C3_BIN, NearestBin(C3_BIN, 9) });
}

TEST_CASE("CQTHarmonicSuppressor: a harmonic keeps only what rises above its series' credit")
{
	// C3's envelope at G4 is the geometric mean of C4 and C5, and KEEP_DB's 6 dB credits the
	// series with twice that. Whatever rises further stays, taken out in power: sqrt(L² − credit²).
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);
	const int g4 = NearestBin(C3_BIN, 3);
	const float envelope = std::sqrt(before[C4_BIN] * before[C5_BIN]);
	const float credit = envelope * std::pow(10.0f, 6.0f / 20.0f);

	SUBCASE("1 dB under the credit is flattened")
	{
		AddHill(before, (float)g4, credit * std::pow(10.0f, -1.0f / 20.0f) - FLOOR);
		CheckFlattened(Suppress(before), g4);
	}
	SUBCASE("1 dB over the credit keeps the rest, 6.9 dB under its level")
	{
		const float level = credit * std::pow(10.0f, 1.0f / 20.0f);
		AddHill(before, (float)g4, level - FLOOR);
		const kfr::univector<float> after = Suppress(before);
		CHECK(after[g4] == doctest::Approx(std::sqrt(level * level - credit * credit)).epsilon(TOLERANCE));
		CHECK(20.0f * std::log10(after[g4] / level) == doctest::Approx(-6.87).epsilon(0.005));
	}
	SUBCASE("18 dB over the credit loses less than 0.3 dB")
	{
		const float level = credit * std::pow(10.0f, 18.0f / 20.0f);
		AddHill(before, (float)g4, level - FLOOR);
		const kfr::univector<float> after = Suppress(before);
		CHECK(after[g4] == doctest::Approx(std::sqrt(level * level - credit * credit)).epsilon(TOLERANCE));
		CHECK(20.0f * std::log10(after[g4] / level) > -0.3f);
	}
}

TEST_CASE("CQTHarmonicSuppressor: with SUPPRESS_OCTAVES off the octaves stay and h3 still goes")
{
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);
	const kfr::univector<float> after = Suppress(before, { { "SUPPRESS_OCTAVES", "No" } });

	for (int n : { 2, 4, 8 })
	{
		CAPTURE(n);
		CheckUntouched(before, after, NearestBin(C3_BIN, n));
	}
	CheckFlattened(after, NearestBin(C3_BIN, 3));
}

TEST_CASE("CQTHarmonicSuppressor: a flattened harmonic explains nothing")
{
	// D6 is G4's 3rd harmonic and C3's 9th, beyond HARMONICS 7. C3 flattens G4, its 3rd, and a
	// flattened hill is no longer a local maximum, so it is never a root and D6 keeps its level.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 8);
	const int d6 = NearestBin(C3_BIN, 9);
	AddHill(before, (float)d6, 0.2f);
	const kfr::univector<float> after = Suppress(before);

	CheckFlattened(after, NearestBin(C3_BIN, 3));
	CheckUntouched(before, after, d6);
}

TEST_CASE("CQTHarmonicSuppressor: only a peak over ROOT_DB explains others, even a stray one")
{
	// C4, G4 and C5 at one level are three real notes. Under ROOT_DB a bump at C3 explains none of
	// them. Over it, C3 is a root and G4, its 3rd harmonic, sits under the envelope C4 and C5 make.
	// One frame can't tell that voicing from a single note, so G4 goes: the plan's close-voicing
	// limit, pinned so a fix shows up.
	kfr::univector<float> before = Floor();
	const int g4 = NearestBin(C3_BIN, 3);
	for (int top : { C4_BIN, g4, C5_BIN })
		AddHill(before, (float)top, 0.3f);
	const float gate = FLOOR * std::pow(10.0f, 12.0f / 20.0f);

	SUBCASE("a bump under ROOT_DB")
	{
		AddHill(before, (float)C3_BIN, 0.5f * (gate - FLOOR));
		const kfr::univector<float> after = Suppress(before);
		CheckUntouched(before, after, C4_BIN);
		CheckUntouched(before, after, g4);
	}
	SUBCASE("a bump over ROOT_DB")
	{
		AddHill(before, (float)C3_BIN, 2.0f * gate);
		CheckFlattened(Suppress(before), g4);
	}
}

TEST_CASE("CQTHarmonicSuppressor: a partly lowered hill keeps its shape, its top and its valleys")
{
	// G4 as a five-bin hill as loud as C3, over the credit C3's series has there, so it is only
	// partly lowered: every bin of it scales towards the floor by the same factor.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);
	const int g4 = NearestBin(C3_BIN, 3);
	const float shape[5] = { 0.35f, 0.8f, 1.0f, 0.8f, 0.35f };
	for (int x = -2; x <= 2; x++)
		before[g4 + x] = FLOOR + shape[x + 2];
	const kfr::univector<float> after = Suppress(before);

	const float factor = (after[g4] - FLOOR) / (before[g4] - FLOOR);
	CHECK(factor > 0.0f);
	CHECK(factor < 1.0f);
	for (int x = -2; x <= 2; x++)
	{
		CAPTURE(x);
		CHECK((after[g4 + x] - FLOOR) / (before[g4 + x] - FLOOR) == doctest::Approx(factor).epsilon(TOLERANCE));
	}
	CHECK(LocalMaxima(after).count(g4) == 1);
	CHECK(after[g4 - 3] == before[g4 - 3]);
	CHECK(after[g4 + 3] == before[g4 + 3]);
	CheckNothingRose(before, after);
	CheckNoNewMaxima(before, after);
}

TEST_CASE("CQTHarmonicSuppressor: lowering a hill leaves no peak a semitone from it")
{
	// G4's hill falls for seven bins, over two semitones, on its left before it reaches a valley,
	// and drops to the floor at once on its right. A walk stopped a semitone out would end at 0.2
	// on a slope still falling, above the right valley, and lowering the hill would make that end
	// a peak.
	kfr::univector<float> before = Floor();
	AddHill(before, (float)C3_BIN, 1.0f);
	AddHill(before, (float)C4_BIN, 0.5f);
	AddHill(before, (float)C5_BIN, 0.25f);
	const int g4 = NearestBin(C3_BIN, 3);
	const float left[8] = { 0.135f, 0.13f, 0.14f, 0.15f, 0.17f, 0.2f, 0.25f, 0.28f };
	for (int i = 0; i < 8; i++)
		before[g4 - 8 + i] = left[i];
	before[g4] = 0.3f;
	before[g4 + 1] = 0.1f;
	const kfr::univector<float> after = Suppress(before);

	CHECK(after[g4] < before[g4]);
	CheckNothingRose(before, after);
	CheckNoNewMaxima(before, after);
}

TEST_CASE("CQTHarmonicSuppressor: a flat run beside a lowered hill never becomes a peak")
{
	// G4's right side falls onto a run of five equal bins before the next hill rises, and its left
	// valley is lower than the run. The walk crosses the run, so the run's last bin is the valley
	// and the run becomes a ramp up to it. A walk stopped at the run's first bin would leave that
	// bin above the lowered hill and level with the next, which PeakExtractor counts as a peak.
	kfr::univector<float> before = Floor();
	AddHill(before, (float)C3_BIN, 1.0f);
	AddHill(before, (float)C4_BIN, 0.5f);
	AddHill(before, (float)C5_BIN, 0.25f);
	const int g4 = NearestBin(C3_BIN, 3);
	before[g4 - 2] = 0.15f;
	before[g4 - 1] = 0.25f;
	before[g4] = 0.3f;
	before[g4 + 1] = 0.2f;
	for (int bin = g4 + 2; bin <= g4 + 6; bin++)
		before[bin] = 0.12f;
	AddHill(before, (float)(g4 + 8), 0.3f);
	const kfr::univector<float> after = Suppress(before);

	CHECK(after[g4] < before[g4]);
	const std::set<int> maxima = LocalMaxima(after);
	for (int bin = g4 + 2; bin <= g4 + 6; bin++)
	{
		CAPTURE(bin);
		CHECK(maxima.count(bin) == 0);
	}
	CheckNothingRose(before, after);
	CheckNoNewMaxima(before, after);
}

TEST_CASE("CQTHarmonicSuppressor: HARMONICS 0 or STRENGTH 0 leaves the spectrum bit-identical")
{
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);

	SUBCASE("HARMONICS 0")
	{
		CHECK(CountChanged(before, Suppress(before, { { "HARMONICS", 0 } })) == 0);
	}
	SUBCASE("STRENGTH 0")
	{
		CHECK(CountChanged(before, Suppress(before, { { "STRENGTH", 0.0f } })) == 0);
	}
}

TEST_CASE("CQTHarmonicSuppressor: STRENGTH takes that share of one series' harmonics")
{
	// Partials 1 to 6 of C3. HARMONICS 4 with the octaves left alone explains h3 and h5 only, and
	// ROOT_DB 57 lets only the fundamental be a root, so no partial explains another. h3 and h5
	// are each fully explained, so each keeps 1 − STRENGTH of its height above the floor.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 6);
	const int h3 = NearestBin(C3_BIN, 3);
	const int h5 = NearestBin(C3_BIN, 5);

	for (float strength : { 0.25f, 0.5f })
	{
		CAPTURE(strength);
		const kfr::univector<float> after = Suppress(before, {
			{ "HARMONICS", 4 }, { "SUPPRESS_OCTAVES", "No" }, { "ROOT_DB", 57.0f }, { "STRENGTH", strength } });

		CheckHeightShare(before, after, h3, 1.0f - strength);
		CheckHeightShare(before, after, h5, 1.0f - strength);
		size_t elsewhere = 0;
		for (int bin = 0; bin < BIN_COUNT; bin++)
		{
			if (std::abs(bin - h3) > 1 && std::abs(bin - h5) > 1 && after[bin] != before[bin])
				++elsewhere;
		}
		CHECK(elsewhere == 0);
	}
}

TEST_CASE("CQTHarmonicSuppressor: below full STRENGTH a let-through harmonic explains its own series")
{
	// "A flattened harmonic explains nothing"'s C3 and D6, plus C3's 12th partial, G6, so G4's own series has an
	// envelope at D6. At full strength G4 is flattened and D6 keeps its level. At 0.5 G4 keeps half
	// its height, which still clears ROOT_DB, so G4 is a root and takes half of D6, its 3rd
	// harmonic. With ROOT_DB over G4's half height, D6 is left alone again.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 8);
	AddHill(before, (float)NearestBin(C3_BIN, 12), 1.0f / 12.0f);
	const int d6 = NearestBin(C3_BIN, 9);
	AddHill(before, (float)d6, 0.2f);
	const int g4 = NearestBin(C3_BIN, 3);

	SUBCASE("full strength")
	{
		const kfr::univector<float> after = Suppress(before);
		CheckFlattened(after, g4);
		CheckUntouched(before, after, d6);
	}
	SUBCASE("half strength")
	{
		const kfr::univector<float> after = Suppress(before, { { "STRENGTH", 0.5f } });
		CheckHeightShare(before, after, g4, 0.5f);
		CheckHeightShare(before, after, d6, 0.5f);
	}
	SUBCASE("half strength, with G4 then under ROOT_DB")
	{
		const kfr::univector<float> after = Suppress(before, { { "STRENGTH", 0.5f }, { "ROOT_DB", 50.0f } });
		CheckHeightShare(before, after, g4, 0.5f);
		CheckUntouched(before, after, d6);
	}
}

TEST_CASE("CQTHarmonicSuppressor: a harmonic two series share is lowered by each in turn")
{
	// G4 is C3's 3rd harmonic and G3's 2nd, and each series fully explains it. At full strength C3
	// flattens it and G3 finds no peak there. At 0.5, C3 takes half and G3 half of what is left.
	kfr::univector<float> before = Floor();
	AddHill(before, (float)C3_BIN, 1.0f);
	AddHill(before, (float)C4_BIN, 0.5f);
	AddHill(before, (float)C5_BIN, 0.25f);
	// G3 is 21 bins over C3, and D5, its 3rd harmonic, gives it an envelope at G4.
	const int g3 = C3_BIN + 21;
	AddHill(before, (float)g3, 0.6f);
	AddHill(before, (float)NearestBin(g3, 3), 0.2f);
	const int g4 = NearestBin(C3_BIN, 3);
	REQUIRE(NearestBin(g3, 2) == g4);
	AddHill(before, (float)g4, 0.3f);

	SUBCASE("full strength")
	{
		CheckFlattened(Suppress(before), g4);
	}
	SUBCASE("half strength")
	{
		CheckHeightShare(before, Suppress(before, { { "STRENGTH", 0.5f } }), g4, 0.25f);
	}
}

TEST_CASE("CQTHarmonicSuppressor: an envelope whose upper neighbour is off the axis uses the lower one")
{
	// A note at bin 150 with its 2nd and 3rd partials. The 4th, h3's upper neighbour, would be at
	// bin 222, past the axis's last bin, 216. h3's envelope is then h2 alone, so the series is
	// credited with twice h2, and h3 goes.
	kfr::univector<float> before = Floor();
	const int root = 150;
	AddHill(before, (float)root, 1.0f);
	AddHill(before, (float)NearestBin(root, 2), 0.5f);
	const int h3 = NearestBin(root, 3);
	AddHill(before, (float)h3, 1.0f / 3.0f);
	REQUIRE(h3 + 1 < BIN_COUNT);
	REQUIRE(HarmonicBin((float)root, 4) > (float)(BIN_COUNT - 1));

	CheckFlattened(Suppress(before, { { "HARMONICS", 2 } }), h3);
}

TEST_CASE("CQTHarmonicSuppressor: a harmonic is matched by its interpolated top, not its bin")
{
	// The root sits so that its 3rd harmonic lies halfway between bins 93 and 94. At MATCH_CENTS 15
	// no bin centre lies within the window, as bins are 33 cents apart, so only the interpolated top
	// can find the harmonic.
	const float root = 93.5f - (float)BINS_PER_OCTAVE * std::log2(3.0f);
	kfr::univector<float> before = Floor();
	AddHill(before, root, 1.0f);
	AddHill(before, root + (float)BINS_PER_OCTAVE, 0.5f);
	AddHill(before, root + 2.0f * (float)BINS_PER_OCTAVE, 0.25f);
	const nlohmann::json settings = { { "MATCH_CENTS", 15 } };

	SUBCASE("2 cents off is found and flattened")
	{
		AddHill(before, 93.45f, 1.0f / 3.0f);
		CheckFlattened(Suppress(before, settings), 93);
	}
	SUBCASE("20 cents off is left alone")
	{
		AddHill(before, 94.1f, 1.0f / 3.0f);
		CheckUntouched(before, Suppress(before, settings), 94);
	}
}

TEST_CASE("CQTHarmonicSuppressor: of two peaks in the window, the nearer is the harmonic")
{
	// At MATCH_CENTS 50 the window holds bins 92 and 94, two one-bin peaks. The root puts its 3rd
	// harmonic at 93.005, so bin 92 is 33.5 cents away and bin 94 33.2 cents: 94 is the harmonic,
	// and 92 is left alone.
	const float root = 93.005f - (float)BINS_PER_OCTAVE * std::log2(3.0f);
	kfr::univector<float> before = Floor();
	AddHill(before, root, 1.0f);
	AddHill(before, root + (float)BINS_PER_OCTAVE, 0.5f);
	AddHill(before, root + 2.0f * (float)BINS_PER_OCTAVE, 0.25f);
	before[92] = FLOOR + 0.3f;
	before[94] = FLOOR + 0.3f;
	const kfr::univector<float> after = Suppress(before, { { "MATCH_CENTS", 50 } });

	CHECK(after[94] == doctest::Approx(FLOOR).epsilon(TOLERANCE));
	CHECK(after[92] == before[92]);
}

TEST_CASE("CQTHarmonicSuppressor: a flat-topped harmonic is found as PeakExtractor finds it")
{
	// G4's top is two equal bins. PeakExtractor takes the left one for a peak, above the bin below
	// and level with the bin above, so the suppressor has to as well, or PeakExtractor picks a
	// harmonic the suppressor never saw.
	kfr::univector<float> before = Floor();
	AddSeries(before, C3_BIN, 9);
	const int g4 = NearestBin(C3_BIN, 3);
	before[g4 - 1] = 0.18f;
	before[g4] = 0.3f;
	before[g4 + 1] = 0.3f;
	before[g4 + 2] = 0.18f;
	REQUIRE(LocalMaxima(before).count(g4) == 1);
	const kfr::univector<float> after = Suppress(before);

	for (int bin = g4 - 1; bin <= g4 + 2; bin++)
	{
		CAPTURE(bin);
		CHECK(after[bin] == doctest::Approx(FLOOR).epsilon(TOLERANCE));
	}
}

TEST_CASE("CQTHarmonicSuppressor: an envelope reads a neighbour whose top is a bin off")
{
	// C5 one bin, 33 cents, sharp of C3's 4th partial. The envelope reads every bin within
	// MATCH_CENTS and at least the bins either side of the nearest, so it reads C5's top. G4 sits
	// inside the credit that makes, and over the one C5's side would make.
	kfr::univector<float> before = Floor();
	AddHill(before, (float)C3_BIN, 1.0f);
	AddHill(before, (float)C4_BIN, 0.5f);
	AddHill(before, (float)(C5_BIN + 1), 0.25f);
	const int g4 = NearestBin(C3_BIN, 3);
	AddHill(before, (float)g4, 0.63f - FLOOR);

	const float gain = std::pow(10.0f, 6.0f / 20.0f);
	REQUIRE(before[g4] < gain * std::sqrt(before[C4_BIN] * before[C5_BIN + 1]));
	REQUIRE(before[g4] > gain * std::sqrt(before[C4_BIN] * before[C5_BIN]));
	CheckFlattened(Suppress(before), g4);
}

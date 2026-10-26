#pragma once
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <span>
#include <kfr/base/univector.hpp>

namespace Adagio
{
	const std::map<int, std::string> NoteNames = {
			{0, "C"},
			{1, "Db"},
			{2, "D"},
			{3, "Eb"},
			{4, "E"},
			{5, "F"},
			{6, "Gb"},
			{7, "G"},
			{8, "Ab"},
			{9, "A"},
			{10, "Bb"},
			{11, "B"}
	};

	const std::map<std::string, int> NoteIndices = {
		{"C", 0},
		{"Db", 1},
		{"D", 2},
		{"Eb", 3},
		{"E", 4},
		{"F", 5},
		{"Gb", 6},
		{"G", 7},
		{"Ab", 8},
		{"A", 9},
		{"Bb", 10},
		{"B", 11}
	};

	class AnalysisUtils
	{
	public:
		static float MaxOverRange(const kfr::univector<float>& data, size_t startBin, size_t endBin)
		{
			return *std::max_element(data.begin() + startBin, data.begin() + endBin + 1);
		}

		static float Median(const kfr::univector<float>& spectrum, size_t startBin, size_t endBin)
		{
			std::vector<float> band(spectrum.begin() + startBin, spectrum.begin() + endBin + 1);
			return MedianBand(band);
		}

		static float Median(const std::vector<float>& spectrum, size_t startBin, size_t endBin)
		{
			std::vector<float> band(spectrum.begin() + startBin, spectrum.begin() + endBin + 1);
			return MedianBand(band);
		}

		// Reorders band in place, so pass a copy or a scratch buffer, never the spectrum itself.
		static float MedianBand(std::span<float> band)
		{
			const size_t mid = band.size() / 2;
			std::nth_element(band.begin(), band.begin() + mid, band.end());
			float median = band[mid];
			if ((band.size() & 1) == 0)
			{
				const float lower = *std::max_element(band.begin(), band.begin() + mid);
				median = (lower + median) * 0.5f;
			}
			return median;
		}

		static float ParabolicInterpolation(const kfr::univector<float>& data, int bin)
		{
			if (bin <= 0 || bin >= (int)(data.size() - 1))
				return (float)bin;

			const float alpha = data[bin - 1];
			const float beta = data[bin];
			const float gamma = data[bin + 1];
			return ParabolicInterpolation(bin, alpha, beta, gamma);
		}

		static float ParabolicInterpolation(const std::vector<float>& data, int bin)
		{
			if (bin <= 0 || bin >= (int)(data.size() - 1))
				return (float)bin;

			const float alpha = data[bin - 1];
			const float beta = data[bin];
			const float gamma = data[bin + 1];
			return ParabolicInterpolation(bin, alpha, beta, gamma);
		}

		static float ToDb(float value)
		{
			constexpr float epsilon = 1e-12f;
			return 20.0f * std::log10(std::max(value, epsilon));
		}

		// How far a value stands above the frame's noise floor, in dB, mapped onto
		// [0,1]. Both ends of that measure are properties of the frame, so a lone
		// peak still scores what its level earns.
		static float NormaliseAboveFloor(float value, float floorDb, int harmonics)
		{
			float rangeDb = 60.0f * (harmonics + 1);
			return std::clamp((ToDb(value) - floorDb) / rangeDb, 0.0f, 1.0f);
		}

	private:
		static float ParabolicInterpolation(int bin, float alpha, float beta, float gamma)
		{
			const float denominator = alpha - 2.0f * beta + gamma;
			if (std::abs(denominator) < 1e-6f)
				return (float)bin;

			const float delta = 0.5f * (alpha - gamma) / denominator;
			return (float)bin + delta;
		}
	};
}

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

	struct InterpolatedPeak
	{
		float Bin = 0.0f;
		float Magnitude = 0.0f;
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

		// The top of a parabola through a peak's three bins in dB.
		static InterpolatedPeak LogParabolicPeak(std::span<const float> data, int bin)
		{
			if (bin <= 0 || bin >= (int)data.size() - 1)
				return { (float)bin, data[bin] };

			const float alpha = ToDb(data[bin - 1]);
			const float beta = ToDb(data[bin]);
			const float gamma = ToDb(data[bin + 1]);
			const float denominator = alpha - 2.0f * beta + gamma;
			if (std::abs(denominator) < 1e-6f)
				return { (float)bin, data[bin] };

			const float delta = 0.5f * (alpha - gamma) / denominator;
			const float topDb = beta - 0.25f * (alpha - gamma) * delta;
			return { (float)bin + delta, std::pow(10.0f, topDb / 20.0f) };
		}

		// The level of the higher of the valleys either side of a peak, walking at most maxSpan bins each way.
		static float HigherValley(std::span<const float> spectrum, size_t bin, size_t maxSpan)
		{
			size_t lower = bin;
			while (lower > 0 && bin - lower < maxSpan && spectrum[lower - 1] < spectrum[lower])
				lower--;
			size_t upper = bin;
			while (upper + 1 < spectrum.size() && upper - bin < maxSpan && spectrum[upper + 1] <= spectrum[upper])
				upper++;
			return std::max(spectrum[lower], spectrum[upper]);
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

		static bool IsLocalMax(const kfr::univector<float>& data, int bin)
		{
			if (bin <= 0 || bin >= (int)(data.size() - 1))
				return false;

			return data[bin] > data[bin - 1] && data[bin] >= data[bin + 1];
		}

		static float GeometricMean(const kfr::univector<float>& data)
		{
			double logSum = 0.0;
			for (size_t i = 0; i < data.size(); i++)
				logSum += std::log(std::max(data[i], 1e-12f));
			return (float)std::exp(logSum / (double)data.size());
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

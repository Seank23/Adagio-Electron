#pragma once
#include <nlohmann/json.hpp>

namespace Adagio
{
	static bool IsSameValue(const nlohmann::json& a, const nlohmann::json& b)
	{
		if (a.is_number() && b.is_number())
			return a.get<float>() == b.get<float>();
		return a == b;
	}
}
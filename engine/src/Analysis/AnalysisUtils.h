#pragma once
#include <map>
#include <string>

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
}

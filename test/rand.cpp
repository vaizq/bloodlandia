#include "util.h"
#include <catch2/catch_test_macros.hpp>




TEST_CASE("randf is distributed evenly", "[randf]") {
	float total = 0;
	constexpr int n = 1000;
	for(int i = 0; i < n; ++i) {
		total += RandFloat(100);
	}

	REQUIRE(std::abs(50.0f - total / n) < 1.0f);
}


TEST_CASE("randInt is distributed evenly", "[randInt]") {
	int total = 0;
	constexpr int n = 1000;

	int smallest = 99;
	int largest = 0;
	for(int i = 0; i < n; ++i) {
		int val = RandInt(100);
		smallest = std::min(smallest, val);
		largest = std::max(largest, val);
		total += val;
	}

	REQUIRE(std::abs(50.0f - 1.0f * total/ n) < 1.0f);
	REQUIRE(smallest == 0);
	REQUIRE(largest == 99);
}

TEST_CASE("rand sign", "[randSign]") {
	int total = 0;
	constexpr int n = 1000;

	for(int i = 0; i < n; ++i) {
		if (RandSign() == 1)
			++total;
		else
			--total;
	}

	REQUIRE(total == 0);
}

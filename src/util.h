#ifndef UTIL_H
#define UTIL_H

#include <random>

// Returns random integer in range [0, n)
static int RandInt(int n) {
	static std::mt19937 mt(std::random_device{}());
	static std::uniform_int_distribution<int> dist(0, n - 1);
	return dist(mt);
}

// Returns random float in range [0, n]
static float RandFloat(float n) {
	static std::mt19937 mt(std::random_device{}());
	static std::uniform_real_distribution<float> dist(0, n);
	return dist(mt);
}

static float RandSign() {
	return RandInt(2) == 0 ? -1.0f : 1.0f;
}


#endif

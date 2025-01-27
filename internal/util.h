#ifndef UTIL_H
#define UTIL_H

#include <chrono>
#include <random>
#include "rl.h"

using Clock = std::chrono::high_resolution_clock;

// Returns random integer in range [0, n)
static int RandInt(int n) {
	static std::mt19937 mt(std::random_device{}());
	std::uniform_int_distribution<int> dist(0, n - 1);
	return dist(mt);
}

// Returns random float in range [0, n)
static float RandFloat(float n) {
	static std::mt19937 mt(std::random_device{}());
	std::uniform_real_distribution<float> dist(0, n);
	return dist(mt);
}

static int RandSign() {
	const int val = RandInt(100) >= 50 ? 1 : -1;
	return val;
}

static rl::Vector2 operator-(rl::Vector2 a, rl::Vector2 b) {
	return rl::Vector2(a.x - b.x, a.y - b.y);
}

static rl::Vector2 operator+(rl::Vector2 a, rl::Vector2 b) {
	return rl::Vector2(a.x + b.x, a.y + b.y);
}

static rl::Vector2 operator*(float a, rl::Vector2 b) {
	return rl::Vector2(a * b.x, a * b.y);
}

static rl::Vector2 operator/(rl::Vector2 v, float a) {
	return rl::Vector2(v.x / a, v.y / a);
}

static float distance(rl::Vector2 a, rl::Vector2 b) {
	return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

static float length(rl::Vector2 v) {
	return sqrt(v.x*v.x + v.y*v.y);
}

static rl::Vector2 unit(rl::Vector2 v) {
	return v / length(v);
}

#endif

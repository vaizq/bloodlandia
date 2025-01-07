#ifndef WORLD_H
#define WORLD_H

#include <vector>
#include <random>
#include "rl.h"

struct Block{
	rl::Vector2 pos;
	rl::Vector2 size;
};

class World {
public:
	World() {
		std::mt19937 mt{1};
		std::uniform_real_distribution<float> dist(-1000.f, 1000.f);
		constexpr int n = 1000;
		for (int i = 0; i < n; ++i) {
			blocks.emplace_back(rl::Vector2{dist(mt), dist(mt)}, rl::Vector2{10, 10});
		}
	}

	const std::vector<Block>& getBlocks() const {
		return blocks;
	}
private:
	std::vector<Block> blocks;
};

#endif

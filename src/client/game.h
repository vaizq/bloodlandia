#ifndef GAME_H
#define GAME_H

#include <chrono>
#include "rl.h"
#include <vector>

struct Player {
	rl::Vector2 pos;
	rl::Vector2 target;
};

struct Enemy {
	rl::Vector2 pos;
	rl::Vector2 velo;
};

class Game {
	using Clock = std::chrono::high_resolution_clock;
public:
	Game(const char* serverAddr);
	void run();
private:
	void init();
	void update();
	void render();

	rl::Vector2 worldPosToScreenCoord(rl::Vector2 pos);
	rl::Vector2 screenCoordToWorldPos(rl::Vector2 coord);

	Clock::time_point prevUpdate;
	Player player;
	std::vector<Enemy> enemies;
};


#endif

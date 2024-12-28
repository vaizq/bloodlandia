#ifndef GAME_H
#define GAME_H

#include <memory>
#include "net.h"

struct GameState {
	int playerPos;
	int enemyPos;
	int ballPosX;
	int ballPosY;
	int ballVeloX;
	int ballVeloY;
	int playerScore = 0;
	int enemyScore = 0;
};

class Game {
	using Clock = std::chrono::high_resolution_clock;
public:
	Game(unsigned short port, unsigned short peerPort, bool judge = false);
	void run();
private:
	void init();
	void restart();
	void update();
	void render();
	void drawScore();

	unsigned short port, peerPort;
	bool judge;
	std::unique_ptr<net<GameState>> mnet;
	GameState state;
	Clock::time_point prevUpdate;
};


#endif

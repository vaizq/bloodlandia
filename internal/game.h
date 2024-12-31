#ifndef GAME_H
#define GAME_H

#include <memory>
#include "queueclient.hpp"
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

enum class GameStatus
{
	Pending,
	Queue,
	Waiting,
	Playing,
};

class Game {
	using Clock = std::chrono::high_resolution_clock;
public:
	Game(const char* serverAddr);
	void run();
private:
	void init();
	void restart();
	void update();
	void renderGame();
	void renderWaiting();
	void drawScore();
	void drawPing();

	const char* serverAddr;
	bool judge{false};
	std::unique_ptr<net<GameState>> mnet;
	std::unique_ptr<QueueClient> qc;
	GameState state;
	Clock::time_point prevUpdate;
	GameStatus status = GameStatus::Pending;
	bool isReady{false};
};


#endif

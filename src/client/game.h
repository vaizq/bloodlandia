#ifndef GAME_H
#define GAME_H

#include <chrono>
#include "rl.h"
#include <vector>
#include <asio.hpp>
#include "protocol.h"

using udp = asio::ip::udp;

struct Player {
	proto::ID id;
	rl::Vector2 pos;
	rl::Vector2 velo;
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
	void startReceive();

	void eventMove();
	void eventShoot();

	rl::Vector2 worldPosToScreenCoord(rl::Vector2 pos);
	rl::Vector2 screenCoordToWorldPos(rl::Vector2 coord);

	Clock::time_point prevUpdate;
	Clock::time_point prevServerUpdate;
	Player player;
	std::vector<Enemy> enemies;
	asio::io_context ioc;
	udp::socket socket;
	const udp::endpoint server;
	udp::endpoint peer;

	char buf[4048];
};


#endif

#ifndef GAME_H
#define GAME_H

#include <chrono>
#include "rl.h"
#include <vector>
#include <asio.hpp>
#include <map>
#include "protocol.h"
#include "connection.h"

using udp = asio::ip::udp;

struct Player : public proto::Player {
	rl::Vector2 target;
};

struct Message {
	proto::Header header;
	std::vector<char> payload;
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

	proto::ID nextID();
	void eventMove();
	void eventShoot();

	rl::Vector2 worldPosToScreenCoord(rl::Vector2 pos);
	rl::Vector2 screenCoordToWorldPos(rl::Vector2 coord);

	Clock::time_point prevUpdate;
	Clock::time_point prevServerUpdate;
	Connection con;

	Player player;
	std::vector<proto::Player> enemies;
	std::vector<proto::Bullet> bullets;
	bool viewStats{false};
};


#endif

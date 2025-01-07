#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <memory>
#include <cstring>
#include "rl.h"
#include "connection.h"

namespace proto {

using ID = uint32_t;

constexpr float playerSpeed = 15.f;
constexpr float bulletSpeed = 100.0f;
constexpr float playerRadius = 5.f / 6;
constexpr float enemyRadius = 1.f;
constexpr float bulletRadius = 0.1f;
constexpr uint32_t maxHealth{100};

constexpr Channel moveChannel = openChannelStart + 1;
constexpr Channel shootChannel = openChannelStart + 2;
constexpr Channel updateChannel = openChannelStart + 3;
constexpr Channel mouseMoveChannel = openChannelStart + 4;

struct Header {
	ID playerId;
	uint64_t payloadSize;
};

struct Stats {
	uint32_t kills{0};
	uint32_t deaths{0};
};

struct Player {
	ID id;
	rl::Vector2 pos{0, 0};
	rl::Vector2 velo{0, 0};
	Stats stats;
	Clock::time_point createdAt{Clock::now()};
	rl::Vector2 target{0, 0};
	uint32_t health{maxHealth};
};

struct Bullet {
	rl::Vector2 pos;
	rl::Vector2 velo;
	ID shooterID;
	Clock::time_point createdAt{Clock::now()};
};

struct GameState {
	Player players[64];
	uint64_t numPlayers;
	Bullet bullets[128];
	uint64_t numBullets;
};

struct Move {
	rl::Vector2 velo;
};

struct MouseMove {
	rl::Vector2 pos;
};

struct Shoot {
	Bullet bullet;
};

static std::pair<char*, size_t> makeMessage(Header header, const void* data) {
	const size_t n = sizeof header + header.payloadSize;
	char* buf = new char[n];

	std::memcpy(buf, &header, sizeof header);
	std::memcpy(buf + sizeof header, data, header.payloadSize);
	return {buf, n};
}

}

#endif

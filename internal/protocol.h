#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <memory>
#include <cstring>
#include "rl.h"
#include "connection.h"

namespace proto {

using ID = uint32_t;

constexpr Channel moveChannel = 1;
constexpr Channel shootChannel = 2;
constexpr Channel updateChannel = 3;


struct Header {
	ID playerId;
	uint64_t payloadSize;
};

struct Move {
	rl::Vector2 velo;
};

struct Shoot {
	rl::Vector2 target;
};

struct Player {
	ID id;
	rl::Vector2 pos{0, 0};
	rl::Vector2 velo{0, 0};
};

struct GameState {
	Player players[64];
	uint64_t numPlayers;
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

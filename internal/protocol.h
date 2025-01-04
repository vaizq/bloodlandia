#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <memory>
#include <cstring>
#include "rl.h"

namespace proto {

using ID = uint32_t;

enum class Event : uint8_t {
	Move,
	Shoot,
	Update
};

enum class MessageType : uint8_t {
	Unreliable,
	Reliable,
	Confirmation
};

struct Header {
	Event event;
	ID playerId;
	uint64_t payloadSize;
	ID messageId{0};
	MessageType type{MessageType::Unreliable};
};

struct Move {
	rl::Vector2 velo;
};

struct Shoot {
	rl::Vector2 direction;
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

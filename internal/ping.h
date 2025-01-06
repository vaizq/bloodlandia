#ifndef PING_H
#define PING_H

#include <chrono>
#include "connection.h"
#include "util.h"


using udp = asio::ip::udp;


constexpr Channel pingChannel1 = 420;

struct PingMessage {
	uint32_t senderID;
	uint32_t pingID;
};


class Ping {
public:
	Ping(Connection& connection, Clock::duration interval = std::chrono::milliseconds(100))
	: connection{connection},
	  interval{interval},
	  timer{connection.get_executor()}
	{
		prevReceive = Clock::now();
		msg.senderID = RandInt(std::numeric_limits<int>::max());
		listen();	
		send();
	}

	Clock::duration value() const { 
		if (Clock::now() - prevReceive > std::chrono::milliseconds(1000)) {
			return std::chrono::milliseconds(1001);
		}
		return ping; 
	}

private:
	void listen() {
		connection.listen(
			pingChannel1, 
			[this](char* data, size_t n) {
				if (n < sizeof (PingMessage)) {
					fprintf(stderr, "ERROR: received ping message is too small. (%ld bytes)\n", n);
				}
				PingMessage received;
				std::memcpy(&received, data, sizeof received);

				const auto dt = Clock::now() - prevSend;
				if (received.senderID == msg.senderID) {
					prevReceive = Clock::now();
					ping = dt;
				} else {
					connection.write(
						pingChannel1,
						&received,
						sizeof received
					);
				}
			}
		);
	}
	void send() {
		++msg.pingID;
		connection.write(
			pingChannel1, 
			&msg, 
			sizeof msg, 
			[this](std::error_code ec, size_t) {
				prevSend = Clock::now();
				timer.expires_after(interval);
				timer.async_wait([this](std::error_code ec) {
					send();
				});
			}
		);
	}

	Connection& connection;
	const Clock::duration interval;
	asio::high_resolution_timer timer;
	PingMessage msg{0};
	Clock::time_point prevSend;
	Clock::time_point prevReceive;
	Clock::duration ping{0};
};


#endif

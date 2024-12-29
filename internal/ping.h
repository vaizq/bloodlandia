#ifndef PING_H
#define PING_H

#include <chrono>
#include "channel.h"


using udp = asio::ip::udp;


constexpr Channel pingChannel1 = 71;
constexpr Channel pingChannel2 = 72;


class Ping {
public:
	using Clock = std::chrono::high_resolution_clock;

	Ping(Connection& connection, Clock::duration interval = std::chrono::milliseconds(100))
	: connection{connection},
	  interval{interval},
	  timer{connection.get_executor()}
	{
		connection.listen(
			pingChannel1, 
			[this](char* data, size_t n) {
				uint32_t receivedID;
				std::memcpy(&receivedID, data, sizeof receivedID);
				const auto dt = Clock::now() - prevSend;
				if (receivedID == id) {
					ping = dt;
				} else if (receivedID < id){
					ping = dt + this->interval * (id - receivedID);
				} else {
					fprintf(stderr, "ERROR: invalid ping ID received: expecting %d received %d\n", id, receivedID);
				}	
			}
		);

		// Send pings back
		connection.listen(
			pingChannel2, 
			[this](char* data, size_t n) {
				uint32_t receivedID;
				std::memcpy(&receivedID, data, sizeof receivedID);
				this->connection.write(pingChannel1, &receivedID, sizeof receivedID);
			}
		);

		send();
	}

	Clock::duration value() const { 
		return ping; 
	}

private:
	void send() {
		++id;
		connection.write(
			pingChannel1, 
			&id, 
			sizeof id, 
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
	uint32_t id;
	Clock::time_point prevSend;
	Clock::duration ping;
};


#endif

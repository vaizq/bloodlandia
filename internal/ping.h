#ifndef PING_H
#define PING_H


#include <asio.hpp>
#include <chrono>

using udp = asio::ip::udp;

class Ping {
public:
	using Clock = std::chrono::high_resolution_clock;

	Ping(udp::socket& socket, udp::endpoint endpoint, Clock::duration interval = std::chrono::milliseconds(100))
	: socket{socket}, 
	  endpoint{std::move(endpoint)}, 
	  interval{interval},
	  timer{socket.get_executor()}
	{
		receive();
		send();
	}
private:
	void send() {
		++id;
		std::memcpy(bufOut, &id, sizeof bufOut);
		socket.async_send_to(
			asio::buffer(bufOut, sizeof bufOut),
			endpoint,
			[this](std::error_code ec, size_t n) {
				prevSend = Clock::now();
				timer.expires_after(interval);
				timer.async_wait([this](std::error_code ec) {
					send();
				});
		});
	}

	void receive() {
		socket.async_receive(
			asio::buffer(bufIn, sizeof bufIn),
			[this](std::error_code ec, size_t n) {
				uint32_t receivedID;
				std::memcpy(&receivedID, bufIn, sizeof receivedID);
				const auto dt = Clock::now() - prevSend;
				if (receivedID == id) {
					ping = dt;
				} else if (receivedID < id){
					ping = dt + interval * (id - receivedID);
				} else {
					fprintf(stderr, "ERROR: invalid ping ID received: expecting %d received %d\n", id, receivedID);
				}
		});
	}

	udp::socket& socket;
	udp::endpoint endpoint;
	const Clock::duration interval;
	asio::high_resolution_timer timer;
	uint32_t id;
	Clock::time_point prevSend;
	Clock::duration ping;
	char bufOut[sizeof id];
	char bufIn[sizeof id];
};


#endif

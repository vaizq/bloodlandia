#ifndef NET_H
#define NET_H

#include <asio.hpp>
#include <asio/high_resolution_timer.hpp>
#include <asio/ip/address.hpp>
#include <asio/placeholders.hpp>
#include <chrono>
#include <queue>
#include "channel.h"


using udp = asio::ip::udp;


constexpr Channel channel = 69;


struct Ping {
	unsigned int id;
};


template <typename Mail>
class net 
{
	using Clock = std::chrono::high_resolution_clock;
public:
	net(unsigned short port, const udp::endpoint& peer) 
	: connection{peer, port},
	  pingTimer{connection.get_executor()}
	{
		prevPingReceived = Clock::now();
		ping();
		connection.listen(channel, [this](char* buf, size_t n) {
			switch (n)
			{
			case sizeof (Mail):
			{
				Mail mail;
				memcpy(&mail, buf, sizeof mail);
				mailbox.push(mail);
				break;
			}
			case sizeof (Ping):
			{
				memcpy(&prevPing, buf, sizeof prevPing);
				prevPingReceived = Clock::now();
				break;
			}
			default:
				printf("ERROR: invalid number of bytes received: %ld\n", n);
			}

			receive();
		});
	}

	void send(const Mail& mail) 
	{
		connection.write(channel, &mail, sizeof mail);
		prevSend = Clock::now();
	}

	Clock::time_point prevSendTime() {
		return prevSend;
	}

	bool hasPeer() {
		return Clock::now() - prevPingReceived < std::chrono::milliseconds(500);
	}

	std::queue<Mail>& getMailbox() {
		return mailbox;
	}

	void poll() 
	{
		connection.poll();
	}
private:
	void receive() {
	}

	void ping() {
		++nextPing.id;
		connection.write(channel, &nextPing, sizeof nextPing);
		pingTimer.expires_after(pingInterval);
		pingTimer.async_wait([this](std::error_code ec) {
			ping();
		});
	}
private:
	Connection connection;
	std::queue<Mail> mailbox;

	Ping prevPing;
	Clock::time_point prevPingReceived;
	Ping nextPing{1};
	asio::high_resolution_timer pingTimer;
	static constexpr Clock::duration pingInterval = std::chrono::milliseconds(100);
	Clock::time_point prevSend;
};

#endif

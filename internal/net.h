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


constexpr Channel mailChannel = 69;
constexpr Channel pingChannel = 70;



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
		connection.listen(mailChannel, [this](char* buf, size_t n) {
			if (n == sizeof (Mail)) {
				Mail mail;
				memcpy(&mail, buf, sizeof mail);
				mailbox.push(mail);
			} else {
				fprintf(stderr, "ERROR (mail) invalid number of bytes received\n");
			}
		});

		connection.listen(pingChannel, [this](char* buf, size_t n) {
			if (n == sizeof prevPing) {
				memcpy(&prevPing, buf, sizeof prevPing);
				prevPingReceived = Clock::now();
			} else {
				fprintf(stderr, "ERROR (ping) invalid number of bytes received\n");
			}
		});
	}

	void send(const Mail& mail) 
	{
		connection.write(mailChannel, &mail, sizeof mail);
		prevSend = Clock::now();
	}

	Clock::time_point prevSendTime() {
		return prevSend;
	}

	bool hasPeer() {
		return Clock::now() - prevPingReceived < std::chrono::milliseconds(1000);
	}

	std::queue<Mail>& getMailbox() {
		return mailbox;
	}

	void poll() 
	{
		connection.poll();
	}
private:
	void ping() {
		++nextPing.id;
		connection.write(pingChannel, &nextPing, sizeof nextPing);
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

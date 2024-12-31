#ifndef NET_H
#define NET_H

#include <asio.hpp>
#include <asio/high_resolution_timer.hpp>
#include <asio/ip/address.hpp>
#include <asio/placeholders.hpp>
#include <chrono>
#include <queue>
#include "connection.h"
#include "ping.h"


using udp = asio::ip::udp;


constexpr Channel mailChannel = 69;
constexpr Channel readyChannel = 70;


template <typename Mail>
class net 
{
	using Clock = std::chrono::high_resolution_clock;
public:
	net(unsigned short port, const udp::endpoint& peer) 
	: connection{peer, port},
	  ping{connection}
	{
		connection.listen(mailChannel, [this](char* buf, size_t n) {
			if (n == sizeof (Mail)) {
				Mail mail;
				memcpy(&mail, buf, sizeof mail);
				mailbox.push(mail);
			} else {
				fprintf(stderr, "ERROR (mail) invalid number of bytes received\n");
			}
		});

		connection.listen(readyChannel, [this](char* buf, size_t n) {
			this->peerIsReady_ = true;
		});
	}

	void send(const Mail& mail) 
	{
		connection.write(mailChannel, &mail, sizeof mail);
	}

	void setReady() {
		connection.writeReliable(readyChannel, 0, 0);
	}

	Clock::duration getPing() {
		return ping.value();
	}

	std::queue<Mail>& getMailbox() {
		return mailbox;
	}

	bool isPeerReady() {
		return peerIsReady_;
	}

	void poll() 
	{
		connection.poll();
	}
private:
	Connection connection;
	std::queue<Mail> mailbox;
	Ping ping;	
	bool peerIsReady_{false};
};

#endif

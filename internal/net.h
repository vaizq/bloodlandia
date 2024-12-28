#ifndef NET_H
#define NET_H

#include <asio.hpp>
#include <asio/high_resolution_timer.hpp>
#include <asio/placeholders.hpp>
#include <chrono>
#include <queue>


using udp = asio::ip::udp;


struct Ping {
	unsigned int id;
};


template <typename Mail>
class net 
{
	using Clock = std::chrono::high_resolution_clock;
public:
	net(unsigned short port, unsigned short peerPort) 
	: sock{ioc, udp::endpoint{udp::v4(), port}},
	  peer{udp::endpoint{udp::v4(), peerPort}},
	  pingTimer{ioc}
	{
		ping();
		receive();
	}

	void send(const Mail& mail) 
	{
		std::memcpy(bufOut, &mail, sizeof bufOut);
		sock.async_send_to(
			asio::buffer(bufOut, sizeof bufOut), 
			peer, 
			[](std::error_code ec, size_t bytesTransferred){
				if (bytesTransferred != sizeof (Mail)) {
					printf("ERROR: invalid number of bytes sent: %ld\n", bytesTransferred);
				}
		});
	}

	bool hasPeer() {
		return Clock::now() - prevPingReceived < std::chrono::milliseconds(500);
	}

	std::queue<Mail>& getMailbox() {
		return mailbox;
	}

	void poll() 
	{
		ioc.poll();
	}
private:
	void receive() {
		sock.async_receive_from(
			asio::buffer(bufIn, sizeof bufIn),
			peer,
			[this](std::error_code ec, size_t bytesTransferred) {
				switch (bytesTransferred)
				{
				case sizeof (Mail):
				{
					Mail mail;
					memcpy(&mail, bufIn, sizeof mail);
					mailbox.push(mail);
					break;
				}
				case sizeof (Ping):
				{
					memcpy(&prevPing, bufIn, sizeof prevPing);
					prevPingReceived = Clock::now();
					break;
				}
				default:
					printf("ERROR: invalid number of bytes received: %ld\n", bytesTransferred);
				}

				receive();
			}
		);

	}

	void ping() {
		++nextPing.id;
		std::memcpy(bufOutPing, &nextPing, sizeof bufOutPing);
		sock.async_send_to(	
			asio::buffer(bufOutPing, sizeof bufOutPing),
			peer,
			[this](std::error_code ec, size_t bytesTransferred) {
				if (ec) {
					fprintf(stderr, "ERROR: ping: %s\n", ec.message().c_str());
					ping();
				}
				else if (bytesTransferred != sizeof (Ping)) {
					fprintf(stderr, "ERROR (ping): invalid number of bytes sent: %ld\n", bytesTransferred);
					ping();
				} else {
					pingTimer.expires_after(pingInterval);
					pingTimer.async_wait([this](std::error_code ec) {
						ping();
					});
				}
		});
	}
private:
	asio::io_context ioc;
	asio::ip::udp::socket sock;
	asio::ip::udp::endpoint peer;
	char bufOut[sizeof (Mail)];
	char bufIn[sizeof (Mail)];
	std::queue<Mail> mailbox;

	Ping prevPing;
	Clock::time_point prevPingReceived;
	Ping nextPing{1};
	Clock::time_point prevPingSent;
	char bufOutPing[sizeof (Ping)];
	asio::high_resolution_timer pingTimer;
	static constexpr Clock::duration pingInterval = std::chrono::milliseconds(100);
};

#endif

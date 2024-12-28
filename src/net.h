#ifndef NET_H
#define NET_H

#include <asio.hpp>
#include <asio/placeholders.hpp>
#include <queue>


using udp = asio::ip::udp;


template <typename Mail>
class net 
{
public:
	net(unsigned short port, unsigned short peerPort) 
	: sock{ioc, udp::endpoint{udp::v4(), port}},
	  peer{udp::endpoint{udp::v4(), peerPort}}
	{
		registerReceive();
	}

	void send(const Mail& mail) 
	{
		std::memcpy(bufOut, &mail, sizeof bufOut);
		sock.async_send_to(
			asio::buffer(bufOut, sizeof bufOut), 
			peer, 
			[](std::error_code ec, size_t bytesTransferred){
				if (bytesTransferred != sizeof (Mail)) {
					printf("ERROR: invalid number of bytes received: %ld\n", bytesTransferred);
				}
		});
	}

	std::queue<Mail>& getMailbox() {
		return mailbox;
	}

	void poll() 
	{
		ioc.poll();
	}
private:
	void receive(std::error_code ec, size_t bytesTransferred) {
		Mail mail;
		if (bytesTransferred == sizeof mail) {
			memcpy(&mail, bufIn, sizeof mail);
			mailbox.push(mail);
		} else {
			printf("ERROR: invalid number of bytes received: %ld\n", bytesTransferred);
		}
		registerReceive();
	}

	void registerReceive() {
		sock.async_receive_from(
			asio::buffer(bufIn, sizeof bufIn),
			peer,
			std::bind(
				&net::receive, 
				this,
				asio::placeholders::error,
				asio::placeholders::bytes_transferred
			)
		);

	}
private:
	asio::io_context ioc;
	asio::ip::udp::socket sock;
	asio::ip::udp::endpoint peer;
	char bufOut[sizeof (Mail)];
	char bufIn[sizeof (Mail)];
	std::queue<Mail> mailbox;
};

#endif

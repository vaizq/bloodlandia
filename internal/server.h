#ifndef SERVER_H
#define SERVER_H

#include <asio.hpp>
#include "connection.h"
#include <vector>


using udp = asio::ip::udp;


class Server {
public:
	using ConnectionPtr = std::unique_ptr<Connection>;

	Server(unsigned short port, unsigned short connPort)
	: serverSocket{ioc, udp::endpoint{udp::v4(), port}},
	  connSocket{ioc, udp::endpoint{udp::v4(), connPort}}
	{
		start();
	}

	std::vector<Connection>& getConnections() {
		return connections;
	}

	void poll() {
		ioc.poll();
	}
private:
	void start() {
		serverSocket.async_receive_from(
			asio::buffer(buf, sizeof buf),
			peer,
			[this](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR: server::start(): %s\n", ec.message().c_str());
					start();
				}
				connections.emplace_back(connSocket, peer);
				printf("INFO: client connected from %s:%d\n", peer.address().to_string().c_str(), peer.port());
				start();
		});
	}

	asio::io_context ioc;
	udp::socket serverSocket;
	udp::socket connSocket;
	std::vector<Connection> connections;
	char buf[1028];
	udp::endpoint peer;
};

#endif

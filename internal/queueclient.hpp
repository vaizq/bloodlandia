#ifndef QUEUECLIENT_HPP
#define QUEUECLIENT_HPP

#include <asio.hpp>
#include "protocol.h"

using tcp = asio::ip::tcp;

class QueueClient {
public:
	QueueClient(const char* serverAddr, const char* port) 
	: socket{ioc}, serverAddr{serverAddr}, serverPort{port}
	{}

	void start() {
		tcp::resolver resolver{ioc};
		auto endpoint = *resolver.resolve(serverAddr, serverPort).begin();
		printf("connecting to server at %s:%s\n", 
			endpoint.host_name().c_str(), 
			endpoint.service_name().c_str()
		);
		socket.async_connect(endpoint, [this](std::error_code ec) {
			if (ec) {
				fprintf(stderr, "ERROR: failed to connect: %s\n", ec.message().c_str());
				return;
			}
			printf("connected!\n");
			asio::async_read(socket,
				asio::buffer(buf, sizeof (Header)),
				[this](std::error_code ec, size_t n) {
					if (ec) {
						fprintf(stderr, "ERROR: failed to receive header: %s\n", ec.message().c_str());
						start();
						return;
					}
					Header h;
					std::memcpy(&h, buf, sizeof h);
					asio::async_read(socket,
						asio::buffer(buf, h.payloadSize),
						[this, header=h](std::error_code ec, size_t n) {
							if (ec) {
								fprintf(stderr, "ERROR: failed to receive payload: %s\n", ec.message().c_str());
								start();
								return;
							}
							handlePayload(header);
						});
				});
			});
	}

	bool hasPeer() const {
		return receivedEndpoint;
	}

	uint32_t getBindPort() const {
		return bindPort;
	}

	const asio::ip::udp::endpoint& getPeer() const {
		return endpoint;
	}

	bool isJudge() const {
		return judge;
	}

	void poll() {
		ioc.poll();
	}
private:
	void handlePayload(const Header& h) {
		switch (h.payloadType) {
		case PayloadType::ConnectInfo:
		{
			ConnectInfo info;
			std::memcpy(&info, buf, sizeof info);
			bindPort = info.bindPort;
			endpoint = asio::ip::udp::endpoint(asio::ip::make_address(info.peerAddress), info.peerPort);
			judge = info.isJudge;
			receivedEndpoint = true;
			break;
		}
		default:
			printf("Unknown payload type\n");
		}
	}

	const char* serverAddr;
	const char* serverPort;
	asio::io_context ioc;
	tcp::socket socket;
	char buf[1024];
	asio::ip::udp::endpoint endpoint;
	bool receivedEndpoint = false;
	uint32_t bindPort;
	bool judge{false};
};

#endif

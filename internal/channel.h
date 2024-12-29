#ifndef CHANNEL_H
#define CHANNEL_H

#include <asio.hpp>
#include <map>
#include <functional>

using udp = asio::ip::udp;

using Channel = uint32_t;

struct Header {
	Channel channel;
	uint64_t payloadSize;
};

class Connection {
public:
	using Listener = std::function<void(char*, size_t)>;

	Connection(udp::endpoint peer, unsigned short bindPort)
	: socket{ioc, udp::endpoint{udp::v4(), bindPort}},
	  peer{std::move(peer)}
	{}

	void write(Channel channel, char* data, size_t dataLen) {
		Header h{channel, dataLen};
		char buf[sizeof h + dataLen];
		std::memcpy(buf, &h, sizeof h);
		std::memcpy(buf + sizeof h, data, dataLen);

		socket.async_send_to(
			asio::buffer(buf, sizeof buf),
			peer,
			[](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR Connection::write: %s\n", ec.message().c_str());
				}
			});
	}

	void listen(Channel channel, Listener listener) {
		listeners[channel] = listener;
	}

	void poll() {
		ioc.poll();
	}

private:
	void startReceive() {
		socket.async_receive(
			asio::buffer(buf, sizeof buf),
			[this](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR Connection::startReceive: %s\n", ec.message().c_str());
					startReceive(); // Fuckit just try again
					return;
				}
				Header h;
				if (n == sizeof h + h.payloadSize) {
					std::memcpy(&h, buf, sizeof h);
					if (listeners.contains(h.channel)) {
						listeners[h.channel](buf + sizeof h, h.payloadSize);
					} else {
						fprintf(stderr, "ERROR: received data to channel %d that is not being listened\n", h.channel);
					}
				} else {
					fprintf(stderr, "ERROR: Size of the received datagram is not valid. (%ld bytes. Should be %ld)\n", 
						n, sizeof h + h.payloadSize);
				}

				startReceive();
			});
	}

	asio::io_context ioc;
	udp::socket socket;
	udp::endpoint peer;
	std::map<Channel, Listener> listeners;
	char buf[2048];
};


#endif

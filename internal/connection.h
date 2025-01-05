#ifndef CONNECTION_H 
#define CONNECTION_H

#include <asio.hpp>
#include <chrono>
#include <map>
#include <functional>
#include <queue>
#include <vector>
#include <format>


using udp = asio::ip::udp;

using Channel = uint32_t;
using Handler = std::function<void(std::error_code ec, size_t)>;
using Clock = std::chrono::high_resolution_clock;


struct Header {
	enum class Type {
		Unreliable,
		Reliable,
		Confirmation
	};
	Channel channel;
	uint64_t payloadSize;
	Type type;
	uint32_t id;

	std::string toString() const {
		return std::format("channel: {}\t id: {}\ttype: {}\tsize: {}\n", 
			channel, 
			id,
			(type == Header::Type::Unreliable ? "unreliable" : (type == Header::Type::Reliable ? "reliable" : "confirmation")), 
			payloadSize);
	}
};


class Connection {

	struct Message {
		Header header;
		std::vector<char> payload;
		Handler handler;
	};

public:
	using Listener = std::function<void(char*, size_t)>;

	Connection(udp::endpoint peer)
	: socket{ioc, udp::v4()},
	  peer{std::move(peer)},
	  timer{ioc}
	{
		start();
		startReceive();
		startSendReliable();
	}

	bool isConnected() {
		return isConnected_;
	}

	void write(Channel channel, const void* data, size_t dataLen, Handler handler = [](std::error_code, size_t){}) {
		static uint32_t id{0};
		send(
			Header{channel, (uint32_t)dataLen, Header::Type::Unreliable, ++id}, 
			data, 
			handler
		);
	}

	void writeReliable(Channel channel, const void* data, size_t dataLen, Handler handler = [](std::error_code, size_t){}) {
		static uint32_t id{0};

		const Message& msg = waitingForConfirmation[++id] = Message{
			Header{channel, (uint32_t)dataLen, Header::Type::Reliable, id},
			{(const char*) data, ((const char*) data) + dataLen}, 
			handler
		};

		send(msg);
	}

	void listen(Channel channel, Listener listener) {
		listeners[channel] = listener;
	}

	void poll() {
		ioc.poll();
	}

	asio::io_context& get_executor() {
		return ioc;
	}

private:
	void send(const Header& h, const void* data = nullptr, Handler handler = [](std::error_code, size_t){}) {
		const size_t totalSize = sizeof h + h.payloadSize;
		auto buf = std::make_unique<char[]>(totalSize);
		std::memcpy(buf.get(), &h, sizeof h);
		if (h.payloadSize > 0) {
			std::memcpy(buf.get() + sizeof h, data, h.payloadSize);
		}

		socket.async_send_to(
			asio::buffer(buf.get(), totalSize),
			peer,
			[handler, buf=std::move(buf)](std::error_code ec, size_t n) {
				handler(ec, n);
				if (ec) {
					fprintf(stderr, "ERROR Connection::write: %s\n", ec.message().c_str());
				}
			});
	}

	void send(const Message& msg) {
		socket.async_send_to(
			std::vector<asio::const_buffer>{
				asio::buffer((char*)&msg.header, sizeof (Header)), 
				asio::buffer(msg.payload)},
			peer,
			[](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR Connection::send(Message): %s\n", ec.message().c_str());
				}
			}
		);
	}

	void start() {
		startSendReliable();
		startReceive();
	}

	void startSendReliable() {
		const auto interval = std::chrono::milliseconds(50);
		timer.expires_after(interval);
		timer.async_wait([this](std::error_code ec) {
			if (ec) {
				printf("ERROR: timer async_wait: %s\n", ec.message().c_str());
			}

			for (auto& [_, msg] : waitingForConfirmation) {
				send(msg);
			}
			

			startSendReliable();
		});
	}

	void startReceive() {
		socket.async_receive_from(
			asio::buffer(buf, sizeof buf),
			peer,
			[this](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR Connection::startReceive: %s\n", ec.message().c_str());
					startReceive(); // Fuckit just try again
					return;
				}

				if (n < sizeof (Header)) {
					fprintf(stderr, "ERROR: received less bytes than the header is in length. (%ld bytes)\n", n);
					startReceive();
					return;
				}

				Header h;
				std::memcpy(&h, buf, sizeof h);

				if (n == sizeof h + h.payloadSize) {
					handleMessage(h);
				} else {
					fprintf(stderr, "ERROR: Size of the received datagram is not valid. (%ld bytes. Should be %ld)\n", 
						n, sizeof h + h.payloadSize);
				}

				startReceive();
			});
	}

	void handleMessage(const Header& h) {
		switch (h.type) {
		case Header::Type::Unreliable:
			if (listeners.contains(h.channel)) {
				listeners[h.channel](buf + sizeof h, h.payloadSize);
			} else {
				fprintf(stderr, "ERROR: received data to channel %d that is not being listened\n", h.channel);
			}
			break;
		case Header::Type::Reliable:
			if (listeners.contains(h.channel)) {
				listeners[h.channel](buf + sizeof h, h.payloadSize);
			} else {
				fprintf(stderr, "ERROR: received data to channel %d that is not being listened\n", h.channel);
			}

			send(Header{h.channel, 0, Header::Type::Confirmation, h.id});
			break;
		case Header::Type::Confirmation:
			if (waitingForConfirmation.erase(h.id) == 0) {
				fprintf(stderr, "ERROR: received confirmation for message %d that is not waiting to be confirmed\n", h.id);
			}
			break;
		}
	}

	asio::io_context ioc;
	udp::socket socket;
	udp::endpoint peer;
	std::map<Channel, Listener> listeners;
	char buf[2048];
	std::map<uint32_t, Message> waitingForConfirmation;
	asio::high_resolution_timer timer;
	bool isConnected_{false};
};


#endif

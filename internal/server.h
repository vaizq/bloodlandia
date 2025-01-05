#ifndef SERVER_H
#define SERVER_H

#include <asio.hpp>
#include "connection.h"
#include <vector>


using udp = asio::ip::udp;

struct Message {
	/*
	Message() = default;
	Message(udp::endpoint peer, const void* data, size_t datalen)
	: peer{peer}, payload{(const char*) data, ((const char*) data) + datalen}
	{}
	*/

	udp::endpoint peer; // Somekind of identifier for peer
	std::vector<char> payload;
};

class Server {

	struct ReliableMessage {
		Header h;
		Message msg;
	};

	struct ChannelInfo {
		uint32_t writeID{0};
		uint32_t writeReliableID{0};
		uint32_t receiveID{0};
		uint32_t receiveReliableID{0};

		std::map<uint32_t, ReliableMessage> unConfirmed;
	};


	struct PeerInfo {
		std::map<Channel, ChannelInfo> chInfo;
	};

public:
	using Listener = std::function<void(Message)>;
	Server(unsigned short port)
	: socket{ioc, udp::endpoint{udp::v4(), port}}
	{
		start();
	}

	void write(Channel ch, Message msg) {
		if (!peers.contains(msg.peer)) {
			peers[msg.peer] = PeerInfo{{{ch, ChannelInfo{}}}};
			printf("INFO: initialized peerinfo for new peer\n");
		} else if (!peers[msg.peer].chInfo.contains(ch)) {
			peers[msg.peer].chInfo[ch] = ChannelInfo{};
		}

		send({ch, msg.payload.size(), Header::Type::Unreliable, peers[msg.peer].chInfo[ch].writeID++}, msg);
			
	}

	void writeReliable(Channel ch, Message msg) {
		if (!peers.contains(msg.peer)) {
			peers[msg.peer] = PeerInfo{{{ch, ChannelInfo{}}}};
			printf("INFO: initialized peerinfo for new peer\n");
		} else if (!peers[msg.peer].chInfo.contains(ch)) {
			peers[msg.peer].chInfo[ch] = ChannelInfo{};
		}

		send({ch, msg.payload.size(), Header::Type::Unreliable, peers[msg.peer].chInfo[ch].writeReliableID++}, msg);
	}

	void listen(Channel ch, Listener listener) {
		listeners[ch] = listener;
	}

	void poll() {
		ioc.poll();
	}
private:
	void send(Header h, Message msg) {
		auto buf = std::make_unique<std::vector<char>>((const char*)&h, ((const char*)&h) + sizeof h);
		buf->insert(buf->end(), msg.payload.begin(), msg.payload.end());
		socket.async_send_to(
			asio::buffer(*buf),
			msg.peer,
			[buf = std::move(buf)](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR\t send(): %s\n", ec.message().c_str());
				}
		});
	}

	void start() {
		receive();
	}


	void receive() {
		socket.async_receive_from(
			asio::buffer(bufIn, sizeof bufIn),
			peer,
			[this](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR\t receive(): %s\n", ec.message().c_str());
					receive();
					return;
				}

				if (n < sizeof (Header)) {
					fprintf(stderr, "ERROR\t received less bytes than the header is in length. (%ld bytes)\n", n);
					receive();
					return;
				}

				Header h;
				std::memcpy(&h, bufIn, sizeof h);

				if (n == sizeof h + h.payloadSize) {
					handleMessage(h);
				} else {
					fprintf(stderr, "ERROR\t Invalid payloadSize. (%ld bytes. Should be %ld)\n", 
							h.payloadSize, n - sizeof h);
				}

				receive();
		});
	}

	void handleMessage(Header h) {
		if (!listeners.contains(h.channel)) {
			fprintf(stderr, "ERROR: no listener for channel %d\n", h.channel);
			return;
		}

		if (!peers.contains(peer)) {
			peers[peer] = PeerInfo{{{h.channel, ChannelInfo{}}}};
		} else if (!peers[peer].chInfo.contains(h.channel)) {
			peers[peer].chInfo[h.channel] = ChannelInfo{};
		}

		switch (h.type) {
			case Header::Type::Unreliable:
			{
				auto& prevID = peers[peer].chInfo[h.channel].receiveID;
				if (h.id < prevID) {
					return;
				} else {
					prevID = h.id;
				}
				break;
			}
			case Header::Type::Reliable:
			{
				send({h.channel, 0, Header::Type::Confirmation, h.id}, {peer, {}});						
				auto& prevID = peers[peer].chInfo[h.channel].receiveReliableID;
				if (h.id < prevID) {
					return;
				} else {
					prevID = h.id;
				}
				break;
			}
			case Header::Type::Confirmation:
				// our message got confirmed
				if (peers[peer].chInfo[h.channel].unConfirmed.erase(h.id) != 0) {
					printf("INFO\t message ch: %d id: %d confirmed\n", h.channel, h.id);
				}
				return;
		}

		listeners[h.channel](Message{peer, {bufIn + sizeof h, bufIn + sizeof h + h.payloadSize}});
	}

	asio::io_context ioc;
	udp::socket socket;
	std::map<udp::endpoint, PeerInfo> peers;
	std::map<Channel, Listener> listeners;
	char bufIn[2048];
	udp::endpoint peer;
};


static void run() {
}

#endif

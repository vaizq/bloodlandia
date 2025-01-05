#ifndef SERVER_H
#define SERVER_H

#include <asio.hpp>
#include "connection.h"
#include <vector>


using udp = asio::ip::udp;


class Server {
	struct Message {
		udp::endpoint peer; // Somekind of identifier for peer
		std::vector<char> payload;
	};


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
	using Listener = std::function<void(const udp::endpoint& ep, char* data, size_t datalen)>;
	Server(unsigned short port)
	: socket{ioc, udp::endpoint{udp::v4(), port}}
	{
		start();
	}

	void write(Channel ch, const udp::endpoint& peer, const void* data, size_t datalen) {
		if (!peers.contains(peer)) {
			peers[peer] = PeerInfo{{{ch, ChannelInfo{}}}};
			fprintf(stderr, "ERROR\t peerinfo did not exist for %s:%d\n",
					peer.address().to_string().c_str(), peer.port());
		}
		if (!peers[peer].chInfo.contains(ch)) {
			peers[peer].chInfo[ch] = ChannelInfo{};
			fprintf(stderr, "ERROR\t ChannelInfo on channel %d did not exist for %s:%d\n",
					ch, peer.address().to_string().c_str(), peer.port());

		}

		send({ch, datalen, Header::Type::Unreliable, peers[peer].chInfo[ch].writeID++},
		     {peer, {(const char*)data, (const char*)data + datalen}});
	}

	void writeReliable(Channel ch, const udp::endpoint& peer, const void* data, size_t datalen) {
		if (!peers.contains(peer)) {
			peers[peer] = PeerInfo{{{ch, ChannelInfo{}}}};
			fprintf(stderr, "ERROR\t peerinfo did not exist for %s:%d\n",
					peer.address().to_string().c_str(), peer.port());
		}
		if (!peers[peer].chInfo.contains(ch)) {
			peers[peer].chInfo[ch] = ChannelInfo{};
			fprintf(stderr, "ERROR\t ChannelInfo on channel %d did not exist for %s:%d\n",
					ch, peer.address().to_string().c_str(), peer.port());
		}

		send({ch, datalen, Header::Type::Reliable, peers[peer].chInfo[ch].writeReliableID++},
		     {peer, {(const char*)data, (const char*)data + datalen}});
	}

	void listen(Channel ch, Listener listener) {
		listeners[ch] = listener;
	}

	void poll() {
		ioc.poll_one();
	}
private:
	void send(Header h, const Message& msg) {
		const size_t n = sizeof h + msg.payload.size();
		char* buf = new char[n];
		std::memcpy(buf, &h, sizeof h);
		std::memcpy(buf + sizeof h, msg.payload.data(), msg.payload.size());

		socket.async_send_to(
			asio::buffer(buf, n),
			msg.peer,
			[buf](std::error_code ec, size_t n) {
				if (ec) {
					fprintf(stderr, "ERROR\t send(): %s\n", ec.message().c_str());
				}
				delete[] buf;
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
		} 
		if (!peers[peer].chInfo.contains(h.channel)) {
			peers[peer].chInfo[h.channel] = ChannelInfo{};
		}

		switch (h.type) {
			case Header::Type::Unreliable:
			{
				auto& prevID = peers[peer].chInfo[h.channel].receiveID;
				if (h.id < prevID) {
					printf("INFO\t received old message %d\n", h.id);
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
					printf("INFO\t received old message %d\n", h.id);
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

		listeners[h.channel](peer, bufIn + sizeof h, h.payloadSize);
	}

	asio::io_context ioc;
	udp::socket socket;
	std::map<udp::endpoint, PeerInfo> peers;
	std::map<Channel, Listener> listeners;
	char bufIn[10000];
	udp::endpoint peer;
};

#endif
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <cstring>

enum class PayloadType : uint32_t {
	ConnectInfo
};

struct Header {
	PayloadType payloadType;
	uint64_t payloadSize;
};

struct ConnectInfo {
	ConnectInfo() = default;
	ConnectInfo(uint32_t bindPort, const std::string& peerAddr, uint32_t peerPort, bool isJudge=false)
	: bindPort{bindPort}, peerPort{peerPort}, isJudge{isJudge}
	{
		std::memset(peerAddress, 0, sizeof peerAddress);
		std::memcpy(peerAddress, peerAddr.c_str(), peerAddr.size());
	}
	uint32_t bindPort;
	char peerAddress[128];
	uint32_t peerPort;
	bool isJudge{false};
};

#endif

// This file is public domain, in case it's useful to anyone. -comex

// The central server implementation.

#include "TraversalProto.h"
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <utility>

#define DEBUG 1

static u64 currentTime;

struct OutgoingPacketInfo
{
	TraversalPacket packet;
	TraversalRequestId misc;
	struct sockaddr_in6 dest;
	int tries;
	u64 sendTime;
};

template <typename K>
struct EvictingKey
{
	// 30 seconds
	const u64 EvictTimeout = 30 * 1000000;
	template <typename T>
	EvictingKey(T&& k) : m_K(std::forward<T>(k))
	{
		m_CreateTime = currentTime;
	}
	bool operator==(const EvictingKey<K>& other) const
	{
		return currentTime - m_CreateTime > EvictTimeout ||
		       currentTime - other.m_CreateTime > EvictTimeout ||
			   m_K == other.m_K;
	}
	K m_K;
	u32 m_CreateTime;
};

template <typename T>
struct std::hash<EvictingKey<T>>
{
	size_t operator()(const EvictingKey<T>& key) const
	{
		return std::hash<T>()(key.m_K);
	}
};

template <>
struct std::hash<TraversalHostId>
{
	size_t operator()(const TraversalHostId& id) const
	{
		auto p = (u32*) id.data();
		return p[0] ^ ((p[1] << 13) | (p[1] >> 19));
	}
};

static int sock;
static int urandomFd;
static std::unordered_map<
TraversalRequestId,
	OutgoingPacketInfo
	> outgoingPackets;
	static std::unordered_map<
	EvictingKey<TraversalRequestId>,
	std::pair<TraversalInetAddress, TraversalRequestId>
	> didSendInfo;
	static std::unordered_map<
	EvictingKey<TraversalHostId>,
	TraversalInetAddress
	> connectedClients;

static TraversalInetAddress MakeInetAddress(const struct sockaddr_in6& addr)
{
	if (addr.sin6_family != AF_INET6)
	{
		fprintf(stderr, "bad sockaddr_in6\n");
		exit(1);
	}
	u32* words = (u32*) addr.sin6_addr.s6_addr;
	TraversalInetAddress result = {0};
	if (words[0] == 0 && words[1] == 0 && words[2] == 0xffff0000)
	{
		result.isIPV6 = false;
		result.address[0] = words[3];
	}
	else
	{
		result.isIPV6 = true;
		memcpy(result.address, words, sizeof(result.address));
	}
	result.port = addr.sin6_port;
	return result;
}

static struct sockaddr_in6 MakeSinAddr(const TraversalInetAddress& addr)
{
	struct sockaddr_in6 result;
	result.sin6_len = sizeof(result);
	result.sin6_family = AF_INET6;
	result.sin6_port = addr.port;
	result.sin6_flowinfo = 0;
	if (addr.isIPV6)
	{
		memcpy(&result.sin6_addr, addr.address, 16);
	}
	else
	{
		u32* words = (u32*) result.sin6_addr.s6_addr;
		words[0] = 0;
		words[1] = 0;
		words[2] = 0xffff0000;
		words[3] = addr.address[0];
	}
	result.sin6_scope_id = 0;
	return result;
}

static void GetRandomBytes(void* output, size_t size)
{
	static u8 bytes[8192];
	static size_t bytesLeft = 0;
	if (bytesLeft < size)
	{
		ssize_t rv = read(urandomFd, bytes, sizeof(bytes));
		if (rv != sizeof(bytes))
		{
			perror("read from /dev/urandom");
			exit(1);
		}
		bytesLeft = sizeof(bytes);
	}
	memcpy(output, bytes + (bytesLeft -= size), size);
}

static void GetRandomHostId(TraversalHostId* hostId)
{
	char buf[9];
	u32 num;
	GetRandomBytes(&num, sizeof(num));
	sprintf(buf, "%08x", num);
	memcpy(hostId->data(), buf, 8);
}

static const char* SenderName(struct sockaddr_in6* addr)
{
	static char buf[INET6_ADDRSTRLEN + 10];
	inet_ntop(PF_INET6, &addr->sin6_addr, buf, sizeof(buf));
	sprintf(buf + strlen(buf), ":%d", ntohs(addr->sin6_port));
	return buf;
}

static void TrySend(const void* buffer, size_t size, struct sockaddr_in6* addr)
{
#if DEBUG
	printf("-> %d %llx %s\n", ((TraversalPacket*) buffer)->type, ((TraversalPacket*) buffer)->requestId, SenderName(addr));
#endif
	if ((size_t) sendto(sock, buffer, size, 0, (struct sockaddr*) addr, sizeof(*addr)) != size)
	{
		perror("sendto");
	}
}

static TraversalPacket* AllocPacket(const struct sockaddr_in6& dest, TraversalRequestId misc = 0)
{
	TraversalRequestId requestId;
	GetRandomBytes(&requestId, sizeof(requestId));
	OutgoingPacketInfo* info = &outgoingPackets[requestId];
	info->dest = dest;
	info->misc = misc;
	info->tries = 0;
	info->sendTime = currentTime;
	TraversalPacket* result = &info->packet;
	memset(result, 0, sizeof(*result));
	result->requestId = requestId;
	return result;
}

static void SendPacket(OutgoingPacketInfo* info)
{
	info->tries++;
	info->sendTime = currentTime;
	TrySend(&info->packet, sizeof(info->packet), &info->dest);
}


static void ResendPackets()
{
	std::vector<std::pair<TraversalInetAddress, TraversalRequestId>> todoFailures;
	todoFailures.clear();
	for (auto it = outgoingPackets.begin(); it != outgoingPackets.end();)
	{
		OutgoingPacketInfo* info = &it->second;
		if (currentTime - info->sendTime >= (u64) (300000 * info->tries))
		{
			if (info->tries >= 5)
			{
				if (info->packet.type == TraversalPacketPleaseSendPacket)
				{
					todoFailures.push_back(std::make_pair(info->packet.pleaseSendPacket.address, info->misc));
				}
				it = outgoingPackets.erase(it);
				continue;
			}
			else
			{
				SendPacket(info);
			}
		}
		++it;
	}

	for (auto it = todoFailures.begin(); it != todoFailures.end(); ++it)
	{
		TraversalPacket* fail = AllocPacket(MakeSinAddr(it->first));
		fail->type = TraversalPacketConnectFailed;
		fail->connectFailed.requestId = it->second;
	}
}

static void HandlePacket(TraversalPacket* packet, struct sockaddr_in6* addr)
{
#if DEBUG
	printf("<- %d %llx %s\n", packet->type, packet->requestId, SenderName(addr));
#endif
	bool packetOk = true;
	switch (packet->type)
	{
		case TraversalPacketAck:
			{
			auto it = outgoingPackets.find(packet->requestId);
			if (it == outgoingPackets.end())
				break;

			OutgoingPacketInfo* info = &it->second;

			if (info->packet.type == TraversalPacketPleaseSendPacket)
			{
				TraversalPacket* ready = AllocPacket(MakeSinAddr(info->packet.pleaseSendPacket.address));
				if (packet->ack.ok)
				{
					ready->type = TraversalPacketConnectReady;
					ready->connectReady.requestId = info->misc;
					ready->connectReady.address = MakeInetAddress(info->dest);
				}
				else
				{
					ready->type = TraversalPacketConnectFailed;
					ready->connectFailed.requestId = info->misc;
				}
			}

		outgoingPackets.erase(it);
		break;
		}
	case TraversalPacketPing:
		{
		packetOk = connectedClients.find(packet->ping.hostId) != connectedClients.end();
		break;
		}
	case TraversalPacketHelloFromClient:
		{
		u8 ok = packet->helloFromClient.protoVersion <= TraversalProtoVersion;
		TraversalPacket* reply = AllocPacket(*addr);
		reply->type = TraversalPacketHelloFromServer;
		reply->helloFromServer.ok = ok;
		if (ok)
		{
			TraversalHostId hostId;
			TraversalInetAddress* iaddr;
			// not that there is any significant change of
			// duplication, but...
			do
			{
				GetRandomHostId(&hostId);
				iaddr = &connectedClients[hostId];
			} while (iaddr->port);
			*iaddr = MakeInetAddress(*addr);

			reply->helloFromServer.yourAddress = *iaddr;
			reply->helloFromServer.yourHostId = hostId;
		}
		break;
		}
	case TraversalPacketConnectPlease:
		{
		TraversalHostId& hostId = packet->connectPlease.hostId;
		auto it = connectedClients.find(hostId);
		if (it == connectedClients.end())
		{
			TraversalPacket* reply = AllocPacket(*addr);
			reply->type = TraversalPacketConnectFailed;
			reply->connectFailed.requestId = packet->requestId;
		}
		else
		{
			TraversalPacket* please = AllocPacket(MakeSinAddr(it->second), packet->requestId);
			please->type = TraversalPacketPleaseSendPacket;
			please->pleaseSendPacket.address = MakeInetAddress(*addr);
		}
		break;
		}
	default:
		fprintf(stderr, "received unknown packet type %d from %s\n", packet->type, SenderName(addr));
	}
	if (packet->type != TraversalPacketAck)
	{
		TraversalPacket ack = {0};
		ack.type = TraversalPacketAck;
		ack.requestId = packet->requestId;
		ack.ack.ok = packetOk;
		TrySend(&ack, sizeof(ack), addr);
	}
}

int main()
{
	int rv;

	urandomFd = open("/dev/urandom", O_RDONLY);
	if (urandomFd < 0)
	{
		perror("open /dev/urandom");
		return 1;
	}

	sock = socket(PF_INET6, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		perror("socket");
		return 1;
	}
	int no = 0;
	rv = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
	if (rv < 0)
	{
		perror("setsockopt IPV6_V6ONLY");
		return 1;
	}
	struct in6_addr any = IN6ADDR_ANY_INIT;
	struct sockaddr_in6 addr;
	addr.sin6_len = sizeof(addr);
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(6262);
	addr.sin6_flowinfo = 0;
	addr.sin6_addr = any;
	addr.sin6_scope_id = 0;

	rv = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
	if (rv < 0)
	{
		perror("bind");
		return 1;
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 300000;
	rv = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (rv < 0)
	{
		perror("setsockopt SO_RCVTIMEO");
		return 1;
	}

	while (1)
	{
		struct sockaddr_in6 raddr;
		socklen_t addrLen = sizeof(raddr);
		TraversalPacket packet;
		// note: switch to recvmmsg (yes, mmsg) if this becomes
		// expensive
		rv = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr*) &raddr, &addrLen);
		if (gettimeofday(&tv, NULL) < 0)
		{
			perror("gettimeofday");
			exit(1);
		}
		currentTime = (u64) tv.tv_sec * 1000000 + tv.tv_usec;
		if (rv < 0)
		{
			if (errno != EAGAIN)
			{
				perror("recvfrom");
				return 1;
			}
		}
		else if ((size_t) rv < sizeof(packet))
		{
			fprintf(stderr, "received short packet from %s\n", SenderName(&raddr));
		}
		else
		{
			HandlePacket(&packet, &raddr);
		}
		ResendPackets();
	}
}

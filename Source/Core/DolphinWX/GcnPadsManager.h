#pragma once

#ifndef GSNPADSMANAGER_H
#define GSNPADSMANAGER_H

#include "PadsCommon.h"
#include <vector>

#include <ws2tcpip.h>
#include <winsock2.h>

class GcnPadManager;

class GcnPadsManager
{
public:
	GcnPadsManager();
	~GcnPadsManager();
	std::vector<GcnPadManager *> pads;
	struct addrinfo * result;
	std::vector<SOCKET *> clients;
};

typedef struct GcnPadsMan_SOCKET_t {
	GcnPadsManager * padsManager;
	SOCKET * client;
} GcnPadsMan_SOCKET;

void clientAcceptorThread(void* pParams);
void clientManagerThread(void* pParams);

#endif
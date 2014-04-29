#pragma once

#ifndef GSNPADMANAGER_H
#define GSNPADMANAGER_H

#include "PadsCommon.h"
#include "GCPadStatus.h"
#include "GcnPadsManager.h"
#include <vector>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>

class PadAtFrame
{
public:
	PadAtFrame(int f, SPADStatus * ps) { frame = f; PadStatus = ps; }
	int frame;
	SPADStatus * PadStatus;
};

class GcnPadManager
{
public:
	GcnPadManager(GcnPadsManager * p);
	~GcnPadManager();

	GcnPadsManager * PadsManager;
	std::vector <PadAtFrame *> PadQueue;
	HANDLE PadQueueMutex;

	void PadManipFunction(SPADStatus * PadStatus, int controllerID);
};

void handlePacket(GcnPadsManager * padsManager, char * recvbuf, SOCKET s);
int distanceToChar(char * buff, char v);

#endif
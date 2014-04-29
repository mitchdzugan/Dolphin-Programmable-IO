#include "stdafx.h"
#include "GcnPadManager.h"



#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "Movie.h"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27016"

GcnPadManager::GcnPadManager(GcnPadsManager * p)
{
	PadsManager = p;
	PadQueueMutex = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex
}

GcnPadManager::~GcnPadManager()
{
}


void GcnPadManager::PadManipFunction(SPADStatus * PadStatus, int controllerID)
{
	int current_frame = Movie::g_currentFrame;
	char buff[256];
	
	WaitForSingleObject(PadQueueMutex, INFINITE);
	for (int i = 0; i<PadQueue.size(); i++)
	{
		if (PadQueue[i]->frame == current_frame)
		{
			*PadStatus = *PadQueue[i]->PadStatus;
		}
		if (PadQueue[0]->frame + 2 < current_frame)
		{
			PadQueue.erase(PadQueue.begin() + i);
		}
	}
	buildPacket(buff, current_frame, controllerID, PadStatus);
	ReleaseMutex(PadQueueMutex);

	for (int i = 0; i < PadsManager->clients.size(); i++)
	{
		send(*PadsManager->clients[i], buff, strlen(buff), 0);
	}
}
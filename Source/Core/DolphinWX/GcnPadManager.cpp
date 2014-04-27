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
}

GcnPadManager::~GcnPadManager()
{
}


void GcnPadManager::PadManipFunction(SPADStatus * PadStatus, int controllerID)
{
	int current_frame = Movie::g_currentFrame;
	char buff[255];
	buildPacket(buff, current_frame, controllerID, PadStatus);
	for (int i = 0; i < PadsManager->clients.size(); i++)
	{
		send(*PadsManager->clients[i], buff, strlen(buff), 0);
	}

	while (PadQueue.size() > 0 && PadQueue[0]->frame < current_frame)
	{
		for (int i = 0; i < PadsManager->clients.size(); i++)
			send(*PadsManager->clients[i], "SHIT\n", 5, 0);
		PadQueue.erase(PadQueue.begin());
	}
	if (PadQueue.size() > 0 && PadQueue[0]->frame == current_frame)
	{
		for (int i = 0; i < PadsManager->clients.size(); i++)
			send(*PadsManager->clients[i], "FUCK\n", 5, 0);
		*PadStatus = *PadQueue[0]->PadStatus;
		PadQueue.erase(PadQueue.begin());
	}
}
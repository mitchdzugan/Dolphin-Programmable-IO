#include "stdafx.h"
#include "GcnPadsManager.h"
#include "GcnPadManager.h"

#include <windows.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <cstdlib>
#include <cstdio>
#include <process.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

GcnPadsManager::GcnPadsManager()
{
	pads.push_back(new GcnPadManager(this));
	pads.push_back(new GcnPadManager(this));
	pads.push_back(new GcnPadManager(this));
	pads.push_back(new GcnPadManager(this));

	WSADATA wsaData;
	int iResult;

	result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return;
	}

	_beginthread(clientAcceptorThread, 0, this);
}

GcnPadsManager::~GcnPadsManager()
{
}

void clientAcceptorThread(void* pParams)
{
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;
	SOCKET * ClientSocketPtr = NULL;

	GcnPadsManager * padsManager = (GcnPadsManager *)pParams;
	struct addrinfo * result = padsManager->result;

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);

	while (1)
	{
		iResult = listen(ListenSocket, SOMAXCONN);
		/*if (iResult == SOCKET_ERROR) {
			printf("listen failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			break;
		}*/

		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		/*if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			break;
		}*/
		ClientSocketPtr = new SOCKET(ClientSocket);

		padsManager->clients.push_back(ClientSocketPtr);

		_beginthread(clientManagerThread, 0, new GcnPadsMan_SOCKET({ padsManager, ClientSocketPtr }));
	}

	// No longer need server socket
	closesocket(ListenSocket);
}

void clientManagerThread(void* pParams)
{
	GcnPadsMan_SOCKET * temp = (GcnPadsMan_SOCKET *)pParams;
	GcnPadsManager * padsManager = temp->padsManager;
	SOCKET ClientSocket = *(temp->client);
	delete temp;
	int iResult;

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int distanceToNewline = 0;
	char * recvbufptr = recvbuf;

	// Receive until the peer shuts down the connection
	do {

		memset(recvbuf, 0, DEFAULT_BUFLEN);
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);

		recvbufptr = recvbuf;
		if (iResult > 0)
		{
			while (true)
			{
				distanceToNewline = distanceToChar(recvbufptr, '\n') + 1;
				if (!distanceToNewline)
					break;
				handlePacket(padsManager, recvbufptr, ClientSocket);
				recvbufptr += distanceToNewline;
			}
		}

	} while (1);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();
}

void handlePacket(GcnPadsManager * padsManager, char * recvbuf, SOCKET s)
{
	SPADStatus * ps;
	int semicolonCount = 0;
	for (int i = 0; i < strlen(recvbuf) && recvbuf[i] != '\n'; i++)
	{
		if (recvbuf[i] == ';')
			semicolonCount++;
	}

	if (strstr(recvbuf, "CLEAR"))
	{
		for (int i = 0; i < 4; i++)
		{
			WaitForSingleObject(padsManager->pads[i]->PadQueueMutex, INFINITE);
			padsManager->pads[i]->PadQueue.clear();
			ReleaseMutex(padsManager->pads[i]->PadQueueMutex);
		}
	}
	else if (semicolonCount == 10)
	{
		ps = new SPADStatus();
		padStatusFromPacket(recvbuf, ps);
		int cid = controllerFromPacket(recvbuf);
		if (cid < 4)
		{
			WaitForSingleObject(padsManager->pads[cid]->PadQueueMutex, INFINITE);
			padsManager->pads[cid]->PadQueue.push_back(new PadAtFrame(frameFromPacket(recvbuf), ps));
			ReleaseMutex(padsManager->pads[cid]->PadQueueMutex);
		}
	}
}

int distanceToChar(char * buff, char v)
{
	for (int i = 0; i < strlen(buff); i++) 
	{
		if (buff[i] == v)
			return i;
	}
	return -1;
}
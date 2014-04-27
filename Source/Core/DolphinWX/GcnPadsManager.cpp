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
	int iResult, iSendResult;

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	SPADStatus * ps;

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			ps = new SPADStatus();
			padsManager->pads[controllerFromPacket(recvbuf)]->PadQueue.push_back(new PadAtFrame(frameFromPacket(recvbuf), ps));
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
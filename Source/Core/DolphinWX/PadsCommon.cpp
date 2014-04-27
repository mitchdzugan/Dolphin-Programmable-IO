#include "stdafx.h"
#include "PadsCommon.h"

void buildPacket(char * buffer, int frame, int controller, SPADStatus * padStatus)
{
	sprintf(buffer, "%i;%i;%i;%i;%i;%i;%i;%i;%i;%i;%i\n", frame, controller,
		padStatus->button, padStatus->stickX, padStatus->stickX, padStatus->substickX,
		padStatus->substickY, padStatus->triggerLeft, padStatus->triggerRight, padStatus->analogA, padStatus->analogB);
}

int frameFromPacket(char * buffer)
{
	int frame;
	sscanf(buffer, "%i;", &frame);
	return frame;
}

int controllerFromPacket(char * buffer)
{
	int ignore, controller;
	sscanf(buffer, "%i;%i", &ignore, &controller);
	return controller;
}

void padStatusFromPacket(char * buffer, SPADStatus * padStatus)
{
	int ignore1, ignore2;
	sscanf(buffer, "%i;%i;%i;%i;%i;%i;%i;%i;%i;%i;%i\n", &ignore1, &ignore2,
		&padStatus->button, &padStatus->stickX, &padStatus->stickX, &padStatus->substickX,
		&padStatus->substickY, &padStatus->triggerLeft, &padStatus->triggerRight, &padStatus->analogA, &padStatus->analogB);
}

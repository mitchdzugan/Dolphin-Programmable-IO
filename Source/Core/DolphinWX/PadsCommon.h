#pragma once

#include "GCPadStatus.h"

void buildPacket(char * buffer, int frame, int controller, SPADStatus * padStatus);
int frameFromPacket(char * buffer);
int controllerFromPacket(char * buffer);
void padStatusFromPacket(char * buffer, SPADStatus * padStatus);

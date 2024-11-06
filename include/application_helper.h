#ifndef APPLICATION_HELPER_H
#define APPLICATION_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "link_layer.h"

unsigned char* buildDataPacket(const unsigned char* data, int dataSize, int* packetSize);
unsigned char* buildControlPacket(int controlType, int fileSize, const char* fileName, int* packetSize);
int initializeConnection(LinkLayer* connectionParams);

#endif
#ifndef APPLICATION_HELPER_H
#define APPLICATION_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link_layer.h"

unsigned char * parseControl(const unsigned int c, const char* filename, long int length, unsigned int* size);
unsigned char * openFile(FILE* fd, long int fileLength);
void removeHeaderData(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer);

#endif // APPLICATION_HELPER_H
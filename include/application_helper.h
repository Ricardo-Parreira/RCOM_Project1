#ifndef APPLICATION_HELPER_H
#define APPLICATION_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link_layer.h"

int initializeConnection(LinkLayer *linkLayer, const char *serialPort, const char *role, int baudRate, int nTries, int timeout);
void handleTransmitter(const char *filename);
void handleReceiver(const char *filename);
unsigned char* readFileData(const char *filename, int *fileSize);
void sendDataPackets(unsigned char *data, int fileSize);
void receiveDataPackets(FILE *file);
unsigned char* build_control_packet(int type, int fileSize, const char *filename, int *packetSize);
unsigned char* read_control_packet(int packetSize,int* fileSize);
unsigned char * build_data_packet(unsigned char sequence, unsigned char *data_field, int dataFieldSize);

#endif // APPLICATION_HELPER_H
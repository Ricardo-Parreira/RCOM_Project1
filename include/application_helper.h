#ifndef APPLICATION_HELPER_H
#define APPLICATION_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



unsigned char* build_control_packet(int control_field, int fileSize, const char* fileName, int *controlpacketSize);
unsigned char * read_control_packet(unsigned char* controlpacket,int packetSize,int* fileSize);
unsigned char * build_data_packet(unsigned char sequence, unsigned char *data_field, int dataFieldSize);

#endif // APPLICATION_HELPER_H
#include "application_helper.h"
#include "application_layer.h"
#include "link_layer.h"
#include <sys/stat.h>



unsigned char * parseControl(const unsigned int c, const char* filename, long int length, unsigned int* size){
    unsigned L1 = sizeof(size);
    unsigned L2 = strlen(filename);
    unsigned char *packet = (unsigned char*)malloc(3+L1+L2);
    *size = 3+L1+L2;
    
    unsigned int pos = 0;
    packet[0]=c;
    packet[1]=0;
    packet[2]=L1;

    for (unsigned char i = 0 ; i < L1 ; i++) {
        packet[2+L1-i] = length & 0xFF;
        length >>= 8;
    }
    pos+=L1;
    packet[pos++]=1;
    packet[pos++]=L2;
    memcpy(packet+pos, filename, L2);
    return packet;
}


unsigned char * openFile(FILE* fd, long int fileLength) {
    unsigned char* data = (unsigned char*)malloc(sizeof(unsigned char) * fileLength);
    fread(data, sizeof(unsigned char), fileLength, fd);
    return data;
}

void removeHeaderData(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    memcpy(buffer,packet+4,packetSize-4);
    buffer+=4;
    buffer += packetSize;
}

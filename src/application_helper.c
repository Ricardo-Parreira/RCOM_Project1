#include "application_helper.h"

unsigned char* build_control_packet(int control_field, int fileSize, const char* fileName, int *controlpacketSize) {
    int fileNameLen = strlen(fileName);
    int packetSize = 1 + 2 + sizeof(int) + 2 + fileNameLen;

    *controlpacketSize = packetSize;

    unsigned char *controlpacket = (unsigned char*) malloc(packetSize);
    if (controlpacket == NULL) {
        printf("Memory allocation failed!\n");
        return NULL;
    }

    int offset = 0;

    controlpacket[offset++] = control_field; 

    controlpacket[offset++] = 0x00;              //T1
    controlpacket[offset++] = sizeof(int);       //L1
    memcpy(&controlpacket[offset], &fileSize, sizeof(int)); //V1 (filesize)
    offset += sizeof(int);

    controlpacket[offset++] = 0x01;             //T2
    controlpacket[offset++] = fileNameLen;      //L2
    memcpy(&controlpacket[offset], fileName, fileNameLen);  //V2 (filename)

    return controlpacket;
}

unsigned char * read_control_packet(unsigned char* controlpacket,int packetSize,int* fileSize){
    unsigned char number_bytes_file = controlpacket[2];
    unsigned char bytes_size[number_bytes_file]; 
    memcpy(bytes_size,controlpacket+3,number_bytes_file);
    int result = 0; 
    for (int i = 0; i < number_bytes_file; ++i) {
        result |= bytes_size[i] << (8 * i); 
    }
    *fileSize = result;
    printf("Number of bytes: %d\n", result);
    unsigned char* m = &controlpacket[3+number_bytes_file+2];
    return m;
}

unsigned char * build_data_packet(unsigned char sequence, unsigned char *data_field, int dataFieldSize){
    unsigned char* packet = (unsigned char*)malloc(4 + dataFieldSize);
    if (packet == NULL) {
        printf("[ERROR] Memory allocation\n");
        return NULL; 
    }
    packet[0] = 2;
    packet[1] = sequence;
    packet[2] = dataFieldSize >> 8 & 0xFF;
    packet[3] = dataFieldSize & 0xFF;
    memcpy(packet+4,data_field,dataFieldSize);
    return packet;
}
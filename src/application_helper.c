#include "application_helper.h"

unsigned char * get_control_packet(const unsigned int value, const char* filename, long int length, unsigned int* size){
    //get L1
    int L1 = 0;
    int temp = length;
    while (temp > 0) {
        temp >>= 1;  // Shift right to count the number of bits
        L1++;
    }

    L1 = (L1 + 7) / 8;  

    const int L2 = strlen(filename);
    *size = 1+2+L1+2+L2;
    unsigned char *packet = (unsigned char*)malloc(*size);
    
    packet[0]=value; //C - should either be START or END
    packet[1]=0;     //T1 - 0 for file size
    packet[2]=L1;

    //for each octet of L1 
    for (unsigned char i = 0 ; i < L1 ; i++) {
        packet[2+L1-i] = length & 0xFF;
        length >>= 8;
    }
    unsigned int pos = 2;
    pos+=L1;
    packet[pos++]=1; //T2 - 1 for file name
    packet[pos++]=L2;
    memcpy(packet+pos, filename, L2);
    return packet;
}

unsigned char * get_data_packet(unsigned char sequence, unsigned char * data, int size, int *packetSize){ //pode ser preciso acrescentar um argumento

    unsigned char* packet = (unsigned char*)malloc(size + 3);

    packet[0] = DATA;
    packet[1] = sequence;
    packet[2] = size >> 8 & 0xFF;   //most significant bits
    packet[3] = size & 0xFF;        //least significant bits
    memcpy(packet+4, data, size);

    return packet;
}

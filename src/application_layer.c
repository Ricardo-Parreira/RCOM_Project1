// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort, serialPort);
    linkLayer.role = strcmp(role, "tx") ? LlRx : LlTx;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    int fd = llopen(linkLayer);
    if (fd < 0){
        perror("Connection error");
        exit(-1);
    }
    else (printf("llopen() works\n"));

    // Testing llwrite
    if (linkLayer.role == LlTx) {
        printf("Testing llwrite...\n");

        const char *data = "~Hello~";
        int bytesWritten = llwrite((unsigned char*)data, strlen(data));

        if (bytesWritten < 0) {
            perror("llwrite failed");
        } else {
            printf("llwrite success: %d bytes written\n", bytesWritten);
        }
    }

  

    // Testing llread 
    if (linkLayer.role == LlRx) {
        printf("Testing llread...\n");

        unsigned char packet[MAX_FRAME_SIZE+6];  // Buffer for the received data
        int bytesRead = llread(packet);

        if (bytesRead < 0) {
            perror("llread failed");
        } else {
            printf("llread success: %d bytes read\n", bytesRead);
            printf("Received data: %s\n", packet);  // Assuming it's a string, for simplicity
            printf("[DEBUG] bytesRead: %d \n", bytesRead);
            for (int i = 0; i < bytesRead; i++) {
                printf("%02X ", packet[i]);  // Print as hex
            }
printf("\n");
        }
    }
    int fd1 = llclose(TRUE);
    if (fd1 < 0) {
        perror("Close error\n");
        exit(-1);
    }
    else (printf("close works\n"));


}
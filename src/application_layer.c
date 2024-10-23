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

        const char *data = "Hello";
        int bytesWritten = llwrite(linkLayer,(unsigned char*)data, strlen(data));

        if (bytesWritten < 0) {
            perror("llwrite failed");
        } else {
            printf("llwrite success: %d bytes written\n", bytesWritten);
        }
    }
}

// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort, serialPort);
    linkLayer.role = strcmp(role, "tx") ? LlTx : LlRx;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    

    int fd = llopen(linkLayer);
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    }
    else (printf("llopen works\n"));
    switch(linkLayer.role){
        case LlTx: {
            printf("App layer transmitter works.\n");
            const char *textMessage = "Se as estrelas fossem tão bonitas como tu passava as noites em claro a olhar pro ceu!";
            int messageLength = strlen(textMessage);

            if (llwrite((unsigned char *)textMessage, messageLength) < 0) {
                printf("Error sending the message\n");
            } else {
                printf("Message sent successfully.\n");
            }
            break;
        }
        case LlRx: {
            printf("App layer receiver works.\n");
            unsigned char buffer[BUF_SIZE];
            int bytesRead;
            bytesRead = llread(buffer);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0'; 
                printf("Received message: %s\n", buffer);
            } else {
                printf("Error reading the message or no data received\n");
            }
            break;
        }
        default:
            break;
    }
    int fd1 = llclose(TRUE);
    if (fd1 < 0) {
        perror("Close error\n");
        exit(-1);
    }
    else (printf("close works\n"));
}
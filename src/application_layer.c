// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "application_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LinkLayer connectionParameters;
int sequence = 0;
struct timeval start, end;

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    gettimeofday(&start, NULL);
    // Set connection parameters
    strcpy(connectionParameters.serialPort, serialPort);
    connectionParameters.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    // Open the connection
    int fd = llopen(connectionParameters);
    if (fd < 0) {
        perror("Error opening connection\n");
        return;
    }

    if (connectionParameters.role == LlTx) {
        // Transmitter: Open file and send it
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("Error opening file for reading");
            llclose(fd);
            return;
        }
        else {
            printf("File opened successfully\n");
        }

        // Determine file size
        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Initialize bytesUnsent with the full file size
        int bytesUnsent = fileSize;

        // Send start control packet
        int startPacketSize;
        //const char *fileName = NULL;
        unsigned char *startPacket = buildControlPacket(1, fileSize, filename, &startPacketSize);
        printf("fileName: %s\n", filename);
        if (llwrite(startPacket, startPacketSize) == -1) {
            perror("Error sending start packet");
            //free(startPacket);
            fclose(file);
            llclose(fd);
            return;
        }
        //free(startPacket);

        // Send file data in chunks of up to 512 bytes
        unsigned char dataBuffer[MAX_FRAME_SIZE];
        int packetSize;
        while (bytesUnsent > 0) {
            int chunkSize = (bytesUnsent >= MAX_FRAME_SIZE) ? MAX_FRAME_SIZE : bytesUnsent;
            int bytesRead = fread(dataBuffer, 1, chunkSize, file);
            if (bytesRead <= 0) {
                perror("Error reading file");
                break;
            }

            unsigned char *dataPacket = buildDataPacket(dataBuffer, bytesRead, &packetSize);
            if (llwrite(dataPacket, packetSize) == -1) {
                perror("Error sending data packet");
                //free(dataPacket);
                fclose(file);
                llclose(fd);
                return;
            }

            //free(dataPacket);
            bytesUnsent -= bytesRead;
            printf("Sent %d bytes, %d bytes remaining\n", bytesRead, bytesUnsent);
        }

        // Send end control packet
        int endPacketSize;
        unsigned char *endPacket = buildControlPacket(3, fileSize, filename, &endPacketSize);
        if (llwrite(endPacket, endPacketSize) == -1) {
            perror("Error sending end packet");
        }
        //free(endPacket);

        fclose(file);
        printf("File transmission completed\n");

    } else if (connectionParameters.role == LlRx) {
        // Receiver: Open file and receive it
        FILE *file = fopen(filename, "wb");
        if (!file) {
            perror("Error opening file for writing");
            llclose(fd);
            return;
        }

        unsigned char packet[MAX_FRAME_SIZE];
        int packetSize;
        while (1) {
            packetSize = llread(packet);
            if (packetSize == -1) {
                //skip to the next iteration
                continue;
            }

            if (packet[0] == 1) {
                // Start packet received
                printf("Start packet received\n");
            } else if (packet[0] == 2) {
                // Data packet received: write data to file
                unsigned char *buffer = (unsigned char*)malloc(packetSize - 4);
                
                int payloadSize = packetSize - 6; // Exclude 4 bytes for header and 2 for BCC
                memcpy(buffer, &packet[4], payloadSize);

                fwrite(buffer, 1, packetSize - 6, file); 
                //free(buffer);
                printf("Data packet received and written\n");
            } else if (packet[0] == 3) {
                // End packet received
                printf("End packet received\n");
                break;
            }
        }

        fclose(file);
        printf("File reception completed\n");
    }

    llclose(fd);
}
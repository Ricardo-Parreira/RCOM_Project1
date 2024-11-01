#include "application_layer.h"
#include "link_layer.h"
#include "application_helper.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <sys/stat.h>

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
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    } 
    switch (linkLayer.role) {
        case LlTx: {
            FILE *file = fopen(filename, "rb");
            if (file == NULL) {
                perror("Error opening file\n");
                exit(-1);
            }
            struct stat st;
            if (stat(filename, &st) != 0) {
                perror("Error calculating file size\n");
                exit(-1);
            }
            int fileSize = st.st_size;
            int startPacketSize;
            unsigned char *startControlPacket = build_control_packet(1, fileSize, filename, &startPacketSize);
            if (startControlPacket == NULL) {
                perror("Error building start control packet\n");
                exit(-1);
            }
            if (llwrite(startControlPacket, startPacketSize) < 0) {
                printf("Error sending control packet (START)\n");
                free(startControlPacket); 
                exit(-1);
            } else {
                printf("Control packet (START) sent successfully.\n");
            }
            free(startControlPacket);
            unsigned char sequence = 0;
            unsigned char * data = (unsigned char*)malloc(fileSize);
            fread(data, 1, fileSize, file);
            int bytes_read = 0;
            while(bytes_read < fileSize){
                int buf_size = 0;
                if ((fileSize-bytes_read)<MAX_PAYLOAD_SIZE) buf_size = fileSize-bytes_read;
                else buf_size = MAX_PAYLOAD_SIZE;
                unsigned char* buf = (unsigned char*) malloc(buf_size);
                memcpy(buf,data,buf_size);
                unsigned char* dataPacket = build_data_packet(sequence,buf,buf_size);
                 if(llwrite(dataPacket, 4+buf_size) == -1) {
                    printf("Exit writting data!\n");
                    exit(-1);
                }
                sequence = (sequence+1)%99; 
                bytes_read += buf_size;
                data += buf_size;
            }
            fclose(file); 
            int endPacketSize;
            unsigned char *endControlPacket = build_control_packet(3, fileSize, filename, &endPacketSize);
            if (endControlPacket == NULL) {
                perror("[ERROR] In building END control packet\n");
                exit(-1);
            }
            if (llwrite(endControlPacket, endPacketSize) < 0) {
                printf("[Error] In Sending control packet (END)\n");
                free(endControlPacket); 
                exit(-1);
            } else {
                printf("Control packet (END) sent successfully!\n");
            }
            free(endControlPacket); 
            break;
        }
        case LlRx: {
            unsigned char buffer[MAX_PAYLOAD_SIZE];
            int bytesRead = -1;
            int TransmissionTerminated = 0;
            while((bytesRead = llread(buffer))<0);
            int fileSize = 0;
            unsigned char* fileName = read_control_packet(buffer, bytesRead, &fileSize); 
            FILE *file = fopen((char*)filename, "wb");
            if (file == NULL) {
                perror("Error opening file\n");
                exit(-1);
            }
            while (!TransmissionTerminated) {
                while(1){
                    bytesRead = llread(buffer);
                    if(bytesRead > 0)break;
                }
                if (bytesRead < 0) {
                    printf("[ERROR] Error receiving data\n");
                    exit(-1);
                }
                if (buffer[0] == 3) { 
                    printf("Received END control packet\n");
                    printf("Filename: %s, Size: %d bytes\n", fileName, fileSize); 
                    TransmissionTerminated = 1;
                } else {
                    unsigned char* buf = (unsigned char*) malloc(bytesRead);
                    memcpy(buf,buffer+4,bytesRead-4);
                    fwrite(buf, 1, bytesRead-4, file);
                    free(buf);
                    printf("Received data packet\n");
                }
            }

            break;
        }
        default:
            break;
    }
    int result = llclose( 1);
    if (result < 0) {
        perror("[ERROR] Closing \n");
        exit(-1);
    } 
}


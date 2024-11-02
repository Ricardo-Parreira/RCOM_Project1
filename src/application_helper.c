#include "application_helper.h"
#include <sys/stat.h>


int initializeConnection(LinkLayer *linkLayer, const char *serialPort, const char *role, int baudRate, int nTries, int timeout) {
    strcpy(linkLayer->serialPort, serialPort);
    linkLayer->role = strcmp(role, "tx") == 0 ? LlTx : LlRx;
    linkLayer->baudRate = baudRate;
    linkLayer->nRetransmissions = nTries;
    linkLayer->timeout = timeout;
    
    int fd = llopen(*linkLayer);
    if (fd < 0) {
        perror("Connection error\n");
    }
    return fd;
}

// Tx handling
void handleTransmitter(const char *filename) {
    int fileSize;
    unsigned char *fileData = readFileData(filename, &fileSize);
    if (fileData == NULL) {
        exit(-1);
    }

    int startPacketSize;
    unsigned char *startControlPacket = build_control_packet(1, fileSize, filename, &startPacketSize);
    if (startControlPacket == NULL || llwrite(startControlPacket, startPacketSize) < 0) {
        perror("Error sending control packet (START)\n");
        exit(-1);
    }
    printf("Control packet (START) sent successfully.\n");


    sendDataPackets(fileData, fileSize);


    int endPacketSize;
    unsigned char *endControlPacket = build_control_packet(3, fileSize, filename, &endPacketSize);
    if (endControlPacket == NULL || llwrite(endControlPacket, endPacketSize) < 0) {
        perror("Error sending control packet (END)\n");

        exit(-1);
    }
    printf("Control packet (END) sent successfully.\n");

}

// Rx handling
void handleReceiver(const char *filename) {
    unsigned char buffer[MAX_PAYLOAD_SIZE];
    int bytesRead = llread(buffer);

    if (bytesRead < 0) {
        perror("[ERROR] Initial control packet reception failed\n");
        exit(-1);
    }

    int fileSize = 0;
    unsigned char *fileName = read_control_packet(buffer, bytesRead, &fileSize);
    //printf("fileNmae: %s\n", fileName); ///fileName is catching weird characters

    FILE *file = fopen((char*)filename, "wb");
    if (file == NULL) {
        perror("Error opening file\n");
        exit(-1);
    }

    receiveDataPackets(file);

    fclose(file);
}

unsigned char* readFileData(const char *filename, int *fileSize) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file\n");
        return NULL;
    }
    
    struct stat st;
    if (stat(filename, &st) != 0) {
        perror("Error calculating file size\n");
        fclose(file);
        return NULL;
    }
    
    *fileSize = st.st_size;
    unsigned char *data = (unsigned char*)malloc(*fileSize);
    fread(data, 1, *fileSize, file);
    fclose(file);
    return data;
}


void sendDataPackets(unsigned char *data, int fileSize) {
    int bytesSent = 0;
    unsigned char sequence = 0;

    while (bytesSent < fileSize) {
        int bufSize;
        // Chopping data into MAX_PAYLOAD_SIZE chunks
        if ((fileSize - bytesSent) < MAX_PAYLOAD_SIZE) {
            bufSize = fileSize - bytesSent;
        } else {
            bufSize = MAX_PAYLOAD_SIZE;
        }
        unsigned char *buf = (unsigned char*)malloc(bufSize);
        memcpy(buf, data + bytesSent, bufSize);

        unsigned char *dataPacket = build_data_packet(sequence, buf, bufSize);
        if (llwrite(dataPacket, 4 + bufSize) < 0) { // 4 bytes for header
            printf("[ERROR] Failed writing data packet.\n");

            exit(-1);
        }

        sequence = (sequence + 1) % 99;
        bytesSent += bufSize;

    }
}


void receiveDataPackets(FILE *file) {
    unsigned char buffer[MAX_PAYLOAD_SIZE];
    int bytesRead;
    int transmissionTerminated = 0;

    while (!transmissionTerminated) {
        bytesRead = llread(buffer);
        
        if (bytesRead < 0) {
            perror("[ERROR] Error receiving data\n");
            exit(-1);
        }

        if (buffer[0] == 3) { // END control packet
            printf("Received END control packet\n");
            transmissionTerminated = 1;
        } else {
            fwrite(buffer + 4, 1, bytesRead - 4, file);
            printf("Received data packet\n");
        }
    }
}


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
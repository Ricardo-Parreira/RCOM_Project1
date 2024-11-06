#include "application_helper.h"


extern int sequence;
unsigned char* buildDataPacket(const unsigned char* data, int dataSize, int* packetSize) {
    unsigned char* packet = (unsigned char*)malloc(dataSize + 6);
    packet[0] = 2;
    packet[1] = sequence;
    packet[2] = (dataSize >> 8)  & 0xFF; //most significant byte     
    packet[3] = dataSize & 0xFF; //least significant byte

    for (int i = 0; i < dataSize; i++) {
        packet[i+4] = data[i];
    }
    *packetSize = dataSize + 6;
    sequence = (sequence+1) % 100; //sequence [0, 99]
    return packet;
}

unsigned char* buildControlPacket(int controlType, int fileSize, const char* fileName, int* packetSize) {

    int fileNameLength = strlen(fileName);
    int totalSize = 7 + fileNameLength; 

    unsigned char* packet = (unsigned char*)malloc(totalSize);

    packet[0] = controlType;    //(1 for START, 3 for END)

    packet[1] = 0;                   // T1
    packet[2] = 2;                   // L1 - Length of file size field (2 bytes)
    packet[3] = (fileSize >> 8) & 0xFF;  // MSB of file size
    packet[4] = fileSize & 0xFF;         // LSB of file size

    packet[5] = 1;                   // T2
    packet[6] = fileNameLength;      // L2
    memcpy(&packet[7], fileName, fileNameLength); // V2 - file name

    *packetSize = totalSize; 
    return packet;
}

int initializeConnection(LinkLayer* connectionParams) {
    int fd = open(connectionParams->serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("ERROR opening serial port");
        return -1;
    }

    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio));

    cfsetispeed(&newtio, connectionParams->baudRate);
    cfsetospeed(&newtio, connectionParams->baudRate);
    newtio.c_cflag = connectionParams->baudRate | CS8 | CLOCAL | CREAD;

    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set non-canonical mode and timeout
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = connectionParams->timeout * 10; // Convert seconds to tenths of seconds
    newtio.c_cc[VMIN] = 0; //minimum number of characters to read (if it wasnt like this it would be stuck on a loop)

    // Apply configuration to the serial port
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) != 0) {
        perror("ERROR setting serial port attributes");
        close(fd);
        return -1;
    }

    return fd;
}

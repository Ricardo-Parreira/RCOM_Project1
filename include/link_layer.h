// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_


extern int alarmCount;
extern int alarmEnabled;



typedef enum
{
    START,
    READ,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC1_OK,
    DATA,
    STOP
} LinkLayerState;


typedef enum
{
    LlTx,
    LlRx
} LinkLayerRole;




typedef struct
{
    LinkLayerRole role;
    char serialPort[50];
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 256

#define MAX_FRAME_SIZE 512

// MISC
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

// Additional helper functions
int byteStuffing(const unsigned char *frame, int frameSize, unsigned char *stuffedData);
int byteDeStuffing(const unsigned char *stuffedData, int stuffedSize, unsigned char *dest);
unsigned char readAckFrame(int fd);

#endif // _LINK_LAYER_H_
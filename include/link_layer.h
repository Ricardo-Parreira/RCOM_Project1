// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#define MAX_BUF_SIZE 256
#define FLAG 0x7E
#define ESC_BYTE 0x7D
#define Awrite 0x03 
#define Aread 0x01
#define Cset 0x03
#define Cua 0x07
#define C_RR_0 0xAA
#define C_RR_1 0xAB
#define C_REJ_0 0x54
#define C_REJ_1 0x55
#define C_DISC 0x0B
#define I_FRAME_0 0x00
#define I_FRAME_1 0x80


typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;
typedef enum
{
    INIT,
    FLAG_RECEBIDO,
    A_RECEBIDO,
    C_RECEBIDO,
    BCC_RECEBIDO, 
    LIDO,
} State;
typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 512

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

unsigned char *byteStuffing(const unsigned char *data, int dataSize, int *newSize);

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

void startClock();

double endClock() ;

#endif // _LINK_LAYER_H_
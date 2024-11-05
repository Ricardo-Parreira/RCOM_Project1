// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_


extern int alarmCount;
extern int alarmEnabled;



typedef enum
{
    START,
    FLAG_RECEIVED,
    A_RECEIVED,
    C_RECEIVED,
    ESC_RECEIVED,
    BCC1,
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



#define FLAG 0x7E

#define Awrite 0x03
#define CSet 0x03
#define BCC1w (Awrite ^ CSet)

//disconnect 
#define DISC 0x0B

#define Aread 0x01
#define CUA 0x07
#define BCC1r (Aread ^ CUA)
#define BCC1_DISC (Awrite ^ DISC)
#define C_I(n) n << 7
#define C_RR(n) (0xAA | n)
#define C_REJ(n) (0x54 | n) 

#define MAX_FRAME_SIZE 512

//destuffing
#define ESCAPE_BYTE 0x7D
#define STUFFING_MASK 0x20

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
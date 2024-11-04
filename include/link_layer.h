// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>


typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

typedef enum
{
   START,
   FLAG_RCV,
   A_RCV,
   C_RCV,
   BCC_OK,
   STOP,
   DATA,
   STUFFEDBYTES,
} LinkLayerStateMachine;


// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 512

// MISC
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5


//Frame constants
#define FLAG 0x7E
#define ESC 0x7D
#define A_ER 0x03
#define A_RE 0x01
#define C_SET 0x03
#define C_DISC 0x0B
#define C_UA 0x07
#define C_RR(Nr) (Nr  | 0xAB)
#define C_REJ(Nr) (Nr | 0x54)
#define C_N(Ns) (Ns << 7)



//Global variables
//volatile int STOP_R = FALSE;



// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(int fd, const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(int fd, unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

//Receiver state machine to validate SET
void receiver_state_machine(int fd, LinkLayerStateMachine current_state);

//Transmiter state machine to validate UA
LinkLayerStateMachine transmiter_state_machine(int fd,LinkLayerStateMachine current_state);

//State machine to process answer's control frame
int control_frame_state_machine(int fd, unsigned char byte,LinkLayerStateMachine current_state);

//Alarm handler
void alarmHandler();

int set_serial_port(LinkLayer connectionParameters);

void startClock();

double endClock();


#endif // _LINK_LAYER_H_
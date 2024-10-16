// Link layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 6

#define FLAG 0x7E

#define Awrite 0x03
#define CSet 0x03
#define BCC1w (Awrite ^ CSet)

#define Aread 0x01
#define CUA 0x07
#define BCC1r (Aread ^ CUA)

int alarmEnabled = FALSE;
int alarmCount = 0;


// Alarm function handler
void alarmHandler(int signal)
{   
    printf("Alarm triggered!\n");
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////


unsigned char byte;

int llopen(LinkLayer connectionParameters){

    LinkLayerState state = START;

    int fd = openSerialPort(connectionParameters.serialPort,connectionParameters.baudRate);
    if (fd < 0) return -1;


    switch (connectionParameters.role)
    {
    case LlTx:
        (void)signal(SIGALRM, alarmHandler);

        while(connectionParameters.nRetransmissions != 0){

            //Send the supervision frame
            unsigned char supFrame[5] = {FLAG, Awrite, CSet, BCC1w, FLAG};
            write(fd, supFrame, 5);
            
            
            alarm(connectionParameters.timeout);
            alarmEnabled = FALSE;

            while(alarmEnabled == FALSE){

                if (read(fd, &byte, 1) > 0){
                switch (state)
                {
                case START:
                    if (byte == FLAG) state = FLAG1;
                    break;
                
                case FLAG1:
                    if (byte == Awrite) state = A;
                    else if(byte != FLAG) state = START;
                    break;

                case A:
                    if (byte == CSet) state = C;
                    else if(byte == FLAG) state = FLAG1;
                    else state = START;
                    break;

                case C:
                    if (byte == (BCC1w)) state = BCC;
                    else if(byte == FLAG) state = FLAG1;
                    else state = FLAG;
                    break;

                case BCC:
                    if (byte == FLAG) state = READ;
                    else if(byte != (BCC1w)) state = START;
                    break;
                
                default:
                    break;
                }
                }
            }
            connectionParameters.nRetransmissions --;
        }

        if (state != READ) return -1;
        break;
    
    case LlRx: 

        while(state != READ){

                if (read(fd, &byte, 1) > 0){
                switch (state)
                {
                case START:
                    if (byte == FLAG) state = FLAG1;
                    break;
                
                case FLAG1:
                    if (byte == Aread) state = A;
                    else if(byte != FLAG) state = START;
                    break;

                case A:
                    if (byte == CUA) state = C;
                    else if(byte == FLAG) state = FLAG1;
                    else state = START;
                    break;

                case C:
                    if (byte == (BCC1r)) state = BCC;
                    else if(byte == FLAG) state = FLAG1;
                    else state = FLAG;
                    break;

                case BCC:
                    if (byte == FLAG) state = READ;
                    else state = START;
                    break;
                
                default:
                    break;
                }
            }
        }
        //Send the supervision frame
        unsigned char supFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, supFrame, 5);
        break;

    default:
        return -1;
        break;
    }

    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}

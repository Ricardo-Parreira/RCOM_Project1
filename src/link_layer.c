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

#define MAX_FRAME_SIZE 512

//destuffing
#define ESCAPE_BYTE 0x7D
#define STUFFING_MASK 0x20



LinkLayerRole role;
extern int fd;
int alarmEnabled = 0;
int alarmCount = 0;

char bitTx = 0;
int retransmissions = 0;
int timeout = 3;


// Alarm function handler
void alarmHandler(int signal)
{   
    printf("Alarm triggered!\n");
    alarmEnabled = 1;//TRUE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

void setConnectionParameters(LinkLayer connectionParameters) {
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    role = connectionParameters.role;
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////


unsigned char byte;

int llopen(LinkLayer connectionParameters){
    alarmCount=0;

    LinkLayerState state = START;
    setConnectionParameters(connectionParameters);

    if (fd < 0) return -1;

    // Configure signal handler for retransmission
    signal(SIGALRM, alarmHandler);
    

    // Role-based behavior
    if (role == LlTx) {
        while (retransmissions > 0) {
            unsigned char supFrame[5] = {FLAG, Awrite, CSet, BCC1w, FLAG};
            write(fd, supFrame, 5);
            alarm(timeout);
            alarmEnabled = 0;

            while (!alarmEnabled && state != READ) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            state = (byte == FLAG) ? FLAG_RCV : START;
                            break;
                        case FLAG_RCV:
                            state = (byte == Aread) ? A_RCV : (byte == FLAG ? FLAG_RCV : START);
                            break;
                        case A_RCV:
                            state = (byte == CUA) ? C_RCV : (byte == FLAG ? FLAG_RCV : START);
                            break;
                        case C_RCV:
                            state = (byte == BCC1r) ? BCC1_OK : START;
                            break;
                        case BCC1_OK:
                            state = (byte == FLAG) ? READ : START;
                            break;
                        default:
                            break;
                    }
                }
            }
            if (state == READ) return fd;
            retransmissions--;
        }
        return -1;  // Failed to connect as transmitter
    } else if (role == LlRx) {
        while (state != READ) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        state = (byte == FLAG) ? FLAG_RCV : START;
                        break;
                    case FLAG_RCV:
                        state = (byte == Awrite) ? A_RCV : (byte == FLAG ? FLAG_RCV : START);
                        break;
                    case A_RCV:
                        state = (byte == CSet) ? C_RCV : (byte == FLAG ? FLAG_RCV : START);
                        break;
                    case C_RCV:
                        state = (byte == BCC1w) ? BCC1_OK : START;
                        break;
                    case BCC1_OK:
                        state = (byte == FLAG) ? READ : START;
                        break;
                    default:
                        break;
                }
            }
        }
        unsigned char supFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, supFrame, 5);
        return fd;
    }
    return -1;
}

// -------- AUXILIARY TO LLWRITE 
int byteStuffing(const unsigned char *frame, int frameSize, unsigned char *stuffedData){
    int j = 0;
    for (int i = 0; i < frameSize; i++) {
        if (frame[i] == 0x7E) {
            stuffedData[j] = 0x7D;
            j++;
            stuffedData[j] = 0x5E;
            j++;
        } 
        else if (frame[i] == 0x7D) {
            stuffedData[j] = 0x7D;
            j++;
            stuffedData[j] = 0x5D;
            j++;
        } 
        else {
            stuffedData[j] = frame[i];
            j++;
        }
    }
    return j;
}

//máquina de estados para ler o acknoldege frame    
unsigned char readAckFrame(int fd){

    unsigned char byte, cByte = 0;
    LinkLayerState state = START;
    
    while (state != READ && alarmEnabled == FALSE) {  
        if (read(fd, &byte, 1) > 0) {
            //printf("byte: %02X \n", byte);
            switch (state) {
                case START:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;

                case FLAG_RCV:
                    if (byte == Aread) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;

                case A_RCV:
                    if (byte == 0xAA || byte == 0xAB || byte == 0x54 || byte == 0x55 || byte == DISC) {
                        state = C_RCV;
                        
                        cByte = byte; // Store the control byte
                    } else if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    
                    if (byte == (Aread ^ cByte)) {
                        state = BCC1_OK;
                    } else if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case BCC1_OK:
                printf("a puta passou \n");
                    if (byte == FLAG) {
                        state = READ;
                    } else {
                        state = START;
                    }
                    break;

                default:
                    break;
            }
        }
    }
    printf("byte passado: %02X \n", cByte);
    return (state == READ) ? cByte : 0;
}


int byteDeStuffing(const unsigned char *stuffedData, int stuffedSize, unsigned char *dest) {

    int j = 0; // Index for dest (de-stuffed data)

    for (int i = 0; i < stuffedSize; i++) {
        if (stuffedData[i] == 0x7D) { // Check for escape byte
            
            if (stuffedData[i+1] == 0x5E) {
                dest[j] = 0x7E; 
                i++;
            } 
            else if (stuffedData[i+1] == 0x5D) {
                dest[j] = 0x7D; 
                i++;
            }
        } 
        else {
            dest[j] = stuffedData[i]; // Copy normal byte
        }
        j++;
    }

   return j; // Return the size of de-stuffed data
}


int llwrite(const unsigned char *buf, int bufSize)
{
    //unsigned char frame[MAX_FRAME_SIZE];
    //int fd = openSerialPort(connectionParameters.serialPort,connectionParameters.baudRate);
    //if (fd < 0) return -1;
    int frameSize = bufSize + 6;
    unsigned char *frame = (unsigned char*) malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = Awrite;
    frame[2] = bitTx << 7; //nao sei se é 6 ou 7 | later bitTx = (bitTx++)%2
    frame[3] = frame[1] ^ frame[2];


    signal(SIGALRM, alarmHandler);

    
    //Passa todos os bytes do buf para o frame (a partir do byte 4) 
    for (int i = 0; i < bufSize; i++){
        frame[i+4] = buf[i];
    }

    //calcula o bcc2
    u_int8_t bcc2 = buf[0];
    for(int i = 1; i< bufSize; i++){
        bcc2 = bcc2 ^ buf[i];
    }

    //STUFFING (swapping 0x7E for 0x7D 0x5E and 0x7D for 0x7D 0x5D)
    unsigned char stuffedFrame[MAX_PAYLOAD_SIZE*2 + 6];
    int stuffedFrameSize = byteStuffing(frame+1, frameSize-2, stuffedFrame);

    memcpy(frame + 4, stuffedFrame, stuffedFrameSize);
    frame[stuffedFrameSize+4] = bcc2;
    frame[stuffedFrameSize+5] = FLAG;

    int aceite = 0;
    int rejeitado = 0;
    int transmission = 0;
    
    while (transmission < retransmissions) {
        alarmEnabled = FALSE;
        //alarm(timeout);
        aceite = 0;
        rejeitado = 0;

        while (!alarmEnabled && !rejeitado && !aceite) {
            write(fd, frame, stuffedFrameSize + 6);
            unsigned char check = readAckFrame(fd);
            printf("check: %02X\n", check);
            if (check == 0x54 || check == 0x55) {
                rejeitado = 1;
            } else if (check == 0xAA || check == 0xAB) {
                aceite = 1;
                bitTx = (bitTx + 1) % 2;
            } else {
                continue;
            }
        }
        if (aceite) break;
        transmission++;
    }

    //free(frame);

    if (aceite){
     printf("[INFO] Foi aceite! frameSize: %d \n", frameSize);
     return frameSize;
    }

    printf("Sending frame: ");
    for (int i = 0; i < bufSize; i++) {
        printf("%02X ", frame[i]);
    }
    printf("\n");

    int bytesWritten = write(fd, frame, bufSize);
    if (bytesWritten < 0) {
        perror("Write error");
        return -1;
    }
    return bytesWritten;  // Return number of bytes written
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char frame[MAX_FRAME_SIZE];
    LinkLayerState state = START;
    unsigned char byte;
    int dataIndex = 0;
    

    while (state != STOP) {
        if (read(fd, &byte, 1) > 0) {
            printf("Read byte: %02X, State: %d\n", byte, state); // Debug print
            switch (state) {
            case START:
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (byte == Awrite) state = A_RCV;
                else if (byte != FLAG) state = START;
                break;
            case A_RCV:
                if ((byte == 0x00) || (byte == 0x40)) {
                    state = C_RCV;
                } else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case C_RCV:
                if (byte == (Awrite ^ 0x00) || byte == (Awrite ^ 0x40)) {
                    state = BCC1_OK;
                } else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case BCC1_OK:
                if (byte == FLAG) {
                    state = STOP;
                } else {
                    frame[dataIndex++] = byte;
                }
                break;
            case READ:
            case DATA:
            case STOP:
                
                break;
            }
        }
    }

    // Desfazer byte stuffing
    int deStuffedSize = byteDeStuffing(frame, dataIndex, packet);

    if (deStuffedSize < 0) {
        // Handle error in de-stuffing
        unsigned char rej[5] = {FLAG, Aread, 0x54, Aread ^ 0x54, FLAG};
        write(fd, rej, 5);
        return -1;
    }

    // Calculate BCC2 for the de-stuffed data
    unsigned char calculatedBCC2 = 0;
    for (int i = 0; i < deStuffedSize - 1; i++) {
        calculatedBCC2 ^= packet[i];
    }

    if (calculatedBCC2 != packet[deStuffedSize - 1]) {
        // Send REJ if BCC2 does not match
        unsigned char rej[5] = {FLAG, Aread, 0x54, Aread ^ 0x54, FLAG};
        write(fd, rej, 5);
        return -1;
    } else {
        // Send RR if BCC2 matches
        unsigned char rr[5] = {FLAG, Aread, 0xAA, Aread ^ 0xAA, FLAG};
        write(fd, rr, 5);
    }

    // Return the size of the de-stuffed data excluding the BCC2 byte
    return deStuffedSize - 1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    LinkLayerState state = START;
    unsigned char byte;
    alarmCount = 0;
    

    if (role == LlTx) {  // Transmitter (Tx)
        int retries = retransmissions;
        while (retries > 0) {
            // send frame DISC
            unsigned char discFrame[5] = {FLAG, Awrite, DISC, BCC1_DISC, FLAG};
            write(fd, discFrame, 5);
            alarm(timeout);
            alarmEnabled = 0;

            // waiting for UA from Rx
            while (!alarmEnabled && state != READ) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            state = (byte == FLAG) ? FLAG_RCV : START;
                            break;
                        case FLAG_RCV:
                            state = (byte == Aread) ? A_RCV : (byte == FLAG ? FLAG_RCV : START);
                            break;
                        case A_RCV:
                            state = (byte == CUA) ? C_RCV : START;
                            break;
                        case C_RCV:
                            state = (byte == BCC1r) ? BCC1_OK : START;
                            break;
                        case BCC1_OK:
                            state = (byte == FLAG) ? READ : START;
                            break;
                        default:
                            break;
                    }
                }
            }
            if (state == READ) {
                close(fd);
                if (showStatistics) {
                    printf("Connection closed successfully. Statistics: Retransmissions: %d, Alarms: %d\n", retransmissions, alarmCount);
                }
                return 1;
            }
            retries--;
        }
        return -1;  // fail to close
    } else if (role == LlRx) {  // Receiver (Rx)
        // wait to receive frame DISC do Tx
        while (state != READ) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        state = (byte == FLAG) ? FLAG_RCV : START;
                        break;
                    case FLAG_RCV:
                        state = (byte == Awrite) ? A_RCV : (byte == FLAG ? FLAG_RCV : START);
                        break;
                    case A_RCV:
                        state = (byte == DISC) ? C_RCV : START;
                        break;
                    case C_RCV:
                        state = (byte == BCC1_DISC) ? BCC1_OK : START;
                        break;
                    case BCC1_OK:
                        state = (byte == FLAG) ? READ : START;
                        break;
                    default:
                        break;
                }
            }
        }

        // send frame UA
        unsigned char uaFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, uaFrame, 5);

        close(fd);
        if (showStatistics) {
            printf("Connection closed successfully. Statistics: Retransmissions: %d, Alarms: %d\n", retransmissions, alarmCount);
        }
        return 1;
    }

    return -1;  // Caso de erro
}
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

            while (!alarmEnabled && state != STOP) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            state = (byte == FLAG) ? FLAG_RECEIVED : START;
                            break;
                        case FLAG_RECEIVED:
                            state = (byte == Aread) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                            break;
                        case A_RECEIVED:
                            state = (byte == CUA) ? C_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                            break;
                        case C_RECEIVED:
                            state = (byte == BCC1r) ? BCC1 : START;
                            break;
                        case BCC1:
                            state = (byte == FLAG) ? STOP : START;
                            break;
                        default:
                            break;
                    }
                }
            }
            if (state == STOP) return fd;
            retransmissions--;
        }
        return -1;  // Failed to connect as transmitter
    } else if (role == LlRx) {
        while (state != STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        state = (byte == FLAG) ? FLAG_RECEIVED : START;
                        break;
                    case FLAG_RECEIVED:
                        state = (byte == Awrite) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                        break;
                    case A_RECEIVED:
                        state = (byte == CSet) ? C_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                        break;
                    case C_RECEIVED:
                        state = (byte == BCC1w) ? BCC1 : START;
                        break;
                    case BCC1:
                        state = (byte == FLAG) ? STOP : START;
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

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

int llwrite(const unsigned char *buf, int bufSize)
{
    unsigned char frame[MAX_FRAME_SIZE*2+6];
    //int fd = openSerialPort(connectionParameters.serialPort,connectionParameters.baudRate);
    //if (fd < 0) return -1;
    int frameSize = bufSize + 6;
    //unsigned char *frame = (unsigned char*) malloc(frameSize);
    frame[0] = FLAG;
    frame[1] = Awrite;
    frame[2] = C_I(bitTx);
    frame[3] = frame[1] ^ frame[2];


    signal(SIGALRM, alarmHandler);



    //STUFFING (swapping 0x7E for 0x7D 0x5E and 0x7D for 0x7D 0x5D)
    int index = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE_BYTE) {
            frame[index++] = ESCAPE_BYTE;
            frame[index++] = buf[i] ^ STUFFING_MASK;
        } else {
            frame[index++] = buf[i];
        }
    }

    //calcula o bcc2
    u_int8_t bcc2 = buf[0];
    for(int i = 1; i< bufSize; i++){
        bcc2 = bcc2 ^ buf[i];
    }


    printf("index: %d\n", index);
    frame[index++] = bcc2;
    printf("bcc2: %d \n", bcc2);
    frame[index++] = FLAG;
    int stuffedFrameSize = index;

    printf("[DEBUG] frame \n");
    for (int i = 0; i < stuffedFrameSize; i++) {
        
        printf("%02X ", frame[i]);
    }

    int aceite = 0;
    int rejeitado = 0;
    int transmission = 0;
    
    while (transmission < retransmissions) {
        alarmEnabled = FALSE;
        alarm(timeout);
        aceite = 0;
        rejeitado = 0;

        while (!alarmEnabled && !rejeitado && !aceite) {
            write(fd, frame, stuffedFrameSize);
            unsigned char byte, cByte = 0;
            LinkLayerState state = START;

                //máquina de estados para checkar a resposta
                while (state != STOP && alarmEnabled == FALSE) {  
                    if (read(fd, &byte, 1) > 0) {
                        printf("byte: %02X \n", byte);
                        switch (state) {
                            case START:
                                if (byte == FLAG) state = FLAG_RECEIVED;
                                break;

                            case FLAG_RECEIVED:
                                if (byte == Aread) state = A_RECEIVED;
                                else if (byte != FLAG) state = START;
                                break;

                            case A_RECEIVED:
                                if ((byte == C_RR(bitTx)) | (byte == C_REJ(bitTx))) {
                                    state = C_RECEIVED;
                                    
                                    cByte = byte; // Store the control byte
                                } else if (byte == FLAG) {
                                    state = FLAG_RECEIVED;
                                } else {
                                    state = START;
                                }
                                break;

                            case C_RECEIVED:
                                
                                if (byte == (Aread ^ cByte)) {
                                    state = BCC1;
                                } else if (byte == FLAG) {
                                    state = FLAG_RECEIVED;
                                } else {
                                    state = START;
                                }
                                break;

                            case BCC1:
                            printf("a puta passou \n");
                                if (byte == FLAG) {
                                    state = STOP;
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
            if (cByte == C_REJ(bitTx)) {
                rejeitado = 1;
            } else if (cByte == C_RR(bitTx)) {
                aceite = 1;
                bitTx = (bitTx + 1) % 2;
            } else {
                continue;
            }
        }
        if (aceite) break;
        transmission++;
    }

    //free(frame); cant uncomment this or it fails

    if (aceite) return frameSize;
    if (rejeitado) printf("[ERROR] Frame rejected, BCC2 mismatch\n");
    
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{

    unsigned char packet_copy[MAX_FRAME_SIZE+6];
    LinkLayerState state = START;
    unsigned char byte;
    int dataIndex = 0;
    

    while (state != STOP) {
        if (read(fd, &byte, 1) > 0) {
            printf("Read byte: %02X, State: %d\n", byte, state); // Debug print
            switch (state) {
            case START:
                if (byte == FLAG) state = FLAG_RECEIVED;
                break;
            case FLAG_RECEIVED:
                if (byte == Awrite) state = A_RECEIVED;
                else if (byte != FLAG) state = START;
                break;
            case A_RECEIVED:
                if (byte == C_I(bitTx)) {
                    state = C_RECEIVED;
                } else if (byte == FLAG) state = FLAG_RECEIVED;
                else state = START;
                break;
            case C_RECEIVED:
                if (byte == (Awrite ^ C_I(bitTx))) {
                    state = DATA;
                } else if (byte == FLAG) state = FLAG_RECEIVED;
                else state = START;
                break;
            case DATA:
                if (byte == FLAG) {
                    state = STOP;
                } else {
                    if(byte==ESCAPE_BYTE){
                        state = ESC_RECEIVED;
                    }
                    else{
                        packet_copy[dataIndex++] = byte;
                    }
                }
                break;
            case ESC_RECEIVED:
                if(byte == 0x5E){
                    packet_copy[dataIndex++] = FLAG;
                }
                else if(byte == 0x5D){
                    packet_copy[dataIndex++] = ESCAPE_BYTE;
                }
                else{
                    packet_copy[dataIndex++] = byte;
                }
                state = DATA;
                break;
            case STOP:
                
                break;  
            case BCC1:
                break;
            }
        }
    }   

    int deStuffedSize = dataIndex;

    printf("[DEBUG] De-stuffed data: \n");
    for(int i = 0; i < deStuffedSize; i++){
        printf("packet_copy[%d]: %02X\n", i, packet_copy[i]);
    }

    if (deStuffedSize < 0) {
        // Handle error in de-stuffing
        unsigned char rej[5] = {FLAG, Aread, C_REJ(bitTx), Aread ^ C_REJ(bitTx), FLAG};
        write(fd, rej, 5);
        return -1;
    }

    //put the data in the packet excluding the bcc2
    for (int i = 0; i < deStuffedSize - 1; i++) {
        packet[i] = packet_copy[i];
    }

    for(int i = 0; i < deStuffedSize -1; i++){
        printf("packet[%d]: %02X\n", i, packet[i]);
    }

    // Calculate BCC2 for the de-stuffed data
    unsigned char calculatedBCC2 = 0;
    for (int i = 0; i < deStuffedSize - 1; i++) {
        calculatedBCC2 ^= packet[i];
    }

    if (calculatedBCC2 != packet_copy[deStuffedSize - 1]) {
        // Send REJ if BCC2 does not match
        unsigned char rej[5] = {FLAG, Aread, C_REJ(bitTx), Aread ^ C_REJ(bitTx), FLAG};
        write(fd, rej, 5);
        return -1;
    } else {
        // Send RR if BCC2 matches
        unsigned char rr[5] = {FLAG, Aread, C_RR(bitTx), Aread ^ C_RR(bitTx), FLAG};
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
            while (!alarmEnabled && state != STOP) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            state = (byte == FLAG) ? FLAG_RECEIVED : START;
                            break;
                        case FLAG_RECEIVED:
                            state = (byte == Aread) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                            break;
                        case A_RECEIVED:
                            state = (byte == CUA) ? C_RECEIVED : START;
                            break;
                        case C_RECEIVED:
                            state = (byte == BCC1r) ? BCC1 : START;
                            break;
                        case BCC1:
                            state = (byte == FLAG) ? STOP : START;
                            break;
                        default:
                            break;
                    }
                }
            }
            if (state == STOP) {
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
        while (state != STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        state = (byte == FLAG) ? FLAG_RECEIVED : START;
                        break;
                    case FLAG_RECEIVED:
                        state = (byte == Awrite) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                        break;
                    case A_RECEIVED:
                        state = (byte == DISC) ? C_RECEIVED : START;
                        break;
                    case C_RECEIVED:
                        state = (byte == BCC1_DISC) ? BCC1 : START;
                        break;
                    case BCC1:
                        state = (byte == FLAG) ? STOP : START;
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
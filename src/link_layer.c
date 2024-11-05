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

// Constants and Globals
extern int fd;               
int alarmEnabled = 0;
int alarmCount = 0;
int bitTx = 0;
int bitRx = 0;
    
int check;

int timeout;
int retransmissions;
LinkLayerRole role;
char serialPort[50];
int baudRate;

// Alarm function handler
void alarmHandler(int signal) {
    alarmEnabled = 1; // Timer timeout flag
    alarmCount++;
    printf("Timeout %d\n", alarmCount);
}

// Initialize connection parameters
void setConnectionParameters(LinkLayer connectionParameters) {
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    role = connectionParameters.role;
    strcpy(serialPort, connectionParameters.serialPort);
    baudRate = connectionParameters.baudRate;
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
}

int reconnectSerialPort(const char* serialPort, int baudRate) {
    closeSerialPort(fd);  // Close existing file descriptor
    fd = openSerialPort(serialPort, baudRate);  // Attempt to reopen
    return (fd < 0) ? -1 : 0;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

int llopen(LinkLayer connectionParameters) {
    alarmCount = 0;
    LinkLayerState state = START;

    setConnectionParameters(connectionParameters);
    if (fd < 0) return -1;

    // Configure signal handler for retransmission
    signal(SIGALRM, alarmHandler);

    if (role == LlTx) {
        // Transmitter: send SET and wait for UA
        while (retransmissions > 0) {
            unsigned char supFrame[5] = {FLAG, Awrite, CSet, BCC1w, FLAG};
            write(fd, supFrame, 5);
            alarm(timeout);  // Start timer
            alarmEnabled = 0;

            while (!alarmEnabled && state != STOP) {
                unsigned char byte;
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
            if (state == STOP) return fd; // Successful connection
            retransmissions--;
        }
        return -1;  // Failed to connect as transmitter

    } else if (role == LlRx) {
        // Receiver: wait for SET, then reply with UA
        while (state != STOP) {
            unsigned char byte;
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
        // Send UA frame back to the transmitter
        unsigned char uaFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, uaFrame, 5);
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


    //printf("index: %d\n", index);
    frame[index++] = bcc2;
    //printf("bcc2: %d \n", bcc2);
    frame[index++] = FLAG;
    int stuffedFrameSize = index;

    /*printf("[DEBUG] frame \n");
    for (int i = 0; i < stuffedFrameSize; i++) {
        
        printf("%02X ", frame[i]);
    }*/

    int aceite = 0;
    int rejeitado = 0;
    int transmission = 0;
    
    while (transmission < retransmissions) {
        alarmEnabled = 0;
        alarm(timeout);
        if (write(fd, frame, stuffedFrameSize) == -1) { // Error on write
            if (reconnectSerialPort(serialPort, baudRate) == -1) {
                return -1;  // Return error if reconnection fails
            }
            continue;  // Retry sending after reconnecting
        }
        aceite = 0;
        rejeitado = 0;

        while (!alarmEnabled && !rejeitado && !aceite) {
            write(fd, frame, stuffedFrameSize);
            unsigned char byte, cByte = 0;
    LinkLayerState state = START;

    while (state != STOP && alarmEnabled == FALSE) {  
        check = read(fd, &byte, 1);
        if (check > 0) {
            //printf("byte: %02X \n", byte);
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
                //printf("a puta passou \n");
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
        else if (check == -1){
            printf("Error reading from serial port opening in llwrite\n");
            openSerialPort(serialPort, baudRate);
        }
    }
    //printf("byte passado: %02X \n", cByte);
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

    //free(frame); cant uncomment tis or it fails

    if (aceite) return frameSize;
    if (rejeitado) printf("[ERROR] Frame rejected, BCC2 mismatch\n");
    
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

int llread(unsigned char *packet) {
    int state = START;
    int packetSize = 0;  // Size of the destuffed data in packet
    int maxRetries = 3;  // Maximum reconnection attempts

    // State machine to receive a full frame and perform destuffing on the fly
    while (state != STOP) {
        unsigned char byte;
        int check = read(fd, &byte, 1);

        if (check > 0) {
            switch (state) {
                case START:
                    state = (byte == FLAG) ? FLAG_RECEIVED : START;
                    break;
                case FLAG_RECEIVED:
                    state = (byte == Awrite) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                    break;
                case A_RECEIVED:
                    state = (byte == C_I(bitRx)) ? C_RECEIVED : START;
                    break;
                case C_RECEIVED:
                    state = (byte == (Awrite ^ C_I(bitRx))) ? DATA : START;
                    break;
                case DATA:
                    if (byte == FLAG) {
                        printf("reached FLAG\n");
                        state = STOP;
                    } else if (byte == ESCAPE_BYTE) {
                        state = ESC_RECEIVED;
                    } else {
                        packet[packetSize++] = byte;
                    }
                    if (packetSize > MAX_FRAME_SIZE * 2) {
                        printf("Packet exceeds maximum frame size\n");
                        return -1;
                    }
                    break;
                case ESC_RECEIVED:
                    packet[packetSize++] = byte ^ STUFFING_MASK;
                    state = DATA;
                    break;
                case STOP:
                    printf("reached STOP\n");
                    break;
                default:
                    break;
            }
        } else if (check == -1) {
            // Error reading from serial port - attempt reconnection
            printf("Error reading from serial port, attempting reconnection...\n");
            closeSerialPort(fd);

            int retryCount = 0;
            while (retryCount < maxRetries) {
                fd = openSerialPort(serialPort, baudRate);
                if (fd >= 0) {
                    printf("Reconnected to serial port successfully.\n");
                    break;
                }
                printf("Reconnection attempt %d failed, retrying...\n", retryCount + 1);
                retryCount++;
                sleep(1); // Pause briefly before retrying
            }

            if (fd < 0) {
                printf("Failed to reconnect after %d attempts.\n", maxRetries);
                return -1;
            }
        }
    }

    printf("packetSize: %d\n", packetSize);

    // Calculate BCC2
    unsigned char bcc2 = packet[0];
    for (int i = 1; i < packetSize - 1; i++) {
        bcc2 ^= packet[i];
    }

    // BCC2 verification: Compare calculated BCC2 to the last byte in the frame buffer
    if (bcc2 != packet[packetSize - 1]) {
        unsigned char rejFrame[5] = {FLAG, Aread, C_REJ(bitRx), Aread ^ C_REJ(bitRx), FLAG};
        write(fd, rejFrame, 5);
        printf("BCC2 error\n");
        return -1;
    }

    // Send acknowledgment (RR)
    unsigned char rrFrame[5] = {FLAG, Aread, C_RR(bitRx), Aread ^ C_RR(bitRx), FLAG};
    write(fd, rrFrame, 5);
    bitRx = (bitRx + 1) % 2;
    printf("Received frame with %d bytes\n", packetSize - 1);

    return packetSize - 1;  // Return the packet size without the final BCC2 byte
}


////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////

int llclose(int showStatistics) {
    LinkLayerState state = START;
    if (role == LlTx) {
        // Send DISC and await UA
        for (int attempts = 0; attempts < retransmissions; attempts++) {
            unsigned char discFrame[5] = {FLAG, Awrite, DISC, BCC1_DISC, FLAG};
            write(fd, discFrame, 5);
            alarm(timeout);
            while (!alarmEnabled && state != STOP) {
                unsigned char byte;
                if (read(fd, &byte, 1) > 0 && byte == CUA) {
                    state = STOP;
                }
            }
            if (state == STOP) break;
        }
    } else if (role == LlRx) {
        // Wait for DISC and send UA
        while (state != STOP) {
            unsigned char byte;
            if (read(fd, &byte, 1) > 0) {
                if (byte == FLAG) {
                    state = FLAG_RECEIVED;
                } else if (byte == Awrite && state == FLAG_RECEIVED) {
                    state = A_RECEIVED;
                } else if (byte == DISC && state == A_RECEIVED) {
                    state = STOP;
                }
            }
        }
        unsigned char uaFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, uaFrame, 5);
    }
    closeSerialPort(fd);
    return 1;
}

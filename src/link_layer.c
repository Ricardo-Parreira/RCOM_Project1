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

int timeout;
int retransmissions;
LinkLayerRole role;

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
    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
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

int llwrite(const unsigned char *buf, int bufSize) {
    unsigned char frame[MAX_FRAME_SIZE * 2 + 6]; // Buffer for stuffed frame
    int stuffedSize = 0;

    // Build the frame with flags, address, control, BCC1, data, and BCC2
    frame[0] = FLAG;
    frame[1] = Awrite;
    frame[2] = C_I(bitTx);
    frame[3] = frame[1] ^ frame[2]; // BCC1 for header

    // Data stuffing
    int index = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE_BYTE) {
            frame[index++] = ESCAPE_BYTE;
            frame[index++] = buf[i] ^ STUFFING_MASK;
        } else {
            frame[index++] = buf[i];
        }
    }

    // Calculate and add BCC2 with stuffing
    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }
    if (bcc2 == FLAG || bcc2 == ESCAPE_BYTE) {
        frame[index++] = ESCAPE_BYTE;
        frame[index++] = bcc2 ^ STUFFING_MASK;
    } else {
        frame[index++] = bcc2;
    }
    frame[index++] = FLAG; // End flag

    stuffedSize = index;

    // Send the frame with retransmissions if needed
    for (int transmission = 0; transmission < retransmissions; transmission++) {
        alarmEnabled = 0;
        alarm(timeout);
        write(fd, frame, stuffedSize);

        // Await acknowledgment (RR or REJ)
        while (!alarmEnabled) {
            unsigned char byte;
            if (read(fd, &byte, 1) > 0) {
                if (byte == C_RR(bitTx)) {
                    bitTx = (bitTx + 1) % 2;
                    return stuffedSize;
                } else if (byte == C_REJ(bitTx)) {
                    break; // Retransmit on REJ
                }
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

int llread(unsigned char *packet) {
    unsigned char frame[MAX_FRAME_SIZE + 6];  // Temporary buffer for reading bytes
    int state = START;
    int packetSize = 0;  // Size of the destuffed data in packet
    //unsigned char bcc2 = 0;  // BCC2 accumulator for error checking

    // State machine to receive a full frame and perform destuffing on the fly
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
                    state = (byte == C_I(bitRx)) ? C_RECEIVED : START;
                    break;
                case C_RECEIVED:
                    state = (byte == (Awrite ^ C_I(bitRx))) ? DATA : START;
                    break;
                case DATA:
                    if (byte == FLAG) {
                        printf("reached FLAG\n");
                        state = STOP;
                        break;
                    } else if (byte == ESCAPE_BYTE) {
                        state = ESC_RECEIVED;
                        }
                    else {
                        packet[packetSize++] = byte;
                        if (packetSize > 0) frame[packetSize-1] = packet[packetSize-1];
                        
                    } 
                    if (packetSize > MAX_FRAME_SIZE*2) {
                        printf("%d \n ", packetSize);
                        // Error: Packet exceeds maximum frame size
                        printf("Packet exceeds maximum frame size fr\n");

                        return -1;
                    }
                    break;
                case ESC_RECEIVED:
                    packet[packetSize++] = byte ^ STUFFING_MASK;
                    if (packetSize > 0) frame[packetSize-1] = packet[packetSize-1];
                    state = DATA;
                    break;
                case STOP:
                    printf("reached STOP\n");
                    break;
                default:
                    break;
            }
        }
    }
    printf("packetSize: %d\n", packetSize);

    //calculate BCC2
    unsigned char bcc2 = packet[0];
    for (int i = 1; i < packetSize - 1; i++) {
        bcc2 ^= packet[i];
    }

    //debug
    printf("packet: ");
    for (int i = 0; i < packetSize; i++) {
        printf("%02X ", packet[i]);
    }
    printf("\n");
    /*//debug
    printf("frame: ");
    for (int i = 0; i < packetSize; i++) {
        printf("%02X ", frame[i]);
    }*/

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
    
    //packet = frame;

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

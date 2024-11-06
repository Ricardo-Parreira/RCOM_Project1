// Link layer protocol implementation
#include "link_layer.h"
#include "serial_port.h"

// Constants and Globals
extern struct timeval start, end;
double duration;

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
int bytesSent = 0;
int bytesReceived = 0;
int rejectedFrames = 0;

void alarmHandler(int signal) {
    alarmEnabled = 1; // Timer timeout flag
    alarmCount++;
    printf("Timeout %d\n", alarmCount);
}

void setConnectionParameters(LinkLayer connectionParameters) {
    struct termios newtio;

    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    role = connectionParameters.role;
    strcpy(serialPort, connectionParameters.serialPort);
    baudRate = connectionParameters.baudRate;

    fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (fd < 0) {
        perror("Error opening serial port");
        exit(-1);
    }

    // Configure the serial port settings in termios
    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD; 
    newtio.c_iflag = IGNPAR;  // Ignore parity errors
    newtio.c_oflag = 0;       // No output processing
    newtio.c_lflag = 0;       // Non-canonical mode

    // Set VMIN and VTIME for non-canonical read
    newtio.c_cc[VTIME] = connectionParameters.timeout * 10; // Timeout in tenths of a second
    newtio.c_cc[VMIN] = 0; // Minimum number of characters to read

    // Apply configuration to the serial port
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) != 0) {
        perror("Error setting serial port attributes");
        close(fd);
        exit(-1);
    }
}

int reconnectSerialPort(const char* serialPort, int baudRate) {
    closeSerialPort(fd);  
    fd = openSerialPort(serialPort, baudRate);  
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

    signal(SIGALRM, alarmHandler);

    if (role == LlTx) {
        while (retransmissions > 0) {
            unsigned char supFrame[5] = {FLAG, Awrite, CSet, BCC1w, FLAG};
            write(fd, supFrame, 5);
            alarm(timeout);
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
        return -1;  // Failed

    } else if (role == LlRx) {
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
    unsigned char frame[MAX_FRAME_SIZE * 2 + 6];
    //int frameSize = bufSize + 6;

    frame[0] = FLAG;
    frame[1] = Awrite;
    frame[2] = C_I(bitTx);
    frame[3] = frame[1] ^ frame[2];

    signal(SIGALRM, alarmHandler);

    // Stuffing 
    int index = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE_BYTE) {
            frame[index++] = ESCAPE_BYTE;
            frame[index++] = buf[i] ^ STUFFING_MASK;
        } else {
            frame[index++] = buf[i];
        }
    }

    // Calculate BCC2
    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];
    }
    frame[index++] = bcc2;
    frame[index++] = FLAG;
    int stuffedFrameSize = index;

    int retries = 0;
    while (retries < retransmissions) {

        alarmEnabled = 0;
        alarm(timeout);
        if (write(fd, frame, stuffedFrameSize) == -1) {
            if (reconnectSerialPort(serialPort, baudRate) == -1) {
                return -1;
            }
            continue;  //Retry sending after reconnecting
        }
        else{
            printf("Sent frame with %d bytes\n", stuffedFrameSize);
        }

        unsigned char byte;
        int receivedRR = 0;
        int receivedREJ = 0;
        LinkLayerState state = START;

        // Wait for acknowledgment or rejection
        while (!alarmEnabled && !receivedREJ && !receivedRR) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        state = (byte == FLAG) ? FLAG_RECEIVED : START;
                        break;
                    case FLAG_RECEIVED:
                        state = (byte == Aread) ? A_RECEIVED : (byte == FLAG ? FLAG_RECEIVED : START);
                        break;
                    case A_RECEIVED:
                        if (byte == C_RR(bitTx)) {
                            receivedRR = 1;
                            state = STOP;
                        } else if (byte == C_REJ(bitTx)) {
                            receivedREJ = 1;
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

        if (receivedRR) {
            bitTx = (bitTx + 1) % 2;
            bytesSent += bufSize;
            return stuffedFrameSize;
        } else if (receivedREJ) {
            printf("Received REJ, retransmitting...\n");
            rejectedFrames++;
            retries++;
            alarmEnabled = 0;
        } else if (alarmEnabled) {
            printf("Timeout occurred, retransmitting...\n");
            retries++;
        }
    }

    printf("Transmission failed after %d attempts.\n", retries);
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

int llread(unsigned char *packet) {
    int state = START;
    int packetSize = 0;
    int maxRetries = 3;

    // State machine to receive a full frame and perform destuffing inside it
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

    // BCC2 verification
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

    bytesReceived += packetSize - 1;
    return packetSize - 1;
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
            if (state == STOP){
                //Statistics
                if (showStatistics) {
                printf("Transmission statistics:\n");
                printf("Retransmissions left: %d\n", retransmissions);
                printf("Timeouts: %d\n", alarmCount);
                printf("Number of bytes sent: %d\n", bytesSent);
                printf("Rejected frames that were sent again: %d\n", rejectedFrames);
                gettimeofday(&end, NULL);
                duration = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
                printf("Total transmission time: %.4f seconds\n", duration);
                }

                break;
            }
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
        // Statistics
        if (showStatistics) {
            printf("Transmission statistics:\n");
            printf("Total number of bytes received: %d\n", bytesReceived);
        }
        unsigned char uaFrame[5] = {FLAG, Aread, CUA, BCC1r, FLAG};
        write(fd, uaFrame, 5);
    }
    closeSerialPort(fd);
    return 1;
}

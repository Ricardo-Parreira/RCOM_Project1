// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
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

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

clock_t start_time;
int received_bits = 0;
int sent_bits = 0;
extern int fd;
LinkLayerRole role;
char serialPort[50];
int baudRate;
int timeout = 0;
int transmitions = 0;
int alarmEnabled = FALSE;
int alarmCount = 0;
int frame_sequence = 0;
int error_count = 0;


void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int llopen(LinkLayer connectionParameters)
{   
    alarmCount = 0;
    baudRate = connectionParameters.baudRate;
    memcpy(serialPort, connectionParameters.serialPort, 50);
    openSerialPort(serialPort,baudRate);
    if(fd < 0)return -1;
    State state = INIT;
    timeout = connectionParameters.timeout;
    transmitions = connectionParameters.nRetransmissions;
    unsigned char byte;
    role = connectionParameters.role;

    // Set up the connection
    switch(role){
        case(LlTx) :{
            (void)signal(SIGALRM, alarmHandler);
            unsigned char frame[5] = {FLAG,Awrite,Cset,Awrite ^Cset,FLAG} ;
            while (alarmCount <= transmitions){
            if (alarmEnabled == FALSE){
                write(fd, frame, 5);
                alarm(timeout); 
                alarmEnabled =  TRUE;
            }
            while (alarmEnabled == TRUE && state != LIDO){
                if (read(fd,&byte,1) > 0) {
                    switch(state){
                        case INIT:
                            if (byte == FLAG) state = FLAG_RECEBIDO; 
                            break;
                        case FLAG_RECEBIDO:
                            if (byte == Aread) state = A_RECEBIDO; 
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RECEBIDO:
                            if(byte == Cua) state = C_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case C_RECEBIDO:
                            if(byte == (Aread ^ Cua)) state = BCC_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;        
                        case BCC_RECEBIDO:
                            if(byte == FLAG) state = LIDO;
                            else state = INIT;
                            break;  
                        default:
                            break;         
                    }
            }   
            }
            if(state == LIDO){
                printf("Received UA, connection established!\n");
                return fd;
            }
        }
        break;
    }
        case (LlRx) :{
            while (state != LIDO){
                if (read(fd,&byte,1) > 0) {
                    switch(state){
                        case INIT:
                            if (byte == FLAG) state = FLAG_RECEBIDO; 
                            break;
                        case FLAG_RECEBIDO:
                            if (byte == Awrite) state = A_RECEBIDO; 
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RECEBIDO:
                            if(byte == Cset) state = C_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case C_RECEBIDO:
                            if(byte == (Awrite ^ Cset)) state = BCC_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;        
                        case BCC_RECEBIDO:
                            if(byte == FLAG) state = LIDO;
                            else state = INIT;
                            break;  
                        default:
                            break;         
                    }
            }   
            }
            if(state ==LIDO){
                unsigned char frame[5] = {FLAG,Aread,Cua,Aread ^Cua,FLAG} ;
                write(fd, frame, 5);
                printf("Sent UA frame. Connection established.\n");
                return fd;
            }
            break;
        }
        default :
            return -1;
            break;
    }
    return -1;
}


unsigned char *byteStuffing(const unsigned char *data, int dataSize, int *newSize) {
    // Temporary buffer with maximum possible size
    unsigned char *stuffedData = (unsigned char *)malloc(dataSize * 2);
    if (stuffedData == NULL) {
        perror("[ERROR] Memory allocation failed");
        return NULL;
    }
    
    int j = 0;
    for (int i = 0; i < dataSize; i++) {
        if (data[i] == FLAG || data[i] == ESC_BYTE) {
            stuffedData[j++] = ESC_BYTE;
            stuffedData[j++] = data[i] ^ 0x20;
        } else {
            stuffedData[j++] = data[i];
        }
    }
    
    *newSize = j; // Update the new size after stuffing
    return stuffedData;
}

int llwrite(const unsigned char *buf, int bufSize) {
    int frame_size = 6 + bufSize; // Initial frame size (including header and last FLAG)
    unsigned char *frame = (unsigned char *)malloc(frame_size);
    if (frame == NULL) {
        perror("[ERROR] Memory allocation failed");
        return -1;
    }
    
    // Prepare the frame header
    frame[0] = FLAG;
    frame[1] = Awrite;
    frame[2] = (frame_sequence == 0) ? I_FRAME_0 : I_FRAME_1;
    frame[3] = frame[1] ^ frame[2];

    // Calculate BCC2
    unsigned char BCC2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        BCC2 ^= buf[i];
    }

    // Perform byte stuffing on data and BCC2
    int stuffedDataSize;
    unsigned char *stuffedData = byteStuffing(buf, bufSize, &stuffedDataSize);
    if (stuffedData == NULL) {
        free(frame);
        return -1;
    }
    
    // Reallocate frame for stuffed data
    frame = realloc(frame, 4 + stuffedDataSize + 2); // Header (4), stuffed data, BCC2, and FLAG
    if (frame == NULL) {
        free(stuffedData);
        perror("[ERROR] Memory allocation failed");
        return -1;
    }

    memcpy(frame + 4, stuffedData, stuffedDataSize); // Copy stuffed data into frame
    free(stuffedData);

    // Perform byte stuffing on BCC2 and append it to frame
    int stuffedBCC2Size;
    unsigned char BCC2Array[1] = {BCC2};
    unsigned char *stuffedBCC2 = byteStuffing(BCC2Array, 1, &stuffedBCC2Size);
    frame = realloc(frame, 4 + stuffedDataSize + stuffedBCC2Size + 1); // Header, data, BCC2, FLAG
    if (frame == NULL) {
        free(stuffedBCC2);
        perror("[ERROR] Memory allocation failed");
        return -1;
    }

    memcpy(frame + 4 + stuffedDataSize, stuffedBCC2, stuffedBCC2Size); // Append BCC2
    free(stuffedBCC2);

    frame[4 + stuffedDataSize + stuffedBCC2Size] = FLAG; // Add closing FLAG
    frame_size = 4 + stuffedDataSize + stuffedBCC2Size + 1; // Update final frame size


    alarmCount = 0;
    alarmEnabled = FALSE;
    int aceite = 0;
    int rejeitado = 0;
    State state = INIT;
    unsigned char Cbyte;
    unsigned char byte;
    (void)signal(SIGALRM, alarmHandler);

    // Check acknowledgement frame
    while (alarmCount <= transmitions ){
            if (alarmEnabled == FALSE){
                if(write(fd, frame, frame_size) == -1){
                    openSerialPort(serialPort,baudRate);
                }
                rejeitado = 0;
                alarm(timeout); 
                alarmEnabled =  TRUE;
                sent_bits += frame_size * 8;
            }
            while (alarmEnabled == TRUE && state != LIDO){  
                if (read(fd,&byte,1) > 0){
                    switch(state){
                        case INIT:
                            if (byte == FLAG) state = FLAG_RECEBIDO; 
                            break;
                        case FLAG_RECEBIDO:
                            if (byte == Aread) state = A_RECEBIDO; 
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RECEBIDO:
                            if ( byte == C_RR_0 || byte == C_RR_1 || byte == C_REJ_0 || byte == C_REJ_1 ){
                                state = C_RECEBIDO;
                                Cbyte = byte;
                            }
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case C_RECEBIDO:
                            if(byte == (Aread ^ Cbyte)) state = BCC_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case BCC_RECEBIDO:
                            if(byte == FLAG){
                                state = LIDO;
                            }
                            else state = INIT;
                            break;
                        default:
                            break;           
                    }
                }         
            }
            if(state == LIDO){
                if(Cbyte == C_RR_0 || Cbyte == C_RR_1){
                    aceite = 1;
                    frame_sequence = (frame_sequence+1)%2;
                    received_bits += frame_size *8;
                }
                else if(Cbyte == C_REJ_0 || Cbyte== C_REJ_1){
                    rejeitado = 1;
                    error_count++;
                }
           
            }
            if(aceite){
                alarmCount = transmitions+1;
                alarm(0);
                alarmEnabled = FALSE;
                break;
            }
            if(rejeitado){
                alarm(0);
                printf("[ERROR] Frame rejected!\n");
                alarmHandler(SIGALRM);
                state = INIT;
            }
    }
    free(frame);
    if (aceite) return frame_size;
    return -1;
}


int llread(unsigned char *packet) {

    int dataIndex = 0;  
    unsigned char CResponse;
    int readByte = 0;
    State state = INIT;
    unsigned char byte;
    unsigned char Cbyte;
    
    // State machine to read the frame
    while (state != LIDO) {
        readByte = read(fd, &byte, 1) ;
        if (readByte > 0) {
        switch (state) {
            case INIT:
                if (byte == FLAG)
                    state = FLAG_RECEBIDO;  
                break;

            case FLAG_RECEBIDO:
                if (byte == Awrite)
                    state = A_RECEBIDO;  
                else if (byte != FLAG)
                    state = INIT;  
                break;

            case A_RECEBIDO:
                if (byte == I_FRAME_0 || byte == I_FRAME_1) {  
                    Cbyte = byte;
                    state = C_RECEBIDO;  
                } else if (byte == FLAG)
                    state = FLAG_RECEBIDO;  
                else
                    state = INIT;  
                break;

            case C_RECEBIDO:
                if (byte == (Awrite ^ Cbyte))  
                    state = BCC_RECEBIDO;  
                else if (byte == FLAG)
                    state = FLAG_RECEBIDO;  
                else
                    state = INIT;  
                break;

            // Receiving data
            case BCC_RECEBIDO:
                if (byte == ESC_BYTE) {
                    state = ESC_RECEBIDO;  
                } 
                else if (byte == FLAG) {
                unsigned char BCC2 = packet[dataIndex - 1];  
                dataIndex--;  
                packet[dataIndex] = '\0';  
                unsigned char bcc_check = packet[0];
                for (unsigned int j = 1; j < dataIndex; j++) {
                    bcc_check ^= packet[j];
                }
                if (BCC2 == bcc_check) {
                    state = LIDO;  
                } 
                else {
                    if(frame_sequence == 0)CResponse = C_REJ_0;
                    else CResponse = C_REJ_1;
                    unsigned char frame[5] = {FLAG,Aread,CResponse,Aread ^CResponse,FLAG} ;
                    write(fd, frame, 5);
                    printf("[ERROR] Frame rejected.\n");
                    printf("Readon: BCC2 didnt match.\n");
                    return -1;  
                }
                } 
                else {
                    packet[dataIndex++] = byte; //every time it is stuck on BCC_RECEBIDO it will increment the index
                }
            break;
            case ESC_RECEBIDO:
                state = BCC_RECEBIDO;
                if (byte == FLAG || byte == ESC_BYTE) {
                    packet[dataIndex++] = byte;
                } else {
                    packet[dataIndex++] = byte ^ 0x20;  
                }
                break;


            default:
                break;
        }
        }
        else if(readByte <0) {
            openSerialPort(serialPort,baudRate);
        }
    }
    if(frame_sequence == 0)CResponse = C_RR_0;
    else CResponse = C_RR_1;
    unsigned char frame[5] = {FLAG,Aread,CResponse,Aread ^CResponse,FLAG} ;
    write(fd,frame,5);
    printf("Frame received!!\n");
    return dataIndex; 
}

int llclose(int showStatistics)
{   
    alarmCount = 0;
    alarmEnabled = FALSE;
    State state = INIT;
    unsigned char byte;
    
    unsigned char discFrame[5] = {FLAG, Awrite, C_DISC, Awrite ^ C_DISC, FLAG};
    switch(role){
        case LlTx : {
            (void)signal(SIGALRM, alarmHandler);  
            while (alarmCount <= transmitions) {
                if (alarmEnabled == FALSE) {
                    write(fd, discFrame, 5);   
                    alarm(timeout);  
                    alarmEnabled = TRUE;
                }
                while (alarmEnabled == TRUE &&  state != LIDO){
                if (read(fd,&byte,1) > 0) {
                    switch(state){
                        case INIT:
                            if (byte == FLAG) state = FLAG_RECEBIDO; 
                            break;
                        case FLAG_RECEBIDO:
                            if (byte == Awrite) state = A_RECEBIDO; 
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RECEBIDO:
                            if(byte == C_DISC) state = C_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case C_RECEBIDO:
                            if(byte == (Awrite ^ C_DISC)) state = BCC_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;        
                        case BCC_RECEBIDO:
                            if(byte == FLAG) state = LIDO;
                            else state = INIT;
                            break;  
                        default:
                            break;         
                    }
            }   
            }
                if (state == LIDO) {
                    printf("DISC frame detected. Transmitting UA frame in response.\n");
                    unsigned char ua_Frame[5] = {FLAG, Aread, Cua, Aread ^ Cua, FLAG};
                    write(fd, ua_Frame, 5);
                    printf("UA frame sent. Terminating connection.\n");
                    int closeStatus = closeSerialPort();
                    if (showStatistics) {
                        double time_taken = endClock();
                        printf("Total elapsed time: %f seconds\n", time_taken);
                        printf("Count of rejected frames: %i\n", error_count);
                        double sent_rate = sent_bits / time_taken;
                        double received_rate = received_bits / time_taken;
                        printf("Total bits received: %i\n", received_bits);
                        printf("Reception rate (bits/sec): %f\n", received_rate);
                        printf("Total bits sent: %i\n", sent_bits);
                        printf("Transmission rate (bits/sec): %f\n", sent_rate);
                        printf("Connection terminated.\n");
                    }
                


                    return closeStatus;  
                }

            }
        break;
    }
        case LlRx : {
            while (state != LIDO){
                if (read(fd,&byte,1) > 0) {
                    switch(state){
                        case INIT:
                            if (byte == FLAG) state = FLAG_RECEBIDO; 
                            break;
                        case FLAG_RECEBIDO:
                            if (byte == Awrite) state = A_RECEBIDO; 
                            else if (byte != FLAG) state = INIT;
                            break;
                        case A_RECEBIDO:
                            if(byte == C_DISC) state = C_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;
                        case C_RECEBIDO:
                            if(byte == (Awrite ^ C_DISC)) state = BCC_RECEBIDO;
                            else if (byte == FLAG) state = FLAG_RECEBIDO;
                            else state = INIT;
                            break;        
                        case BCC_RECEBIDO:
                            if(byte == FLAG) state = LIDO;
                            else state = INIT;
                            break;  
                        default:
                            break;         
                    }
            }   
            }
            if (state == LIDO) {
                    printf("Received DISC. The DISC receiver frame will now be sent.\n");
                    write(fd, discFrame, 5);
                    state = INIT;
                    while (state != LIDO){
                        if (read(fd,&byte,1) > 0) {
                            switch(state){
                                case INIT:
                                    if (byte == FLAG) state = FLAG_RECEBIDO; 
                                    break;
                                case FLAG_RECEBIDO:
                                    if (byte == Aread) state = A_RECEBIDO; 
                                    else if (byte != FLAG) state = INIT;
                                    break;
                                case A_RECEBIDO:
                                    if(byte == Cua) state = C_RECEBIDO;
                                    else if (byte == FLAG) state = FLAG_RECEBIDO;
                                    else state = INIT;
                                    break;
                                case C_RECEBIDO:
                                    if(byte == (Aread ^ Cua)) state = BCC_RECEBIDO;
                                    else if (byte == FLAG) state = FLAG_RECEBIDO;
                                    else state = INIT;
                                    break;        
                                case BCC_RECEBIDO:
                                    if(byte == FLAG) state = LIDO;
                                    else state = INIT;
                                    break;  
                                default:
                                    break;         
                            }
                    }   
                    }
                    if(state== LIDO){
                    printf("Received UA, closing connection now.\n");
                    int closeStatus = closeSerialPort();
                    if (showStatistics) {
                        double time_taken = endClock(); 
                        printf("Elapsed time: %f seconds\n", time_taken);
                        printf("Connection closed successfully.\n");
                    }
                    return closeStatus;
                    }  
                }
            break;
        }
        default:
            break;
    }

    printf("[ERROR] Failed to receive DISC. %d retransmissions.\n", transmitions);
    return -1;
}

void startClock() {
    start_time = clock(); 
}

double endClock() {
    clock_t end_time = clock(); 
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    return elapsed_time;
}
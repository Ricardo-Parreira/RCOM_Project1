// Link layer protocol implementation
#include "application_helper.h"
#include "application_layer.h"
#include "link_layer.h"
#include <signal.h>
#define BAUDRATE 38400

// MISC
int alarmEnabled = FALSE;
int alarmCount = 0;
int timeout = 0;
int nRetransmissions = 0;
extern struct termios oldtio;
struct timespec initial_time, final_time;
int dataErrors = 0;
unsigned char frameNumberTransmiter = 0;
unsigned char frameNumberReceiver = 0;
int accept = 0;
int BCC_error = 0;
int totalBits = 0;
int sendedBits = 0;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    (void) signal(SIGALRM, alarmHandler);
 
    // check connection
    int fd = set_serial_port(connectionParameters);
    if (fd < 0) return -1;

    LinkLayerStateMachine state = START;

   
    switch (connectionParameters.role) {

        case LlTx: {
   
            startClock();
            state = transmiter_state_machine(fd,state);
            if (state != STOP){
                llclose(fd);
                return -1;
            }

            break;  
        }

        case LlRx: {
            receiver_state_machine(fd,state);
            unsigned char UA[5] = {FLAG, A_RE, C_UA, A_RE ^ C_UA, FLAG};
            write(fd, UA, 5);
            break; 
        }

        default:
            return -1;
    }
    
    return fd;
}

   


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize) {
    int current_size = bufSize + 6;
    unsigned char *information_frame = (unsigned char *) malloc(current_size);
    if (!information_frame) {
        perror("Failed to allocate memory");
        return -1;
    }

    // Set up initial bytes in the information frame
    information_frame[0] = FLAG;
    information_frame[1] = A_ER;
    information_frame[2] = C_N(frameNumberTransmiter);
    information_frame[3] = information_frame[1] ^ information_frame[2];
    
    // Calculate BCC2
    unsigned char BCC2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        BCC2 ^= buf[i];
    }

    // Copy data and perform byte stuffing
    int current_index = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            information_frame = realloc(information_frame, ++current_size);
            information_frame[current_index++] = ESC;
            information_frame[current_index++] = buf[i] ^ 0x20;
        } else {
            information_frame[current_index++] = buf[i];
        }
    }

    // Byte-stuff BCC2 if necessary
    if (BCC2 == FLAG || BCC2 == ESC) {
        information_frame = realloc(information_frame, ++current_size);
        information_frame[current_index++] = ESC;
        information_frame[current_index++] = BCC2 ^ 0x20;
    } else {
        information_frame[current_index++] = BCC2;
    }

    // Add final FLAG byte
    information_frame[current_index++] = FLAG;

    // Initialize variables for transmission and state machine
    alarmCount = 0;
    int accepted = 0, rejected = 0;
    unsigned char byte = '\0';
    LinkLayerStateMachine current_state = START;

    while (alarmCount < nRetransmissions) {
        // Enable alarm for timeout and reset state
        alarmEnabled = TRUE;
        alarm(timeout);
        accepted = 0;
        rejected = 0;

        // Send frame and handle response
        int written_bytes = write(fd, information_frame, current_index);
        if (written_bytes < 0) {
            perror("Failed to write data");
            return -1;
        }
        sendedBits += bufSize * 8;

        // Process response using state machine
        while (alarmEnabled && !accepted && !rejected) {
            unsigned char response = control_frame_state_machine(fd, byte, current_state);
            
            if (response == C_RR(0) || response == C_RR(1)) {
                accepted = 1;
                frameNumberTransmiter = (frameNumberTransmiter + 1) % 2;
                alarm(0);
                totalBits += bufSize * 8;
            } else if (response == C_REJ(0) || response == C_REJ(1)) {
                rejected = 1;
                dataErrors++;
            }
        }

        if (accepted) {
            alarmCount = nRetransmissions;
        } else if (rejected) {
            alarmCount++;
        }
    }

    if (accepted) {
        return current_index;
    } else {
        llclose(fd);
        return -1;
    }
}


////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *packet)
{

    unsigned char byte, control_field;
    LinkLayerStateMachine current_state = START;
    int current_index = 0;
    
    while (current_state != STOP){
        if (read(fd, &byte, 1) > 0){
                
            switch (current_state) {
                case START:
                    
                    if (byte == FLAG) current_state = FLAG_RCV;
                    break;
                
                case FLAG_RCV:

                    if (byte == A_ER) current_state = A_RCV;
                    else if (byte != FLAG) current_state = START;
                    break;

                case A_RCV:

                    if (byte == C_N(frameNumberReceiver)){

                        current_state = C_RCV;
                        control_field = byte;

                    }
                    else if (byte == C_N((frameNumberReceiver+1)%2)){
                        unsigned char ACCEPTED[5] = {FLAG, A_RE, C_RR((frameNumberReceiver+1)%2), A_RE ^ C_RR((frameNumberReceiver+1)%2), FLAG};
                        write(fd, ACCEPTED, 5);
                        return 0;
                    }
                    else if (byte == FLAG) current_state = FLAG_RCV;
                    else if (byte == C_DISC){
                        unsigned char DISC[5] = {FLAG, A_RE, C_DISC, A_RE ^ C_DISC, FLAG};
                        write(fd, DISC, 5);
                        if (tcsetattr(fd, TCSANOW, &oldtio) == -1){
                            perror("tcsetattr");
                            exit(-1);
                        }
    
                        return close(fd);

                    }
                    else current_state = START;
                    break;
                
                case C_RCV:
                    
                    if (byte == (A_ER ^ control_field)){
                        current_state = DATA;
                    }
                    else if (byte == FLAG) current_state = FLAG_RCV;
                    else current_state = START;
                    break;
                
                case DATA:
                    if (byte == ESC) current_state = STUFFEDBYTES;
                    else if (byte == FLAG){

                        
                        unsigned char bcc2 = packet[current_index-1];
                        current_index--;
                        packet[current_index] = '\0';
                        unsigned char bcc2_test = packet[0];

                        for (unsigned int i = 1; i < current_index; i++){

                            bcc2_test ^= packet[i];

                        }

                        if (bcc2 == bcc2_test){
                            if (accept == 0 && BCC_error==1){
     
                                unsigned char REJECTED[5] = {FLAG, A_RE, C_REJ(frameNumberReceiver), A_RE ^ C_REJ(frameNumberReceiver), FLAG};
                                write(fd, REJECTED, 5);
                                accept = 1;
                                return -1;
                        
                            }
    
                            current_state = STOP;
                            unsigned char ACCEPTED[5] = {FLAG, A_RE, C_RR(frameNumberReceiver), A_RE ^ C_RR(frameNumberReceiver), FLAG};
                            write(fd, ACCEPTED, 5);
                            frameNumberReceiver = (frameNumberReceiver + 1)%2;
                            accept = 0;
                            return current_index;
                            
                        }

                        else{

                            unsigned char REJECTED[5] = {FLAG, A_RE, C_REJ(frameNumberReceiver), A_RE ^ C_REJ(frameNumberReceiver), FLAG};
                            write(fd, REJECTED, 5);
                            return -1;

                        }
                    }
                    else{
                        packet[current_index++] = byte;
                    }
                    break;
                case STUFFEDBYTES:
                    current_state = DATA;
                    packet[current_index++] = (byte^0x20);
                    break;
                default:
                    break;

            }
        }

    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{

    LinkLayerStateMachine current_state=START;
    unsigned char byte;
    alarmEnabled = TRUE;
    alarmCount = 0;

    while (alarmCount < nRetransmissions && current_state!=STOP){

        unsigned char DISC[5] = {FLAG, A_ER, C_DISC, A_ER ^ C_DISC, FLAG};
        write(showStatistics, DISC, 5);
        alarm(timeout);
        alarmEnabled = TRUE;
        while (alarmEnabled == TRUE && current_state!=STOP){
            if (read(showStatistics, &byte,1) > 0){
                    switch (current_state) {

                        case START:

                            if (byte == FLAG){

                                current_state = FLAG_RCV;
                            }

                            break;
                        case FLAG_RCV:

                            if (byte == A_RE){
                                current_state = A_RCV;
                            }

                            else if (byte != FLAG) current_state = START;

                            break;

                        case A_RCV:

                            if (byte == C_DISC){
                                current_state = C_RCV;
                            }

                            else if (byte == FLAG){
                                current_state = FLAG_RCV;
                            }

                            else current_state = START;

                            break;

                        case C_RCV:

                            if (byte == (A_RE ^ C_DISC)){
                                current_state = BCC_OK;
                            }

                            else if (byte == FLAG){
                                current_state = FLAG_RCV;
                            }

                            else current_state = START;

                            break;
                        case BCC_OK:

                            if (byte == FLAG){
                                current_state = STOP;
                                alarm(0);
                            }

                            else current_state = START;

                            break;

                        default: 

                            break;
                    }   
            }
        }
    }
    double elapsedTime = endClock();
    printf("Elpased Time: %f seconds\n",elapsedTime);
    printf("Number of rejected frames: %i\n",dataErrors);
    double linkCapacity = sendedBits/elapsedTime;
    double receivedBitrait = totalBits/elapsedTime;
    printf("Bits received per second: %f\n", receivedBitrait);
    printf("Bits sended per second %f\n", linkCapacity);

    if (current_state != STOP){
        
        if (tcsetattr(showStatistics, TCSANOW, &oldtio) == -1){
            perror("tcsetattr");
            exit(-1);
        }
        
        close(showStatistics);
        
        perror("Receiver didn't disconnect\n");
           
        return -1;
    }
    unsigned char UA[5] = {FLAG, A_ER, C_UA, A_ER ^ C_UA, FLAG};
    write(showStatistics, UA, 5);

    if (tcsetattr(showStatistics, TCSANOW, &oldtio) == -1){
            perror("tcsetattr");
            exit(-1);
    }
    
    return close(showStatistics);
}


//alarm handler
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

// receiver state machine to validate SET
void receiver_state_machine(int fd, LinkLayerStateMachine current_state){

       unsigned char byte;

       while (current_state != STOP) {
                if (read(fd, &byte, 1) > 0) {
                 
                    
                    switch (current_state) {

                        case START:

                            if (byte == FLAG){
                                current_state = FLAG_RCV;
                            }

                            break;
                        case FLAG_RCV:

                            if (byte == A_ER){
                                current_state = A_RCV;
                            }

                            else if (byte != FLAG) current_state = START;

                            break;

                        case A_RCV:

                            if (byte == C_SET){
                                current_state = C_RCV;
                            }

                            else if (byte == FLAG){
                                current_state = FLAG_RCV;
                            }

                            else current_state = START;

                            break;

                        case C_RCV:

                            if (byte == (A_ER ^ C_SET)){
                                current_state = BCC_OK;
                            }

                            else if (byte == FLAG){
                                current_state = FLAG_RCV;
                            }

                            else current_state = START;

                            break;
                        case BCC_OK:

                            if (byte == FLAG){
                                current_state = STOP;
                                alarm(0);
                            }

                            else current_state = START;

                            break;

                        default: 

                            break;
                    }
                }
            }  
  
}


//trasmiter state machine to validade UA
LinkLayerStateMachine transmiter_state_machine(int fd,LinkLayerStateMachine current_state){

   unsigned char byte;
   while (alarmCount < nRetransmissions && current_state != STOP) {
                unsigned char SET[5] = {FLAG, A_ER, C_SET, A_ER ^ C_SET, FLAG};
                write(fd, SET, 5);
                alarmEnabled = TRUE;
                alarm(0);
                alarm(timeout);
                
                while (alarmEnabled == TRUE && current_state != STOP) {

                    if (read(fd, &byte, 1) > 0) {
                      
                        switch (current_state) {

                            case START:

                                if (byte == FLAG){
                                    current_state = FLAG_RCV;
                                }

                                break;

                            case FLAG_RCV:

                                if (byte == A_RE){
                                    current_state = A_RCV;
                                } 

                                else if (byte != FLAG){
                                    current_state = START;
                                }
                                break;

                            case A_RCV:

                                if (byte == C_UA){
                                    current_state = C_RCV;
                                } 
                                else if (byte == FLAG){
                                    current_state = FLAG_RCV;
                                } 

                                else current_state = START;

                                break;

                            case C_RCV:

                                if (byte == (A_RE ^ C_UA)){
                                    current_state = BCC_OK;
                                } 

                                else if (byte == FLAG){
                                    current_state = FLAG_RCV;
                                }

                                else current_state = START;

                                break;

                            case BCC_OK:

                                if (byte == FLAG){
                                
                                    current_state = STOP;
                                    alarm(0);
                                }

                                else current_state = START;

                                break;
                            default: 
                                break;
                        }
                    }
                } 
              
            }
            return current_state;
}

int control_frame_state_machine(int fd, unsigned char byte, LinkLayerStateMachine state){

    int answer = 0;
    LinkLayerStateMachine current_state = START;
    while (current_state != STOP && alarmEnabled == TRUE) { 
        
        if (read(fd, &byte, 1) > 0 ) {

            switch (current_state){
            case (START):{

                if (byte == FLAG){
                    current_state = FLAG_RCV;                     
                }

                break;

            }
            case (FLAG_RCV):{

                if (byte == A_RE){
                    current_state = A_RCV;
                }

                else if (byte != FLAG){
                    current_state = START;
                }

                break;

            }

            case (A_RCV):{

                if (byte == C_RR(0) || byte == C_RR(1) || byte == C_REJ(0) || byte == C_REJ(1) ){
                    current_state = C_RCV;
                    answer = byte;

                }

                else if (byte == FLAG){
                    current_state = FLAG_RCV;
                }
            
                else{
                    current_state = START;
                }

                break;

            }

            case (C_RCV):{

                if (byte == (A_RE ^ answer)){
                    current_state = BCC_OK;
                }
            
                else if (byte == FLAG){
                    current_state = FLAG_RCV;
                }
            
                else{
                    current_state = START;
                }

                break;
            }

            case (BCC_OK):{

                if (byte == FLAG){
                    current_state = STOP;
                    alarm(0);
                   

                }

                else{
                    current_state = START;
                }

                break;

            }

            default:

                break;

            }
        }

    }
    return answer;
     
}



int set_serial_port(LinkLayer connectionParameters){

    
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0){

        perror(connectionParameters.serialPort);
        exit(-1);
    }
    
    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    struct termios newtio;
    
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }
    
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }
    return fd;
}

void startClock(){
    clock_gettime(CLOCK_REALTIME, &initial_time);
}

double endClock(){
  clock_gettime(CLOCK_REALTIME, &final_time);
  double time_val = (final_time.tv_sec-initial_time.tv_sec)+
					(final_time.tv_nsec- initial_time.tv_nsec)/1e9;
  return time_val;
}

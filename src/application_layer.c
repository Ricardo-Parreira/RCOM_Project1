// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include "application_helper.h"
#include <math.h>

LinkLayer connectionParameters;


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{

    strcpy(connectionParameters.serialPort,serialPort);
    if (strcmp(role,"tx")){
           connectionParameters.role = LlRx;
    }
    else{
        connectionParameters.role = LlTx;
    }
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    
    FILE *file;

    int fd = llopen(connectionParameters);
    if(fd > 0){
        perror("Llopen\n");
    }
    else return;
    switch(connectionParameters.role){

        case LlRx:{

                file = fopen(filename,"wb+");
                unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
                if (file == NULL) {
                perror("Failed to open file\n");
                return;
                }
                printf("File opened for writing: %s\n", filename);
                while(1){

                    int packetSize = llread(fd, packet);
                    if (packetSize>=0){
                        break;
                   } 
                } 

                while (1) {    
                    
                    int packetSize;
                    while (1){
                        packetSize = llread(fd, packet);
                        if (packetSize>=0) break;
                    }
                        
                    if(packetSize == 0) break;

                    else if(packet[0] != 3){
                        printf("Packet received\n");
                        unsigned char *buffer = (unsigned char*)malloc(packetSize);
                        removeHeaderData(packet, packetSize, buffer);
                        fwrite(buffer, 1, packetSize-4, file);
                        free(buffer);
                    }
                    else if (packet[0] == 3)
                    {   
                        printf("End packet received\n");
                        break;
                    }
                    
                    else break;
                
                }

                 while(1){

                    int packetSize = llread(fd, packet);
                    if (packetSize>=0){
                        break;
                   } 
                } 
                printf("File received\n");

                break;
           

        }
        case LlTx: {

            FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Determine the file size
    fseek(file, 0, SEEK_END); 
    int fileSize = ftell(file); 
    fseek(file, 0, SEEK_SET);
    printf("File opened successfully: %s\n", filename);
    printf("File size: %d bytes\n", fileSize);

    // Create and send start packet
    unsigned int startPacketSize;
    unsigned char* startPacket = parseControl(1, filename, fileSize, &startPacketSize);
    if (llwrite(fd, startPacket, startPacketSize) == -1) { 
        printf("Exit: error in start packet\n");
        free(startPacket); 
        fclose(file); 
        exit(EXIT_FAILURE);
    }
    free(startPacket);

    // Read file data
    unsigned char* data = openFile(file, fileSize);
    int bytesToSend = fileSize;
    unsigned char frameNumber = 0;

    // Send file data in packets
    while (bytesToSend > 0) { 
        int byteSent = (bytesToSend < MAX_PAYLOAD_SIZE) ? bytesToSend : MAX_PAYLOAD_SIZE;

        unsigned char* dataPacket = (unsigned char*)malloc(byteSent + 4);
        if (dataPacket == NULL) {
            perror("Memory allocation error");
            free(data); 
            fclose(file); 
            exit(EXIT_FAILURE);
        }

        // Fill data packet header
        dataPacket[0] = 2; // Packet type
        dataPacket[1] = frameNumber; // Frame number
        dataPacket[2] = (byteSent >> 8) & 0xFF; // High byte of size
        dataPacket[3] = byteSent & 0xFF; // Low byte of size
        memcpy(dataPacket + 4, data, byteSent); // Copy data into packet

        // Send the data packet
        if (llwrite(fd, dataPacket, byteSent + 4) == -1) {
            printf("Exit: error in data packets\n");
            free(dataPacket); 
            free(data); 
            fclose(file); 
            exit(EXIT_FAILURE);
        }

        // Update counters
        bytesToSend -= byteSent; 
        data += byteSent; 
        printf("Bytes sent: %d\n", byteSent);
        frameNumber = (frameNumber + 1) % 255;   

        free(dataPacket);
    }

    // Create and send end packet
    unsigned char* endPacket = parseControl(3, filename, fileSize, &startPacketSize);
    if (llwrite(fd, endPacket, startPacketSize) == -1) { 
        printf("Exit: error in end packet\n");
        free(endPacket); 
        free(data); 
        fclose(file); 
        exit(EXIT_FAILURE);
    }
    free(endPacket); 

    printf("File sent\n");

    // Close the file
    fclose(file);
    llclose(fd); 
            break;

           
            
        }
        
    default:
        break;
        
    }
    


    fclose(file);
    

}

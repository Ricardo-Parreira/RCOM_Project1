#include "application_layer.h"
#include "link_layer.h"
#include "application_helper.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <sys/stat.h>



// Main application layer function
void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer linkLayer;
    int fd = initializeConnection(&linkLayer, serialPort, role, baudRate, nTries, timeout);
    
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    }

    if (linkLayer.role == LlTx) {
        handleTransmitter(filename);
    } else {
        handleReceiver(filename);
    }

    if (llclose(1) < 0) {
        perror("[ERROR] Closing\n");
        exit(-1);
    }
}


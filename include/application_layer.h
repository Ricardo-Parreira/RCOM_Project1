// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Constants

#define CONTROL_START 1
#define CONTROL_END 3
#define CONTROL_DATA 2

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

unsigned char *buildControlPacket(int control_field, int fileSize, const char* fileName, int *controlpacketSize);

unsigned char *readControlPacket(unsigned char *controlpacket, int packetSize, int *fileSize);

unsigned char *buildDataPacket(unsigned char sequence, unsigned char *data_field, int dataFieldSize);




#endif // _APPLICATION_LAYER_H_

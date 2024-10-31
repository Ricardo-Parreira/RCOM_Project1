// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef APPLICATION_LAYER_H
#define APPLICATION_LAYER_H

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
                      
unsigned char * buildControlPacket(int control_field,int fileSize, const char* fileName,int *controlpacketSize);
unsigned char * readControlPacket(unsigned char* controlpacket,int packetSize,int* fileSize);
unsigned char * buildDataPacket(unsigned char sequence, unsigned char *data_field, int dataFieldSize);
#endif // APPLICATION_LAYER_H
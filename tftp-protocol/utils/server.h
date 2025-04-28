#ifndef SERVER_H
#define SERVER_H

#define BUFFER_SIZE 2048
#define TIMEOUT 5
#define MAX_RETRIES 5

#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>

extern struct sockaddr_in expectedClientAddress; // Declare as extern

void runServer(int argc, char **argv);
void packetHandle(Packet packet, struct sockaddr_in *clientAddress, socklen_t clientAddressLength, char* baseDir);
int sendErrPacket(int err, char* message, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
int recievePacket(int socketAddr, Packet *packet, struct sockaddr_in *clientAddress, socklen_t *clientAddressLength);
int sendPacket(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
int sendDataPacket(FILE *file, int blockNum, int *isLast, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
int sendAckPacket(int blockNum, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
void handleReadRquest(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);
void handleWriteRquest(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength);

#endif
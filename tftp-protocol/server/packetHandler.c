#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int recievePacket(int socketAddr, Packet *packet, struct sockaddr_in *clientAddress, socklen_t *clientAddressLength) {
    unsigned char *buffer = calloc(BUFFER_SIZE, sizeof(unsigned char));
    int c = recvfrom(socketAddr, buffer, BUFFER_SIZE, 0, (struct sockaddr *)clientAddress, clientAddressLength);
    if (c < 0 && errno != EAGAIN) {
        printf("Transfer killed\n");
        exit(1);
    }
    if (c >= 4)
        *packet = convertToPacket(buffer, c);
    free(buffer);
    return c;
}

int sendPacket(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    int len, c;
    unsigned char *buffer = convertToBuffer(packet, &len);
    if ((c = sendto(socketAddr, buffer, len, 0, (struct sockaddr *)clientAddress, clientAddressLength)) < 0) {
        printf("Sending packet error\n");
    }
    free(buffer);
    return c;
}

int sendDataPacket(FILE *file, int blockNum, int *isLast, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    unsigned char *buffer = calloc(512, sizeof(unsigned char));
    fseek(file, (blockNum - 1) * 512, SEEK_SET);
    int len = fread(buffer, 1, 512, file);
    if (len < 512) {
        *isLast = 1;
    }
    buffer = realloc(buffer, len);
    Packet packet;
    packet.type = DataPacket;
    packet.content.DataPacket.block = blockNum;
    packet.content.DataPacket.data = calloc(len, sizeof(unsigned char));
    memcpy(packet.content.DataPacket.data, buffer, len);
    packet.content.DataPacket.dataLen = len;
    packet.content.DataPacket.op = DATA;
    free(buffer);
    int c = sendPacket(&packet, socketAddr, clientAddress, clientAddressLength);
    if (c < 0) {
        printf("Transfer killed\n");
        exit(1);
    }
    free(packet.content.DataPacket.data); // Free allocated memory
    return c;
}

int sendAckPacket(int blockNum, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    Packet packet;
    packet.type = AckPacket;
    packet.content.AckPacket.block = blockNum;
    packet.content.AckPacket.op = ACK;
    int c = sendPacket(&packet, socketAddr, clientAddress, clientAddressLength);
    if (c < 0) {
        printf("Transfer killed\n");
        exit(1);
    }
    return c;
}

int sendErrPacket(int err, char *message, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    Packet packet;
    packet.type = ErrorPacket;
    packet.content.ErrorPacket.op = ERR;
    packet.content.ErrorPacket.ec = err;
    packet.content.ErrorPacket.messageLen = strlen(message);
    packet.content.ErrorPacket.message = calloc(packet.content.ErrorPacket.messageLen + 1, sizeof(char));
    memcpy(packet.content.ErrorPacket.message, message, packet.content.ErrorPacket.messageLen);
    int c = sendPacket(&packet, socketAddr, clientAddress, clientAddressLength);
    free(packet.content.ErrorPacket.message);
    if (c < 0) {
        printf("Transfer killed\n");
        exit(1);
    }
    return c;
}

void handleReadRquest(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    FILE *file = fopen(packet->content.IOPacket.filename, "rb");
    if (file == NULL) {
        sendErrPacket(FILE_NOT_FOUND, "File not found", socketAddr, clientAddress, clientAddressLength);
        exit(1);
    }
    int isLast = 0, blockNumber = 0;
    while (!isLast) {
        blockNumber++;
        for (int i = 0; i <= MAX_RETRIES; i++) {
            if (i == MAX_RETRIES) {
                printf("Transfer Timed Out\n");
                fclose(file);
                exit(1);
            }
            int c = sendDataPacket(file, blockNumber, &isLast, socketAddr, clientAddress, clientAddressLength);
            Packet response;
            c = recievePacket(socketAddr, &response, clientAddress, &clientAddressLength);
            if (clientAddress->sin_addr.s_addr != expectedClientAddress.sin_addr.s_addr ||
                clientAddress->sin_port != expectedClientAddress.sin_port) {
                sendErrPacket(WRONG_TID, "Wrong client", socketAddr, clientAddress, clientAddressLength);
                continue;
            }
            if (c >= 0 && c < 4) {
                printf("Invalid message received\n");
                sendErrPacket(NOT_DEFINED, "Invalid packet", socketAddr, clientAddress, clientAddressLength);
                fclose(file);
                exit(1);
            }
            if (c >= 4 && response.content.AckPacket.block == blockNumber)
                break;
            if (response.type == ErrorPacket) {
                printf("Error occurred while transferring data from client side\n");
                fclose(file);
                exit(1);
            }
        }
    }
    fclose(file);
}

void handleWriteRquest(Packet *packet, int socketAddr, struct sockaddr_in *clientAddress, socklen_t clientAddressLength) {
    FILE *file = fopen(packet->content.IOPacket.filename, "wb");
    if (file == NULL) {
        sendErrPacket(FILE_NOT_FOUND, "File not found", socketAddr, clientAddress, clientAddressLength);
        exit(1);
    }
    int isLast = 0, blockNumber = 0;
    while (!isLast) {
        Packet response;
        for (int i = 0; i <= MAX_RETRIES; i++) {
            if (i == MAX_RETRIES) {
                printf("Transfer Timed Out\n");
                fclose(file);
                exit(1);
            }
            int c = sendAckPacket(blockNumber, socketAddr, clientAddress, clientAddressLength);
            c = recievePacket(socketAddr, &response, clientAddress, &clientAddressLength);
            if (clientAddress->sin_addr.s_addr != expectedClientAddress.sin_addr.s_addr ||
                clientAddress->sin_port != expectedClientAddress.sin_port) {
                sendErrPacket(WRONG_TID, "Wrong client", socketAddr, clientAddress, clientAddressLength);
                continue;
            }
            if (c >= 0 && c < 4) {
                printf("Invalid message received\n");
                sendErrPacket(NOT_DEFINED, "Invalid packet", socketAddr, clientAddress, clientAddressLength);
                fclose(file);
                exit(1);
            }
            if (c >= 4 && response.content.DataPacket.block == blockNumber + 1)
                break;
        }
        if (response.type == ErrorPacket) {
            printf("Error occurred while transferring data from client side\n");
            fclose(file);
            exit(1);
        }
        if (response.content.DataPacket.dataLen < 512)
            isLast = 1;
        blockNumber++;
        fwrite(response.content.DataPacket.data, 1, response.content.DataPacket.dataLen, file);
        free(response.content.DataPacket.data); // Free allocated memory
    }
    sendAckPacket(blockNumber, socketAddr, clientAddress, clientAddressLength);
    fclose(file);
}

void packetHandle(Packet packet, struct sockaddr_in *clientAddress, socklen_t clientAddressLength, char *baseDir) {
    memcpy(&expectedClientAddress, clientAddress, clientAddressLength);

    int childSocket;
    struct protoent *pp;
    struct timeval time;

    if ((pp = getprotobyname("udp")) == 0) {
        printf("Error getting port\n");
        exit(1);
    }

    if ((childSocket = socket(AF_INET, SOCK_DGRAM, pp->p_proto)) < 0) {
        printf("Error getting socket\n");
        exit(1);
    }

    time.tv_sec = TIMEOUT;
    time.tv_usec = 0;

    if (setsockopt(childSocket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        printf("Error setting opt\n");
        exit(1);
    }

    char *requestedDir = calloc(BUFFER_SIZE, sizeof(char));
    if (realpath(packet.content.IOPacket.filename, requestedDir) < 0 || strncmp(requestedDir, baseDir, strlen(baseDir)) != 0) {
        sendErrPacket(ACCESS_VOILATION, "Not allowed to access this file/folder", childSocket, clientAddress, clientAddressLength);
        free(requestedDir);
        exit(1);
    }
    packet.content.IOPacket.fileLen = strlen(requestedDir) + 1;
    packet.content.IOPacket.filename = realloc(packet.content.IOPacket.filename, packet.content.IOPacket.fileLen);
    memcpy(packet.content.IOPacket.filename, requestedDir, packet.content.IOPacket.fileLen);
    free(requestedDir);

    if (packet.content.IOPacket.op == RRQ)
        handleReadRquest(&packet, childSocket, clientAddress, clientAddressLength);
    else
        handleWriteRquest(&packet, childSocket, childAddress, clientAddressLength);
    exit(0);
}
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int serverSocket;
struct sockaddr_in serverAddress;
struct sockaddr_in expectedClientAddress;

void childExit(int signal) {
    int status;
    pid_t pid = wait(&status);
    printf("Exiting child process with pid: %d and it was %s\n", pid, (status ? "Unsuccessful" : "Successful"));
    fflush(stdout);
}

void intialiseSocket(char *port) {
    int SERVER_PORT = atoi(port);
    if (SERVER_PORT == 0) {
        printf("Use correct port number\n");
        exit(1);
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    serverSocket = socket(PF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        printf("Server socket initialization failed!\n");
        exit(1);
    }

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Server socket binding failed!\n");
        exit(1);
    }
}

void runServer(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage:\n\t %s [baseDirectory] [serverPortNumber]\n", argv[0]);
        exit(1);
    }

    intialiseSocket(argv[2]);
    signal(SIGCHLD, (void *)childExit);

    char *realPath = calloc(2048, sizeof(char));
    if (realpath(argv[1], realPath) < 0 || chdir(realPath) < 0) {
        printf("Server cannot change to given directory\n");
        exit(1);
    }

    for (;;) {
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        unsigned char *buffer = calloc(BUFFER_SIZE, sizeof(unsigned char));

        int length = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
        if (length < 4) {
            printf("Receive Packet Error\n");
            free(buffer);
            continue;
        }

        Packet request = convertToPacket(buffer, length);
        free(buffer);

        if (request.type != IOPacket) {
            sendErrPacket(ILLEGAL_OP, "Only write and read request supported", serverSocket, &clientAddress, clientAddressLength);
            continue;
        }

        if (fork() == 0) {
            packetHandle(request, &clientAddress, clientAddressLength, realPath);
            exit(0);
        }
    }
    free(realPath);
}

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
    if (c < 0) {
        printf("Transfer killed\n");
        exit(1);
    }
    free(packet.content.ErrorPacket.message);
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
    }
    int c = sendAckPacket(blockNumber, socketAddr, clientAddress, clientAddressLength);
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
        handleWriteRquest(&packet, childSocket, clientAddress, clientAddressLength);
    exit(0);
}

Packet convertToPacket(unsigned char *buffer, int length) {
    Packet packet;
    unsigned short int opcode = ntohs(*(unsigned short*)buffer);
    
    if (opcode == RRQ || opcode == WRQ) {
        packet.type = IOPacket;
        packet.content.IOPacket.op = opcode;
        char *pos = (char *)(buffer + 2);
        packet.content.IOPacket.filename = strdup(pos);
        packet.content.IOPacket.fileLen = strlen(pos) + 1;
        pos += packet.content.IOPacket.fileLen;
        packet.content.IOPacket.mode = strdup(pos);
        packet.content.IOPacket.modeLen = strlen(pos) + 1;
    } else if (opcode == DATA) {
        packet.type = DataPacket;
        packet.content.DataPacket.op = DATA;
        packet.content.DataPacket.block = ntohs(*(unsigned short*)(buffer + 2));
        packet.content.DataPacket.dataLen = length - 4;
        packet.content.DataPacket.data = malloc(packet.content.DataPacket.dataLen);
        memcpy(packet.content.DataPacket.data, buffer + 4, packet.content.DataPacket.dataLen);
    } else if (opcode == ACK) {
        packet.type = AckPacket;
        packet.content.AckPacket.op = ACK;
        packet.content.AckPacket.block = ntohs(*(unsigned short*)(buffer + 2));
    } else if (opcode == ERR) {
        packet.type = ErrorPacket;
        packet.content.ErrorPacket.op = ERR;
        packet.content.ErrorPacket.ec = ntohs(*(unsigned short*)(buffer + 2));
        packet.content.ErrorPacket.messageLen = length - 4;
        packet.content.ErrorPacket.message = malloc(packet.content.ErrorPacket.messageLen + 1);
        memcpy(packet.content.ErrorPacket.message, buffer + 4, packet.content.ErrorPacket.messageLen);
        packet.content.ErrorPacket.message[packet.content.ErrorPacket.messageLen] = '\0';
    }
    return packet;
}

unsigned char *convertToBuffer(Packet *packet, int *length) {
    unsigned char *buffer = NULL;
    if (packet->type == IOPacket) {
        *length = 2 + packet->content.IOPacket.fileLen + packet->content.IOPacket.modeLen;
        buffer = malloc(*length);
        *(unsigned short*)buffer = htons(packet->content.IOPacket.op);
        char *pos = (char *)(buffer + 2);
        strcpy(pos, packet->content.IOPacket.filename);
        pos += packet->content.IOPacket.fileLen;
        strcpy(pos, packet->content.IOPacket.mode);
    } else if (packet->type == DataPacket) {
        *length = 4 + packet->content.DataPacket.dataLen;
        buffer = malloc(*length);
        *(unsigned short*)buffer = htons(DATA);
        *(unsigned short*)(buffer + 2) = htons(packet->content.DataPacket.block);
        memcpy(buffer + 4, packet->content.DataPacket.data, packet->content.DataPacket.dataLen);
    } else if (packet->type == AckPacket) {
        *length = 4;
        buffer = malloc(*length);
        *(unsigned short*)buffer = htons(ACK);
        *(unsigned short*)(buffer + 2) = htons(packet->content.AckPacket.block);
    } else if (packet->type == ErrorPacket) {
        *length = 4 + packet->content.ErrorPacket.messageLen + 1;
        buffer = malloc(*length);
        *(unsigned short*)buffer = htons(ERR);
        *(unsigned short*)(buffer + 2) = htons(packet->content.ErrorPacket.ec);
        memcpy(buffer + 4, packet->content.ErrorPacket.message, packet->content.ErrorPacket.messageLen);
        buffer[*length - 1] = '\0';
    }
    return buffer;
}

int main(int argc, char** argv) {
    runServer(argc, argv);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TFTP_PORT 69
#define MODE "octet"
#define MAXL 100

void send_ack(int sock, struct sockaddr_in *serv_addr, uint16_t block_num) {
    char packet[4];
    uint16_t opcode = htons(4);
    uint16_t block_len = htons(block_num);
    memcpy(packet, &opcode, 2);
    memcpy(packet + 2, &block_len, 2);
    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr*)serv_addr, sizeof(struct sockaddr_in));
}

void send_data(int sock, struct sockaddr_in *addr, uint16_t block_num, const char *data, size_t len) {
    char packet[516];
    uint16_t opcode = htons(3);
    memcpy(packet, &opcode, 2);
    uint16_t blocknum = htons(block_num);
    memcpy(packet + 2, &blocknum, 2);
    memcpy(packet + 4, data, len);
    sendto(sock, packet, 4 + len, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
}

void sendWRQ(int sock, struct sockaddr_in *server, const char *filename) {
    char packet[516];
    int position = 0;
    uint16_t opcode = htons(2);
    memcpy(packet + position, &opcode, 2);
    position += 2;

    strcpy(packet + position, filename);
    position += strlen(filename) + 1;
    strcpy(packet + position, MODE);
    position += strlen(MODE) + 1;
    sendto(sock, packet, position, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in));
}

void sendRRQ(int sock, struct sockaddr_in *server, const char *filename) {
    char packet[516];
    uint16_t opcode = htons(1);
    memcpy(packet, &opcode, 2);
    strcpy(packet + 2, filename);
    strcpy(packet + 2 + strlen(filename) + 1, MODE);
    size_t len = 4 + strlen(filename) + strlen(MODE);
    sendto(sock, packet, len, 0, (struct sockaddr *)server, sizeof(struct sockaddr_in));
}

void PutFile(int sock, struct sockaddr_in *server_addr, const char *fileName) {
    sendWRQ(sock, server_addr, fileName);
    printf("Sent WRQ for file: %s\n", fileName);

    struct sockaddr_in transfer_addr;
    socklen_t addr_len = sizeof(transfer_addr);
    char buffer[516];
    ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&transfer_addr, &addr_len);

    if (recv_len < 4) {
        fprintf(stderr, "Invalid acknowledgment packet\n");
        return;
    }

    uint16_t opcode = ntohs(*(uint16_t*)(buffer));
    uint16_t block_num_ack = ntohs(*(uint16_t*)(buffer + 2));

    if (opcode == 5) {
        fprintf(stderr, "Server Error: %s\n", buffer + 4);
        return;
    } else if (opcode != 4 || block_num_ack != 0) {
        fprintf(stderr, "Unexpected opcode or block number (opcode: %d, block_num: %d)\n", opcode, block_num_ack);
        return;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    uint16_t block_num = 1;
    while (1) {
        char data[512];
        fseek(file, (block_num-1) * 512, SEEK_SET);
        size_t read_len = fread(data, 1, 512, file);

        send_data(sock, &transfer_addr, block_num, data, read_len);
        printf("Sent DATA packet for block: %d\n", block_num);

        recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&transfer_addr, &addr_len);
        if (recv_len < 4) {
            fprintf(stderr, "Invalid ACK packet\n");
            break;
        }

        opcode = ntohs(*(uint16_t*)(buffer));
        block_num_ack = ntohs(*(uint16_t*)(buffer + 2));

        if (opcode == 5) {
            fprintf(stderr, "Server Error: %s\n", buffer + 4);
            break;
        } else if (opcode != 4 || block_num_ack != block_num) {
            fprintf(stderr, "Unexpected ACK (opcode: %d, block_num: %d)\n", opcode, block_num_ack);
            break;
        }

        printf("Received ACK for block: %d\n", block_num);
        block_num++;

        if (read_len < 512) {
            printf("End of transfer\n");
            break;
        }
    }

    fclose(file);
    fflush(stdout);
}

void getFile(int sock, struct sockaddr_in *server, const char *filename) {
    sendRRQ(sock, server, filename);
    printf("Sent RRQ for file: %s\n", filename);

    struct sockaddr_in transfer_addr;
    socklen_t addr_len = sizeof(transfer_addr);
    char buffer[516];
    FILE *file = fopen(filename, "wb");

    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    uint16_t expected_block = 1;
    while (1) {
        ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&transfer_addr, &addr_len);
        if (recv_len < 4) {
            printf("Received packet with invalid length\n");
            break;
        }

        uint16_t opcode = ntohs(*(uint16_t *)(buffer));
        if (opcode == 3) {
            uint16_t blocknum = ntohs(*(uint16_t *)(buffer + 2));
            printf("Received DATA packet for block: %d, length: %zd\n", blocknum, recv_len);

            if (blocknum == expected_block) {
                size_t data_len = recv_len - 4;
                fwrite(buffer + 4, 1, data_len, file);
                printf("Written %zu bytes to file\n", data_len);
                send_ack(sock, &transfer_addr, blocknum);
                printf("Sent ACK for block: %d\n", blocknum);

                expected_block++;
                if (data_len < 512) {
                    printf("End of file transfer\n");
                    break;
                }
            } else {
                printf("Unexpected block number: %d (expected: %d)\n", blocknum, expected_block);
            }
        } else if (opcode == 5) {
            fprintf(stderr, "Error: %s\n", buffer + 4);
            break;
        } else {
            printf("Unexpected opcode: %d\n", opcode);
            break;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        exit(EXIT_FAILURE);
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    printf("put/get fileName    or quit \n");
    char input[MAXL];
    while (1) {
        printf("tftp> ");
        if (!fgets(input, MAXL, stdin)) { break; }

        char cmd[MAXL], fileName[MAXL];
        if (sscanf(input, "%s %s", cmd, fileName) < 1) { continue; }
        if (strcmp(cmd, "get") == 0) {
            getFile(sock, &server_addr, fileName);
        } else if (strcmp(cmd, "put") == 0) {
            PutFile(sock, &server_addr, fileName);
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else {
            printf("Command not recognized %s\n", cmd);
        }
    }
    close(sock);
    return 0;
}
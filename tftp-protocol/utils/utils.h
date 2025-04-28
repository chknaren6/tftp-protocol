#ifndef UTILS_H
#define UTILS_H

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERR 5

#define NOT_DEFINED 0
#define FILE_NOT_FOUND 1
#define ACCESS_VOILATION 2
#define DISK_FULL 3
#define ILLEGAL_OP 4
#define WRONG_TID 5
#define ALREADY_EXISTS 6
#define UNKNOWN_USER 7

typedef enum PacketType
{
    IOPacket,
    DataPacket,
    AckPacket,
    ErrorPacket
} PacketType;

typedef struct Packet
{
    PacketType type;
    union
    {
        struct IOPacket
        {
            unsigned short int op;
            unsigned int fileLen, modeLen;
            char *filename;
            char *mode;
        } IOPacket;

        struct DataPacket
        {
            unsigned short int op, block;
            unsigned int dataLen;
            unsigned char *data;
        } DataPacket;

        struct AckPacket
        {
            unsigned short int op, block;
        } AckPacket;

        struct ErrorPacket
        {
            unsigned short int op, ec;
            unsigned int messageLen;
            char *message;
        } ErrorPacket;
    } content;
} Packet;

unsigned char *convertToBuffer(Packet *packet, int *length);
Packet convertToPacket(unsigned char *buffer, int length);
void debugPacket(Packet *packet);

#endif
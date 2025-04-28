#include "utils.h"
#include "socket.h"
#include <string.h>
#include <stdlib.h>

unsigned char *convertToBuffer(Packet *packet, int *length)
{
    unsigned char *result;
    switch (packet->type)
    {
    case IOPacket:
    {
        *length = 2 + packet->content.IOPacket.fileLen + packet->content.IOPacket.modeLen;
        result = calloc(*length, sizeof(unsigned char));
        memset(result, 0, *length);
        short int op = htons(packet->content.IOPacket.op);
        memcpy(result, &op, 2);
        memcpy(result + 2, packet->content.IOPacket.filename, packet->content.IOPacket.fileLen);
        memcpy(result + 2 + packet->content.IOPacket.fileLen, packet->content.IOPacket.mode, packet->content.IOPacket.modeLen);
        break;
    }
    case DataPacket:
    {
        *length = 2 + 2 + packet->content.DataPacket.dataLen;
        result = calloc(*length, sizeof(unsigned char));
        memset(result, 0, *length);
        short int op = htons(packet->content.DataPacket.op), block = htons(packet->content.DataPacket.block);
        memcpy(result, &op, 2);
        memcpy(result + 2, &block, 2);
        memcpy(result + 2 + 2, packet->content.DataPacket.data, packet->content.DataPacket.dataLen);
        break;
    }
    case AckPacket:
    {
        *length = 2 + 2;
        result = calloc(*length, sizeof(unsigned char));
        memset(result, 0, *length);
        short int op = htons(packet->content.AckPacket.op), block = htons(packet->content.AckPacket.block);
        memcpy(result, &op, 2);
        memcpy(result + 2, &block, 2);
        break;
    }
    case ErrorPacket:
    {
        *length = 2 + 2 + packet->content.ErrorPacket.messageLen;
        result = calloc(*length, sizeof(unsigned char));
        memset(result, 0, *length);
        short int op = htons(packet->content.ErrorPacket.op), ec = htons(packet->content.ErrorPacket.ec);
        memcpy(result, &op, 2);
        memcpy(result + 2, &ec, 2);
        memcpy(result + 4, packet->content.ErrorPacket.message, packet->content.ErrorPacket.messageLen);
        break;
    }
    default:
        break;
    }
    return result;
}

Packet convertToPacket(unsigned char *buffer, int length)
{
    unsigned short int op;
    memcpy(&op, (unsigned short int *)(buffer), 2);
    op = ntohs(op);
    Packet packet;
    switch (op)
    {
    case RRQ:
    case WRQ:
    {
        packet.type = IOPacket;
        packet.content.IOPacket.op = op;
        packet.content.IOPacket.fileLen = strlen(buffer + 2) + 1;
        packet.content.IOPacket.filename = calloc(sizeof(unsigned char), packet.content.IOPacket.fileLen);
        memset(packet.content.IOPacket.filename, 0, packet.content.IOPacket.fileLen);
        memcpy(packet.content.IOPacket.filename, (buffer + 2), packet.content.IOPacket.fileLen);
        packet.content.IOPacket.modeLen = strlen(buffer + 2 + packet.content.IOPacket.fileLen) + 1;
        packet.content.IOPacket.mode = calloc(sizeof(unsigned char), packet.content.IOPacket.modeLen);
        memset(packet.content.IOPacket.mode, 0, packet.content.IOPacket.modeLen);
        memcpy(packet.content.IOPacket.mode, (buffer + 2 + packet.content.IOPacket.fileLen), packet.content.IOPacket.modeLen);
        break;
    }
    case DATA:
    {
        packet.type = DataPacket;
        packet.content.DataPacket.op = op;
        unsigned short int block;
        memcpy(&block, (unsigned short int *)(buffer + 2), 2);
        block = ntohs(block);
        packet.content.DataPacket.block = block;
        packet.content.DataPacket.dataLen = length - 2 - 2;
        packet.content.DataPacket.data = calloc(sizeof(unsigned char), packet.content.DataPacket.dataLen);
        memset(packet.content.DataPacket.data, 0, packet.content.DataPacket.dataLen);
        memcpy(packet.content.DataPacket.data, buffer + 2 + 2, packet.content.DataPacket.dataLen);
        break;
    }
    case ACK:
    {
        packet.type = AckPacket;
        packet.content.AckPacket.op = op;
        unsigned short int block;
        memcpy(&block, (unsigned short int *)(buffer + 2), 2);
        block = ntohs(block);
        packet.content.AckPacket.block = block;
        break;
    }
    case ERR:
    {
        packet.type = ErrorPacket;
        packet.content.ErrorPacket.op = op;
        unsigned short int ec;
        memcpy(&ec, (unsigned short int *)(buffer + 2), 2);
        ec = ntohs(ec);
        packet.content.ErrorPacket.ec = ec;
        packet.content.ErrorPacket.messageLen = strlen(buffer + 2) + 1;
        packet.content.ErrorPacket.message = calloc(sizeof(unsigned char), packet.content.ErrorPacket.messageLen);
        memset(packet.content.ErrorPacket.message, 0, packet.content.ErrorPacket.messageLen);
        memcpy(packet.content.ErrorPacket.message, (buffer + 2), packet.content.ErrorPacket.messageLen);
        break;
    }
    default:
        break;
    }
    return packet;
}
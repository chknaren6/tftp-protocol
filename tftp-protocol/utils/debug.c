#include "utils.h"
#include <stdio.h>

void debugPacket(Packet *packet)
{
    switch (packet->type)
    {
    case IOPacket:
        printf("%d ", packet->content.IOPacket.op);
        printf("%d ", packet->content.IOPacket.fileLen);
        printf("%s ", packet->content.IOPacket.filename);
        printf("%d ", packet->content.IOPacket.modeLen);
        printf("%s ", packet->content.IOPacket.mode);
        printf("\n");
        break;
    case DataPacket:
        printf("%d ", packet->content.DataPacket.op);
        printf("%d ", packet->content.DataPacket.block);
        printf("%d ", packet->content.DataPacket.dataLen);
        printf("\n");
        break;
    case AckPacket:
        printf("%d ", packet->content.AckPacket.op);
        printf("%d ", packet->content.AckPacket.block);
        printf("\n");
        break;
    case ErrorPacket:
        printf("%d ", packet->content.ErrorPacket.op);
        printf("%d ", packet->content.ErrorPacket.ec);
        printf("%d ", packet->content.ErrorPacket.messageLen);
        printf("%s ", packet->content.ErrorPacket.message);
        printf("\n");
        break;
    default:
        break;
    }
    return;
}
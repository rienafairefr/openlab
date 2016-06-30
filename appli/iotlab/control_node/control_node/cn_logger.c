#include "iotlab_serial.h"
#include "cn_logger.h"

static void cn_logger_packet_free(iotlab_packet_t *packet);

static iotlab_packet_t pkt;
iotlab_packet_t *cn_logger_pkt = NULL;

void cn_logger_start()
{
    // Not using a fifo, so custom 'free' function
    pkt.free = cn_logger_packet_free;
    pkt.free(&pkt);
}


static void cn_logger_packet_free(iotlab_packet_t *packet)
{
    packet_reset((packet_t *)packet, IOTLAB_SERIAL_HEADER_SIZE);
    cn_logger_pkt = packet;
}


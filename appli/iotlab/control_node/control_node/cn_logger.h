#ifndef CN_LOGGER_H
#define CN_LOGGER_H

#include <string.h>
#include "printf.h"
#include "iotlab_packet.h"
#include "constants.h"

extern iotlab_packet_t *cn_logger_pkt;

void cn_logger_start();

/** Send logger message */
#define cn_logger(level, msg, args...) do {                                    \
                                                                               \
    packet_t *pkt = (packet_t *)cn_logger_pkt;                                 \
    if (pkt == NULL)                                                           \
        break;                                                                 \
                                                                               \
    pkt->data[0] = (level);                                                    \
    snprintf((char *)&pkt->data[1], IOTLAB_SERIAL_DATA_MAX_SIZE - 1,           \
            (msg) , ##args);                                                   \
    pkt->length = 2 + strlen((char *)&pkt->data[1]);                           \
                                                                               \
    cn_logger_pkt = NULL;                                                      \
    if (iotlab_serial_send_frame(LOGGER_FRAME, (iotlab_packet_t *)pkt))        \
        cn_logger_pkt = (iotlab_packet_t *)pkt;  /* Failed, restore packet */  \
                                                                               \
} while (0);


#endif // CN_LOGGER_H

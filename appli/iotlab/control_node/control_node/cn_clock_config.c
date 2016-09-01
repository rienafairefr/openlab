#include "constants.h"
#include "iotlab_serial.h"
#include "platform.h"
#include "soft_timer.h"
#include "iotlab_time.h"
#include "cn_clock_config.h"

#include "cn_meas_pkt.h"

#define MEASURES_PRIORITY 0
#define CN_CLOCK_NUM_PKTS (8)

static iotlab_packet_t* p;
static uint32_t measure_size = 4 * sizeof(uint32_t) + 2 *  sizeof(uint64_t);

iotlab_packet_t meas_pkts[CN_CLOCK_NUM_PKTS];
iotlab_packet_queue_t measures_queue;

void cn_clock_start()
{
   iotlab_packet_init_queue(&measures_queue, meas_pkts, CN_CLOCK_NUM_PKTS);
}

void cn_clock_handler(struct iotlab_time_config *config)
{
    struct soft_timer_timeval timestamp;
    iotlab_packet_t *packet;

    int64_t time0 = config->time0;
    struct soft_timer_timeval time_ref = config->unix_time_ref;
    int32_t kfreq = config->kfrequency;
    int64_t last_set_time = config->last_set_time;

    iotlab_time_extend_relative(&timestamp, last_set_time);
    
    packet = cn_meas_pkt_lazy_alloc(&measures_queue, p, &timestamp);

    if ((p = packet) == NULL)
        return;  // alloc failed, drop this measure

    /* Add measure time + config */
    struct cn_meas clock_config[] = {{&time0, sizeof(uint64_t)},
                              {&time_ref, sizeof(struct soft_timer_timeval)},
                              {&kfreq, sizeof(uint32_t)},
                              {&last_set_time, sizeof(uint64_t)},
                              {NULL, 0}};

    cn_meas_pkt_add_measure(packet, &timestamp, measure_size, clock_config);

    flush_current_clock();
}

void flush_current_clock()
{
    cn_meas_pkt_flush(&p, CLOCK_FRAME);
}

#include "constants.h"
#include "cn_event.h"
#include "iotlab_serial.h"
#include "iotlab_gpio.h"
#include "platform.h"
#include "soft_timer.h"
#include "iotlab_time.h"

#include "cn_meas_pkt.h"

#define MEASURES_PRIORITY 0
#define CN_EVENT_NUM_PKTS (8)

enum events_t {
    EVENT_PPS = 0,
};

enum config_gpio_t {
    CONFIG_GPIO_PPS = 1 << 0,
};


static iotlab_packet_t* p;
static uint32_t measure_size = 3 * sizeof(uint32_t);

iotlab_packet_t meas_pkts[CN_EVENT_NUM_PKTS];
iotlab_packet_queue_t measures_queue;

static int32_t config_gpio_event(uint8_t cmd_type, iotlab_packet_t* packet);
static void event_handler(uint32_t ticks, uint32_t value, uint32_t source);
static void pps_handler_irq(handler_arg_t arg);
static void pps_handler(handler_arg_t arg);


void cn_event_start()
{

   iotlab_packet_init_queue(&measures_queue, meas_pkts, CN_EVENT_NUM_PKTS);

   static iotlab_serial_handler_t handler = {
       .cmd_type = CONFIG_GPIO,
       .handler = config_gpio_event,
   };
   iotlab_serial_register_handler(&handler);
}

static int32_t config_gpio_event(uint8_t cmd_type, iotlab_packet_t* packet)
{
     /*
     * Expected packet format is (length:2B:
     *      * Start / Stop mode         [1B]
     *      * GPIOs (PPS|GPIO1|GPIO2)    [1B]
     */

    packet_t* pkt = (packet_t*) packet;
    if (pkt->length != 2)
        return 1;

    uint8_t mode  = pkt->data[0];
    uint8_t gpios = pkt->data[1];
    (void)gpios;

    if (mode == STOP) {
        gpio_disable_irq(&gpio_config[3]);
        return 0;
    } else {
        gpio_enable_irq(&gpio_config[3], IRQ_RISING, pps_handler_irq, NULL);
        return 0;
    }

}

static void pps_handler_irq(handler_arg_t arg)
{
    (void) arg;
    uint32_t timestamp = soft_timer_time();
    event_post(EVENT_QUEUE_APPLI, pps_handler, (handler_arg_t)timestamp);
}

static void pps_handler(handler_arg_t arg)
{
    uint32_t timestamp = (uint32_t)arg;
    event_handler(timestamp, 1, EVENT_PPS);
}

static void event_handler(uint32_t ticks, uint32_t value, uint32_t source)
{
    struct soft_timer_timeval timestamp;
    iotlab_packet_t *packet;

    iotlab_time_extend_relative(&timestamp, ticks);

    packet = cn_meas_pkt_lazy_alloc(&measures_queue, p, &timestamp);
    if ((p = packet) == NULL)
        return;  // alloc failed, drop this measure

    /* Add measure time + event (value|source) */
    struct cn_meas event[] = {{&value, sizeof(uint32_t)},
                              {&source, sizeof(uint32_t)},
                              {NULL, 0}};
    int send = cn_meas_pkt_add_measure(
            packet, &timestamp, measure_size, event);

    if (send)
        flush_current_event();
}

void flush_current_event()
{
    cn_meas_pkt_flush(&p, EVENT_FRAME);
}

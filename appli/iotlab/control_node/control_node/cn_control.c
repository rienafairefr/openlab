#include <string.h>
#include "platform.h"

#include "iotlab_serial.h"
#include "iotlab_time.h"
#include "iotlab_leds.h"
#include "iotlab_leds_util.h"

#include "constants.h"
#include "cn_control.h"

/* for flush in reset_time */
#include "cn_consumption.h"
#include "cn_radio.h"
#include "cn_event.h"

#define CN_CONTROL_NUM_ACKS (2)

static struct {

    uint16_t node_id;
    iotlab_packet_t acks_pkts[CN_CONTROL_NUM_ACKS];
    iotlab_packet_queue_t acks_queue;


} cn_control = {
    .node_id = 0,
};



static int32_t set_time(uint8_t cmd_type, iotlab_packet_t *pkt);
static void do_set_time(handler_arg_t arg);

static int32_t set_node_id(uint8_t cmd_type, iotlab_packet_t *pkt);

static soft_timer_t green_led_alarm;
static int32_t green_led_blink(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t green_led_on(uint8_t cmd_type, iotlab_packet_t *pkt);


void cn_control_start()
{
    iotlab_packet_init_queue(&cn_control.acks_queue,
            cn_control.acks_pkts, CN_CONTROL_NUM_ACKS);

    // Configure and register all handlers
    // set_time
    static iotlab_serial_handler_t handler_set_time = {
        .cmd_type = SET_TIME,
        .handler = set_time,
    };
    iotlab_serial_register_handler(&handler_set_time);

    // set_node_id
    static iotlab_serial_handler_t handler_set_node_id = {
        .cmd_type = SET_NODE_ID,
        .handler = set_node_id,
    };
    iotlab_serial_register_handler(&handler_set_node_id);


    // green led control
    static iotlab_serial_handler_t handler_green_led_blink = {
        .cmd_type = GREEN_LED_BLINK,
        .handler = green_led_blink,
    };
    iotlab_serial_register_handler(&handler_green_led_blink);
    static iotlab_serial_handler_t handler_green_led_on = {
        .cmd_type = GREEN_LED_ON,
        .handler = green_led_on,
    };
    iotlab_serial_register_handler(&handler_green_led_on);
}

static struct {
        struct soft_timer_timeval unix_time;
        uint32_t t0;
} set_time_aux;


static int32_t set_time(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    if (8 != pkt->length)
        return 1;
    /* Save time as soon as possible */
    set_time_aux.t0 = packet->timestamp;
    /* copy unix time from pkt */
    memcpy(&set_time_aux.unix_time, pkt->data, pkt->length);

    /* alloc the ack frame */
    iotlab_packet_t *ack_pkt = iotlab_serial_packet_alloc(&cn_control.acks_queue);
    if (!ack_pkt)
        return 1;
    ((packet_t *)ack_pkt)->data[0] = SET_TIME;
    ((packet_t *)ack_pkt)->length = 1;

    if (event_post(EVENT_QUEUE_APPLI, do_set_time, ack_pkt)) {
        iotlab_packet_call_free(ack_pkt);
        return 1;
    }

    return 0;
}

static void do_set_time(handler_arg_t arg)
{
    iotlab_packet_t *ack_pkt = (iotlab_packet_t *)arg;

    /*
     * Flush measures packets
     */
#ifdef IOTLAB_CN
    flush_current_consumption_measures();
#endif
    flush_current_rssi_measures();
    flush_current_event();

    // Send the update frame
    if (iotlab_serial_send_frame(ACK_FRAME, ack_pkt)) {
        iotlab_packet_call_free(ack_pkt);
        return;
    }

    iotlab_time_set_time(set_time_aux.t0, &set_time_aux.unix_time);
}


uint16_t cn_control_node_id()
{
    return cn_control.node_id;
}

static int32_t set_node_id(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    if (2 != pkt->length)
        return 1;

    memcpy(&cn_control.node_id, pkt->data, pkt->length);
    return 0;
}


/*
 * green led control
 */

int32_t green_led_blink(uint8_t cmd_type, iotlab_packet_t *packet)
{
    (void)packet;
    leds_blink(&green_led_alarm, soft_timer_s_to_ticks(1), GREEN_LED);
    return 0;
}

int32_t green_led_on(uint8_t cmd_type, iotlab_packet_t *packet)
{
    (void)packet;
    leds_blink(&green_led_alarm, 0, GREEN_LED); // stop
    leds_on(GREEN_LED);

    return 0;
}


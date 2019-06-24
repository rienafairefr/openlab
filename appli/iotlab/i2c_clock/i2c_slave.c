#include <string.h>
#include "soft_timer.h"
#include "iotlab_time.h"

#include "iotlab_i2c_slave.h"
#include "iotlab_i2c/iotlab_i2c_.h"

#include "i2c_slave.h"

#include "debug.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG

void get_timestamp(struct soft_timer_timeval *time)
{
    time->tv_sec = 0;
    time->tv_sec = 0;
    iotlab_time_extend_relative(time, soft_timer_time());
}

static void tx_event(struct iotlab_i2c_handler_arg *arg)
{
    uint16_t event;
    memcpy(&event, arg->payload, sizeof(uint16_t));
    log_info("Got event %u", event);
}
static struct iotlab_i2c_handler tx_event_handler = {
    .header      = IOTLAB_I2C_TX_EVENT,
    .type        = IOTLAB_I2C_SLAVE_RX,
    .payload_len = sizeof(uint16_t),
    .handler     = (handler_t)tx_event,
    .next        = NULL,
};

static void rx_node_id(struct iotlab_i2c_handler_arg *arg)
{
    // Put saved timestamp in buffer
    uint16_t node_id = 42;
    memcpy(arg->payload, &node_id, sizeof(uint16_t));
    log_info("Send node_uid: %04x", node_id);
}
static struct iotlab_i2c_handler rx_node_id_handler = {
    .header      = IOTLAB_I2C_RX_NODE_ID,
    .type        = IOTLAB_I2C_SLAVE_TX,
    .payload_len = sizeof(uint16_t),
    .handler     = (handler_t)rx_node_id,
    .next        = NULL,
};


void i2c_slave_start()
{
    // register handlers
    iotlab_i2c_slave_register_handler(&tx_event_handler);
    iotlab_i2c_slave_register_handler(&rx_node_id_handler);

    iotlab_i2c_slave_register_timestamp_fct(get_timestamp);
    iotlab_i2c_slave_start();
}

void i2c_slave_stop()
{
    iotlab_i2c_slave_stop();
}

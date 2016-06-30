#include <stdint.h>

#include "iotlab_autotest_cn.h"
#include "iotlab_serial.h"

#include "constants.h"
#include "cn_autotest.h"

static int32_t gpio(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t tst_i2c2(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t ping_pong(uint8_t cmd_type, iotlab_packet_t *pkt);


void cn_autotest_start()
{
    // Configure and register all handlers
    static iotlab_serial_handler_t handler_pingpong = {
        .cmd_type = TEST_RADIO_PING_PONG,
        .handler = ping_pong,
    };
    iotlab_serial_register_handler(&handler_pingpong);

    static iotlab_serial_handler_t handler_gpio = {
        .cmd_type = TEST_GPIO,
        .handler = gpio,
    };
    iotlab_serial_register_handler(&handler_gpio);

    static iotlab_serial_handler_t handler_i2c2 = {
        .cmd_type = TEST_I2C2,
        .handler = tst_i2c2,
    };
    iotlab_serial_register_handler(&handler_i2c2);
}


int32_t ping_pong(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;

    if (3 != pkt->length)
        return 1;
    if (START == pkt->data[0])
        return cn_test_radio_pp_start(pkt->data[1], pkt->data[2]);
    else
        cn_test_radio_pp_stop();
    return 0;
}

int32_t gpio(uint8_t cmd_type, iotlab_packet_t *packet)
{
    if (START == ((packet_t *)packet)->data[0])
        cn_test_gpio_start();
    else
        cn_test_gpio_stop();
    return 0;
}

int32_t tst_i2c2(uint8_t cmd_type, iotlab_packet_t *packet)
{
    if (START == ((packet_t *)packet)->data[0])
        cn_test_i2c2_start();
    else
        cn_test_i2c2_stop();
    return 0;
}

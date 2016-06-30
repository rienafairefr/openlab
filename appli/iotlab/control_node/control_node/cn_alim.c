#include <string.h>
#include "platform.h"

#include "iotlab_serial.h"
#include "fiteco_lib_gwt.h"

#include "constants.h"
#include "cn_alim.h"

static struct {

    void (*pre_stop_cmd)();
    void (*post_start_cmd)();

} cn_alim = {
    .pre_stop_cmd = NULL,
    .post_start_cmd = NULL,
};

static int32_t on_start(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t on_stop(uint8_t cmd_type, iotlab_packet_t *pkt);

void cn_alim_start()
{
    // Configure and register all handlers

    static iotlab_serial_handler_t handler_start = {
        .cmd_type = OPEN_NODE_START,
        .handler = on_start,
    };
    iotlab_serial_register_handler(&handler_start);
    static iotlab_serial_handler_t handler_stop = {
        .cmd_type = OPEN_NODE_STOP,
        .handler = on_stop,
    };
    iotlab_serial_register_handler(&handler_stop);
}

void cn_alim_config(void (*pre_stop_cmd)(), void (*post_start_cmd)())
{
    cn_alim.pre_stop_cmd = pre_stop_cmd;
    cn_alim.post_start_cmd = post_start_cmd;
}

static int32_t on_start(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    if (1 != pkt->length)
        return 1;

    if (DC_CHARGE == *pkt->data) {
        fiteco_lib_gwt_opennode_power_select(FITECO_GWT_OPENNODE_POWER__MAIN);
        fiteco_lib_gwt_battery_charge_enable();
    } else if (DC_NO_CHARGE == *pkt->data) {
        fiteco_lib_gwt_opennode_power_select(FITECO_GWT_OPENNODE_POWER__MAIN);
        fiteco_lib_gwt_battery_charge_disable();
    } else if (BATTERY == *pkt->data) {
        fiteco_lib_gwt_opennode_power_select(FITECO_GWT_OPENNODE_POWER__BATTERY);
        fiteco_lib_gwt_battery_charge_disable();
    } else {
        //unexpected value
        return 1;
    }

    // Run a pre_stop command before stoping
    if (cn_alim.post_start_cmd != NULL)
        cn_alim.post_start_cmd();

    return 0;
}

static int32_t on_stop(uint8_t cmd_type, iotlab_packet_t *packet)
{
    packet_t *pkt = (packet_t *)packet;
    if (1 != pkt->length)
        return 1;

    // Run a pre_stop command before stoping
    if (cn_alim.pre_stop_cmd != NULL)
        cn_alim.pre_stop_cmd();

    fiteco_lib_gwt_opennode_power_select(FITECO_GWT_OPENNODE_POWER__OFF);

    // Charge if DC
    if (DC_CHARGE == *pkt->data)
        fiteco_lib_gwt_battery_charge_enable();
    else if (DC_NO_CHARGE == *pkt->data)
        fiteco_lib_gwt_battery_charge_disable();
    else if (BATTERY == *pkt->data)
        fiteco_lib_gwt_battery_charge_disable();
    else
        return 1;

    return 0;
}

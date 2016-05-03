/*
 * In the build directory just under the openlab directory:
 * cmake .. -DRELEASE=2 -DPLATFORM=iotlab-m3
 * cmake .. -DRELEASE=2 -DPLATFORM=iotlab-a8-m3
 * -DRELEASE=2 to avoid any log_printf
 */
#include <stdint.h>
#include <string.h>
#include "printf.h"
#include "scanf.h"

#include "platform.h"
#include "shell.h"

/* Drivers */

#include "phy_power.c.h"


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define TX_DELAY_DEFAULT    50
#define TX_POWER_DEFAULT    "0dBm"
#define TX_CHANNEL_DEFAULT  11


#define ON_ERROR(msg, label)  do { \
    ret = msg; \
    goto label; \
} while (0)

static void radio_pkt(handler_arg_t arg);


struct {
    int tx_on;
    phy_packet_t tx_pkt;

    uint16_t delay;
    char power[8];
    uint16_t channel;
    uint32_t tx_count;

    soft_timer_t timer;

} _cfg = {
    .tx_on = 0,
    .tx_pkt = {
        .data = _cfg.tx_pkt.raw_data,
        .length = 125,
        .raw_data = {0xFF},
    },
    .delay = TX_DELAY_DEFAULT,
    .power = TX_POWER_DEFAULT,
    .channel = TX_CHANNEL_DEFAULT,
    .tx_count = 0,

    .timer = {
        .handler = radio_pkt,
        .handler_arg = NULL,
        .priority = EVENT_QUEUE_APPLI,
    }
};



static void schedule_next_packet()
{
    soft_timer_start(&_cfg.timer, soft_timer_ms_to_ticks(_cfg.delay), 0);
}


/* Radio */
static void radio_rx_tx_done(phy_status_t status)
{
    if (PHY_SUCCESS != status)
        printf("WARNING: packet tx failed: %u\n", status);
    if (_cfg.tx_on) {
        schedule_next_packet();
    }
}


static void radio_pkt(handler_arg_t arg)
{
    (void) arg;
    char *ret = NULL;
    phy_idle(platform_phy);

    /*
     * config radio
     */
    uint32_t power = parse_power_rf231(_cfg.power);
    if (phy_set_power(platform_phy, power))
        ON_ERROR("err_set_power", radio_cleanup);

    if (phy_set_channel(platform_phy, _cfg.channel))
        ON_ERROR("err_set_channel", radio_cleanup);

    /*
     * Send packet
     */
    _cfg.tx_count++;
    if (phy_tx_now(platform_phy, &_cfg.tx_pkt, radio_rx_tx_done))
        ON_ERROR("err_tx", radio_cleanup);

    // SUCCESS
    return;

radio_cleanup:
    phy_reset(platform_phy);
    printf("ERROR: radio_pkt: %s\n", ret);
}


/* Control transmission */
static int cmd_tx_on(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 1;

    _cfg.tx_on = 1;
    schedule_next_packet();
    return 0;
}

static int cmd_tx_off(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 1;

    _cfg.tx_on = 0;
    soft_timer_stop(&_cfg.timer);
    return 0;
}

static int cmd_tx_get(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 1;

    if (_cfg.tx_on)
        printf("Packets tx ON\n");
    else
        printf("Packets tx OFF\n");

    return 0;
}

static int cmd_tx_count(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 1;

    printf("Tx count: %u\n", _cfg.tx_count);

    return 0;
}

static int cmd_reset_tx_count(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 1;

    _cfg.tx_count = 0;

    return 0;
}

// Delay

static int cmd_set_delay(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    uint16_t delay;
    if (1 != sscanf(argv[1], "%u", &delay))
        return 1;

    _cfg.delay = delay;
    return 0;
}

static int cmd_get_delay(int argc, char **argv)
{
    (void) argv;
    if (argc != 1)
        return 1;

    printf("%u ms\n", _cfg.delay);

    return 0;
}


// Power

static int cmd_set_power(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    // Power
    uint8_t power;
    power = parse_power_rf231(argv[1]);
    if (255 == power) {
        printf("ERROR %s not in \n%s\n", argv[1], radio_power_rf231_str);
        return 1;
    }

    strncpy(_cfg.power, argv[1], sizeof(_cfg.power));
    _cfg.power[sizeof(_cfg.power) -1] = '\0';

    return 0;
}

static int cmd_get_power(int argc, char **argv)
{
    (void) argv;
    if (argc != 1)
        return 1;

    printf("%s\n", _cfg.power);

    return 0;
}


// Channel
static int cmd_set_channel(int argc, char **argv)
{
    if (argc != 2)
        return 1;

    // Channel
    uint8_t channel;
    if (1 != sscanf(argv[1], "%u", &channel))
        return 1;
    if (11 > channel || channel > 26)
        return 1;

    _cfg.channel = channel;

    return 0;
}

static int cmd_get_channel(int argc, char **argv)
{
    (void) argv;
    if (argc != 1)
        return 1;

    printf("%u\n", _cfg.channel);

    return 0;
}


struct shell_command commands[] = {
    {"tx_on",          "Start packets tx",              cmd_tx_on},
    {"tx_off",         "Stop packets tx",               cmd_tx_off},
    {"tx_mode",        "Get packets ON/OFF",            cmd_tx_get},
    {"tx_count",       "Get number of packets sent",    cmd_tx_count},
    {"tx_count_reset", "Reset number of packets sent",  cmd_reset_tx_count},

    {"set_delay",      "[delay] Set delay between packets ms."
        " Default "TOSTRING(TX_DELAY_DEFAULT)".",       cmd_set_delay},
    {"get_delay",      "Get delay between packets ms.", cmd_get_delay},

    {"set_power",      "[power] Set tx power."
        " Default " TX_POWER_DEFAULT ".",               cmd_set_power},
    {"get_power",      "Get delay between packets ms.", cmd_get_power},

    {"set_channel",    "[channel] Set channel."
        " Default " TOSTRING(TX_CHANNEL_DEFAULT) ".",   cmd_set_channel},
    {"get_channel",    "Get radio channel.",            cmd_get_channel},

    {NULL, NULL, NULL},
};

int main(void)
{
    platform_init();
    soft_timer_init();
    event_init();

    shell_init(commands, 0);
    platform_run();
    return 1;
}


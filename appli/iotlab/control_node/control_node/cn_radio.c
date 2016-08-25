#include "platform.h"
#include "debug.h"

#include "constants.h"
#include "cn_radio.h"
#include "cn_control.h"
#include "cn_meas_pkt.h"
#include "iotlab_serial.h"
#include "iotlab_time.h"
#include "zep_sniffer_format.h"


#include "phy.h"
#include "packer.h"
#include "soft_timer.h"

#include "platform.h"
#include "debug.h"

#include "iotlab_leds.h"


static int32_t radio_off(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t radio_polling(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t radio_sniffer(uint8_t cmd_type, iotlab_packet_t *pkt);

#if 0
static int32_t radio_injection(uint8_t cmd_type, iotlab_packet_t *pkt);
static int32_t radio_jamming(uint8_t cmd_type, iotlab_packet_t *pkt);
#endif
static void proper_stop();
static int manage_channel_switch();
static void set_next_channel();
static void schedule_polling_timer();

static void poll_time(handler_arg_t arg);
static void sniff_rx(void);

enum {
    RSSI_MEASURE_SIZE = sizeof(uint8_t) + sizeof(uint32_t),
    SEC = 1000000,
};

#define MEASURES_PRIORITY 0
#define CN_RADIO_NUM_PKTS (8)

typedef enum radio_mode {
    RADIO_OFF = 0,
    RADIO_POLLING,
    RADIO_SNIFFER,
} radio_mode_t;

struct radio_config {
    radio_mode_t mode;
    uint32_t channels;
    uint32_t num_operations_per_channel;
    uint32_t measure_period;
};

static void update_config(struct radio_config *config);

static struct
{
    struct radio_config config;

    soft_timer_t timer;

    uint32_t current_channel;
    uint8_t  current_op_num_on_channel;

    iotlab_packet_t meas_pkts[CN_RADIO_NUM_PKTS];
    iotlab_packet_queue_t measures_queue;

    /* Radio RX commands */
    struct {
        iotlab_packet_t *serial_pkt;
    } rssi;
    struct {
        phy_packet_t pkt_buf[2];
        int pkt_index;
    } sniff;

#if 0
    /* Radio TX commands */
    struct {
        phy_power_t tx_power;

        phy_packet_t radio_pkt;
    } injection;

    struct {
        phy_power_t tx_power;
    } jam;
#endif
} radio;


static struct radio_config default_config = {
    .mode = RADIO_OFF,
    .channels = 0,
    .num_operations_per_channel = 0,
};

static void radio_init()
{
    iotlab_packet_init_queue(&radio.measures_queue,
            radio.meas_pkts, CN_RADIO_NUM_PKTS);

    radio.rssi.serial_pkt = NULL;
    radio.sniff.pkt_index = 0;
    phy_prepare_packet(&radio.sniff.pkt_buf[0]);
    phy_prepare_packet(&radio.sniff.pkt_buf[1]);
}

void cn_radio_start()
{
    radio_init();
    update_config(&default_config);

    // Set the handlers
    static iotlab_serial_handler_t handler_off = {
        .cmd_type = CONFIG_RADIO_STOP,
        .handler = radio_off,
    };
    iotlab_serial_register_handler(&handler_off);

    static iotlab_serial_handler_t handler_polling = {
        .cmd_type = CONFIG_RADIO_MEAS,
        .handler = radio_polling,
    };
    iotlab_serial_register_handler(&handler_polling);

    static iotlab_serial_handler_t handler_sniffer = {
        .cmd_type = CONFIG_RADIO_SNIFFER,
        .handler = radio_sniffer,
    };
    iotlab_serial_register_handler(&handler_sniffer);

#if 0
    static iotlab_serial_handler_t handler_injection = {
        .cmd_type = CONFIG_RADIO_INJECTION,
        .handler = radio_injection,
    };
    iotlab_serial_register_handler(&handler_injection);

    static iotlab_serial_handler_t handler_jamming = {
        .cmd_type = CONFIG_RADIO_NOISE,
        .handler = radio_jamming,
    };
    iotlab_serial_register_handler(&handler_jamming);
#endif
}

/*
 * Use a non-periodic timer, because for periodic timers, handlers are queued
 * before soft_timer does the 'event_post'.
 * We want only one a finite number of handler at the same time, so only
 * ask to schedule next event when one is currently handled.
 * This way there are only one waiting at the same time.
 *
 */
static void schedule_polling_timer()
{
    soft_timer_start(&radio.timer, radio.config.measure_period, 0);
}

static void _set_new_config(struct radio_config *config);
static void update_config(struct radio_config *config)
{
    // Set back default config
    memcpy(&radio.config, &default_config, sizeof(struct radio_config));
    proper_stop();

    // update in another event in case an event occured between
    if (event_post(EVENT_QUEUE_APPLI, (handler_t)_set_new_config, config))
        return;  // Could not update configuration // TODO
}

static void _set_new_config(struct radio_config *config)
{
    memcpy(&radio.config, config, sizeof(struct radio_config));

    switch (config->mode) {
    case RADIO_OFF:
        break;  // already off
    case RADIO_POLLING:
        // Select first radio channel
        set_next_channel();
        // Start Timer
        soft_timer_set_handler(&radio.timer, poll_time, NULL);
        schedule_polling_timer();
        break;
    case RADIO_SNIFFER:
        // Select first radio channel
        set_next_channel();
        // Select first packet
        radio.sniff.pkt_index = 0;

        sniff_rx();
        break;
    }
}

void flush_current_rssi_measures()
{
    cn_meas_pkt_flush(&radio.rssi.serial_pkt, RADIO_MEAS_FRAME);
}

static void proper_stop()
{
    // Stop timer
    soft_timer_stop(&radio.timer);

    // Set PHY idle
    phy_idle(platform_phy);

    /* Reset configs */
    radio.current_channel = 0;
    radio.current_op_num_on_channel = 0;

    flush_current_rssi_measures();
}

static int manage_channel_switch()
{
    if (0 == radio.config.num_operations_per_channel)
        return 1;  // channel switch disabled

    // increment operation num
    radio.current_op_num_on_channel++;

    // test if channel switch required
    if (radio.config.num_operations_per_channel == radio.current_op_num_on_channel) {
        set_next_channel();
        radio.current_op_num_on_channel = 0;
        return 0;
    }
    return 1;
}


static void set_next_channel()
{
    phy_idle(platform_phy);

    do {
        radio.current_channel++;
        if (radio.current_channel > PHY_2400_MAX_CHANNEL)
            radio.current_channel = PHY_2400_MIN_CHANNEL;
    } while ((radio.config.channels & (1 << radio.current_channel)) == 0);
    phy_set_channel(platform_phy, radio.current_channel);
}

/* ********************** OFF **************************** */
static int32_t radio_off(uint8_t cmd_type, iotlab_packet_t *packet)
{
    (void)packet;
    // Stop from radio event queue
    if (event_post(EVENT_QUEUE_APPLI, (handler_t)update_config, &default_config))
        return 1;  // Could not update configuration
    return 0;
}


/* ********************** POLLING **************************** */

static int32_t radio_polling(uint8_t cmd_type, iotlab_packet_t *packet)
{
    /*
     * Expected packet format is (length:7B):
     *      * channels                  [4B]
     *      * Measure period (ms)       [2B]
     *      * Measures per channel      [1B]
     */
    static struct radio_config config = {
        .mode = RADIO_POLLING,
    };

    packet_t *pkt = (packet_t *)packet;
    if (pkt->length != 7)
        return 1;

    size_t index = 0;

    /** GET values, system endian */
    memcpy(&config.channels, &pkt->data[index], 4);
    index += 4;
    memcpy(&config.measure_period, &pkt->data[index], sizeof(uint16_t));
    index += 2;
    memcpy(&config.num_operations_per_channel, &pkt->data[index], 1);
    index ++;

    /*
     * Check arguments validity
     */
    config.channels &= PHY_MAP_CHANNEL_2400_ALL;
    if (config.channels == 0)
        return 1;

    if (0 == config.measure_period)
        return 1;
    config.measure_period = soft_timer_ms_to_ticks(config.measure_period);
    // config.num_operations_per_channel:  0 means no switch


    // Configure from radio event queue
    if (event_post(EVENT_QUEUE_APPLI, (handler_t)update_config, &config))
        return 1;  // Could not update configuration

    return 0;
}


static void poll_time(handler_arg_t arg)
{
    int32_t ed = 0;
    struct soft_timer_timeval timestamp;
    iotlab_packet_t *packet;
    uint8_t channel = (uint8_t) radio.current_channel;

    if (radio.config.mode != RADIO_POLLING)
        return;

    schedule_polling_timer();

    phy_ed(platform_phy, &ed);
    iotlab_time_extend_relative(&timestamp, soft_timer_time());

    packet = cn_meas_pkt_lazy_alloc(&radio.measures_queue,
                                    radio.rssi.serial_pkt, &timestamp);
    if ((radio.rssi.serial_pkt = packet) == NULL)
        goto poll_time_end;  // alloc failed, drop this measure

    /* Add measure time + rssi measure*/
    struct cn_meas rssi[] = {{&channel, sizeof(uint8_t)},
                             {&ed, sizeof(uint8_t)},
                             {NULL, 0}};
    int send = cn_meas_pkt_add_measure(
            packet, &timestamp, RSSI_MEASURE_SIZE, rssi);

    if (send)
        flush_current_rssi_measures();

poll_time_end:
    /* Is it time to switch channel ? */
    manage_channel_switch();
}




/* ********************** SNIFFER **************************** */

static void sniff_handle_rx(phy_status_t status);
static void sniff_handle_rx_appli_queue(handler_arg_t arg);
#if 0
static void sniff_switch_channel(handler_arg_t arg);
#endif

static int32_t radio_sniffer(uint8_t cmd_type, iotlab_packet_t *packet)
{
    /*
     * Expected packet format is (length:6B):
     *      * channels                  [4B]
     *      * time per channel (ms)     [2B]
     */
    static struct radio_config config = {
        .mode = RADIO_SNIFFER,
    };

    packet_t *pkt = (packet_t *)packet;

    if (pkt->length != 6)
        return 1;

    size_t index = 0;
    uint16_t time_per_channel;

    /** GET values, system endian */
    memcpy(&config.channels, &pkt->data[index], 4);
    index += 4;
    memcpy(&time_per_channel, &pkt->data[index], sizeof(uint16_t));
    index += 2;

    /*
     * Check arguments validity
     */
    config.channels &= PHY_MAP_CHANNEL_2400_ALL;
    if (config.channels == 0)
        return 1;

    // Multiple channels given, error
    if (config.channels & (config.channels -1))
        return 1;

    if (0 != time_per_channel)
        return 1;

    // Configure from radio event queue
    if (event_post(EVENT_QUEUE_APPLI, (handler_t)update_config, &config))
        return 1; // Could not update configuration

    // OK
    return 0;
}

static void sniff_rx()
{
    phy_packet_t *tx_pkt = &radio.sniff.pkt_buf[radio.sniff.pkt_index];
    phy_rx_now(platform_phy, tx_pkt, sniff_handle_rx);
    // TODO Handle errors on phy_rx_now
}

static void sniff_handle_rx(phy_status_t status)
{
    event_post(EVENT_QUEUE_APPLI, sniff_handle_rx_appli_queue, (handler_arg_t)status);
}

static void zep_to_packet(packet_t *pkt, phy_packet_t *rx_pkt, uint8_t channel)
{
    // TODO register time handler for phy_rx to do this in phy layer
    struct soft_timer_timeval timestamp;
    iotlab_time_extend_relative(&timestamp, rx_pkt->timestamp);
    rx_pkt->timestamp_alt.msb = timestamp.tv_sec;
    rx_pkt->timestamp_alt.lsb = timestamp.tv_usec;

    // Save packet as zep
    pkt->length = to_zep(pkt->data, rx_pkt, channel, cn_control_node_id());
}

static void sniff_handle_rx_appli_queue(handler_arg_t arg)
{
    if (radio.config.mode != RADIO_SNIFFER)
        return;

    phy_status_t status = (phy_status_t)arg;

    // Get current packet and switch packets
    phy_packet_t *rx_pkt = &radio.sniff.pkt_buf[radio.sniff.pkt_index];
    radio.sniff.pkt_index = (radio.sniff.pkt_index + 1) % 2;
    sniff_rx();

    if (status != PHY_SUCCESS)
        return;

    iotlab_packet_t *packet = iotlab_serial_packet_alloc(&radio.measures_queue);
    if (packet == NULL)
        return;

    zep_to_packet((packet_t *)packet, rx_pkt, radio.current_channel);

    if (iotlab_serial_send_frame(RADIO_SNIFFER_FRAME, packet))
        iotlab_packet_call_free(packet);
}


#if 0
static void sniff_switch_channel(handler_arg_t arg)
{
    set_next_channel();
    sniff_rx();
}
#endif

#if 0

/* ********************** INJECTION **************************** */
static void injection_time(handler_arg_t arg);
static void injection_tx_done(phy_status_t status);

static int32_t radio_injection(uint8_t cmd_type, iotlab_packet_t *pkt)
{
    // Stop all
    proper_stop();

    /*
     * Expected packet format is (length:13B):
     *      * channels bitmap           [4B]
     *      * TX period (1/200s)        [2B]
     *      * num packets per channel   [2B]
     *      * TX power                  [4B]
     *      * packet size               [1B]
     */

    if (pkt->length != 13)
    {
        log_warning("Bad Packet length: %u", pkt->length);
        pkt->length = 0;
        return 1;
    }

    const uint8_t *data = pkt->data;
    memcpy(&radio.channels, data, 4);
    data += 4;
    uint16_t tx_period;
    memcpy(&tx_period, data, 2);
    data += 2;
    memcpy(&radio.injection.num_pkts_per_channel, data, 2);
    data += 2;
    float tx_power;
    memcpy(&tx_power, data, 4);
    data += 4;
    uint32_t pkt_size = *data++;

    if ((radio.channels & PHY_MAP_CHANNEL_2400_ALL) == 0)
    {
        log_warning("No channel selected %u", radio.channels);
        pkt->length = 0;
        return 1;
    }
    radio.channels &= PHY_MAP_CHANNEL_2400_ALL;

    if (pkt_size > 125)
    {
        log_warning("Bad injection length value: %u", pkt_size);
        pkt->length = 0;
        return 1;
    }

    if (tx_period == 0)
    {
        log_warning("Invalid injection TX period: %u", tx_period);
        pkt->length = 0;
        return 1;
    }

    log_info(
            "Radio Injection on channels %08x, period %u, %u pkt/ch, %fdBm, %ubytes",
            radio.channels, tx_period, radio.injection.num_pkts_per_channel,
            tx_power, pkt_size);

    // Prepare packet
    phy_prepare_packet(&radio.injection.pkt);
    radio.injection.pkt.length = pkt_size;
    int i;
    for (i = 0; i < pkt_size; i++)
    {
        // Set payload as ascii characters
        radio.injection.pkt.data[i] = (0x20 + i) % 95;
    }

    // Select first channel
    for (radio.current_channel = 0;
            (radio.channels & (1 << radio.current_channel)) == 0;
            radio.current_channel++)
    {
    }

    // Clear TX count
    radio.injection.current_pkts_on_channel = 0;

    // Wake PHY and configure
    phy_set_channel(platform_phy, radio.current_channel);
    phy_set_power(platform_phy, phy_convert_power(tx_power));

    // Start sending timer
    soft_timer_set_handler(&radio.period_tim, injection_time, NULL);
    soft_timer_start(&radio.period_tim, soft_timer_ms_to_ticks(5 * tx_period),
            1);

    // OK
    pkt->length = 0;
    return 0;
}

static void injection_time(handler_arg_t arg)
{
    if (phy_tx_now(platform_phy, &radio.injection.pkt, injection_tx_done) != PHY_SUCCESS)
    {
        log_error("Failed to send injection packet");
    }
}
static void injection_tx_done(phy_status_t status)
{
    // Increment packet count and check
    radio.injection.current_pkts_on_channel++;

    if (radio.injection.current_pkts_on_channel
            >= radio.injection.num_pkts_per_channel)
    {
        radio.injection.current_pkts_on_channel = 0;

        // Select next channel
        do
        {
            radio.current_channel++;
            if (radio.current_channel > PHY_2400_MAX_CHANNEL)
            {
                radio.current_channel = PHY_2400_MIN_CHANNEL;
            }
        } while ((radio.channels & (1 << radio.current_channel)) == 0);

        phy_set_channel(platform_phy, radio.current_channel);

        log_info("Injecting %u packets on channel %u",
                radio.injection.num_pkts_per_channel, radio.current_channel);
    }
}

/* ********************** JAMMING **************************** */
static void jam_change_channel_time(handler_arg_t arg);
static int32_t radio_jamming(uint8_t cmd_type, iotlab_packet_t *pkt)
{
    // Stop all
    proper_stop();

    /*
     * Expected packet format is (length:10B):
     *      * channels bitmap           [4B]
     *      * channel period (1/200s)   [2B]
     *      * TX power                  [4B]
     */

    if (pkt->length != 10)
    {
        log_warning("Bad Packet length: %u", pkt->length);
        pkt->length = 0;
        return 1;
    }

    const uint8_t *data = pkt->data;
    memcpy(&radio.channels, data, 4);
    data += 4;
    uint16_t channel_period;
    memcpy(&channel_period, data, 2);
    data += 2;
    float tx_power;
    memcpy(&tx_power, data, 4);
    data += 4;

    if ((radio.channels & PHY_MAP_CHANNEL_2400_ALL) == 0)
    {
        log_warning("No channel selected %u", radio.channels);
        pkt->length = 0;
        return 1;
    }
    radio.channels &= PHY_MAP_CHANNEL_2400_ALL;

    log_info("Radio Jamming on channels %08x, change period %u, power %f",
            radio.channels, channel_period, tx_power);

    // Select first channel
    for (radio.current_channel = 0;
            (radio.channels & (1 << radio.current_channel)) == 0;
            radio.current_channel++)
    {
    }

    // Store power
    radio.jam.tx_power = phy_convert_power(tx_power);

    // Start jamming
    phy_jam(platform_phy, radio.current_channel, radio.jam.tx_power);

    if (channel_period)
    {
        soft_timer_set_handler(&radio.period_tim, jam_change_channel_time,
                NULL);
        soft_timer_start(&radio.period_tim,
                soft_timer_ms_to_ticks(5 * channel_period), 1);
    }

    return 0;
}

static void jam_change_channel_time(handler_arg_t arg)
{
    // Increment channel
    set_next_channel();

    // Re-Jam
    phy_idle(platform_phy);
    phy_jam(platform_phy, radio.current_channel, radio.jam.tx_power);

    log_info("Jamming on channel %u", radio.current_channel);
}

#endif

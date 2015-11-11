#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "soft_timer.h"
#include "radio_network.h"
#include "clock_convergence.h"
#include "mac_csma.h"
#include "phy_power.c.h"

#define RADIO_NETWORK_SEND_RETRY 5

uint16_t neighbours[MAX_NUM_NEIGHBOURS] = {0};
uint32_t num_neighbours = 0;
static void send(uint16_t addr, const void *packet, size_t length, int max_try);

static struct {
    uint32_t channel;
} rn_config;


#define ADDR_BROADCAST 0xFFFF
#define RADIO_POWER PHY_POWER_3dBm

void network_init(uint32_t channel)
{
    network_reset();

    rn_config.channel = channel;
    mac_csma_init(rn_config.channel, RADIO_POWER);
}

void network_reset()
{
    memset(neighbours, 0, sizeof(neighbours));
    num_neighbours = 0;
    mac_csma_init(rn_config.channel, RADIO_POWER);
}


void network_send(const void *packet, size_t length)
{
    send(ADDR_BROADCAST, packet, length, RADIO_NETWORK_SEND_RETRY);
}

void network_send_no_retry(const void *packet, size_t length)
{
    send(ADDR_BROADCAST, packet, length, 1);
}


struct msg_send
{
    int try;
    int max_try;
    uint16_t addr;
    uint8_t pkt[MAC_PKT_LEN];
    size_t length;
};


static void do_send(handler_arg_t arg)
{
    struct msg_send *send_cfg = (struct msg_send *)arg;
    int ret;

    ret = mac_csma_data_send(send_cfg->addr, send_cfg->pkt, send_cfg->length);
    if (ret != 0) {
        DEBUG("Sending to %04x try %u Success\n", send_cfg->addr, send_cfg->try);
    } else if (++send_cfg->try < send_cfg->max_try) {
        ERROR("Sending to %04x try %u Failed. Retrying\n",
                send_cfg->addr, send_cfg->try);
        event_post(EVENT_QUEUE_APPLI, do_send, arg);
    } else {
        ERROR("Sending to %04x try %u Failed.\n",
                send_cfg->addr, send_cfg->try);
    }
}

static void send(uint16_t addr, const void *packet, size_t length, int max_try)
{
    static struct msg_send send_cfg;
    send_cfg.try = 0;
    send_cfg.addr = addr;
    send_cfg.length = length;
    send_cfg.max_try = max_try;
    memcpy(&send_cfg.pkt, packet, length);

    event_post(EVENT_QUEUE_APPLI, do_send, &send_cfg);
}

/*
 * Generic neighbours functions
 */


int network_neighbours_load(int argc, char **argv)
{
    if (argc <= 2)
        return 0;
    if (strtol(argv[1], NULL, 16) != iotlab_uid())
        return 0;  // It's for another node
    int num_neigh = argc -2;
    if (num_neigh > MAX_NUM_NEIGHBOURS) {
        ERROR("Too many neighbours: %d > %d", num_neigh, MAX_NUM_NEIGHBOURS);
        return 1;
    }

    int i;
    for (i = 0; i < num_neigh; i++) {
        neighbours[i] = strtol(argv[i + 2], NULL, 16);
        num_neighbours++;
    }

    // Should have been blanked but anyway
    if (num_neigh < MAX_NUM_NEIGHBOURS)
        neighbours[num_neigh] = 0;

    INFO("Loaded %d neighbours\n", num_neigh);
    return 0;
}

int network_neighbours_print(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int i;
    MSG("Neighbours;%u", num_neighbours);
    for (i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
        if (neighbours[i])
            printf(";%04x", neighbours[i]);
        else
            break; // no more neighbours
    }
    printf("\n");
    return 0;
}

static int network_neighbour_id(uint16_t src_addr)
{
    int i;
    for (i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
        uint16_t neighbour_addr = neighbours[i];
        if (neighbour_addr == src_addr)
            return i;
        else if (neighbour_addr == 0)
            return -1;
    }
    return -1;
}

int network_set_tx_power(int argc, char **argv)
{
    uint32_t power = 0;

    if (argc != 2)
        return 1;

    if (0 == strcmp("high", argv[1])) {
        power = RADIO_POWER;
    } else if (0 == strcmp("low", argv[1])) {
        power = PHY_POWER_m17dBm;
    } else if (255 != (power = parse_power_rf231(argv[1]))) {
        DEBUG("Power == %s\n", argv[1]);
        ;  // Power read from value
    } else {
        ERROR("%s: Invalid power '%s', not in %s or '%s'\n",
                argv[0], argv[1], "['low', 'high']",
                radio_power_rf231_str);
        return 1;
    }

    mac_csma_init(rn_config.channel, power);
    return 0;
}

/*
 * Neighbours discovery
 */
int network_neighbours_discover(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint8_t pkt = PKT_GRAPH;
    send(ADDR_BROADCAST, &pkt, 1, RADIO_NETWORK_SEND_RETRY);
    return 0;
}

static void network_neighbours_add(uint16_t src_addr, int8_t rssi)
{
    if (rssi < MIN_RSSI) {
        DEBUG("DROP neighbour %04x. rssi: %d\n", src_addr, rssi);
        return;
    } else {
        DEBUG("ADD  neighbour %04x. rssi: %d\n", src_addr, rssi);
    }

    int i;
    for (i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
        uint16_t neighbour_addr = neighbours[i];
        if (neighbour_addr == 0) {
            neighbours[i] = src_addr;
            num_neighbours++;
            break;
        } else if (neighbour_addr == src_addr) {
            break;
        } else {
            continue;
        }

    }
}


/*
 * Validate who are your neighbours
 */
struct neighbours_pkt {
    uint8_t type;
    uint16_t neighbours[MAX_NUM_NEIGHBOURS];
};

int network_neighbours_acknowledge(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    struct neighbours_pkt pkt;

    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_NEIGH;
    memcpy(&pkt.neighbours, neighbours, sizeof(neighbours));
    send(ADDR_BROADCAST, &pkt, sizeof(pkt), RADIO_NETWORK_SEND_RETRY);
    return 0;
}

static void network_neighbours_validate(uint16_t src_addr,
        const uint8_t *data, size_t length)
{
    if (sizeof(struct neighbours_pkt) != length)
        ERROR("Invalid neighbours pkt len\n");
    struct neighbours_pkt pkt;
    memcpy(&pkt, data, sizeof(struct neighbours_pkt));

    const uint16_t my_id = iotlab_uid();

    int i;
    // Add 'src_addr' has a neighbour if I'm his neighbourg
    for (i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
        const uint16_t cur_id = pkt.neighbours[i];
        if (cur_id == my_id)
            network_neighbours_add(src_addr, INT8_MAX);
        else if (cur_id == 0)
            break; // no more neighbours in pkt
    }
}


/*
 * Packet reception
 */
void mac_csma_data_received(uint16_t src_addr,
        const uint8_t *data, uint8_t length, int8_t rssi, uint8_t lqi)
{
    uint8_t pkt_type = data[0];
    DEBUG("pkt received from %04x\n", src_addr);

    switch (pkt_type) {
    case (PKT_GRAPH):
        network_neighbours_add(src_addr, rssi);
        return;
    case (PKT_NEIGH):
        network_neighbours_validate(src_addr, data, length);
        return;
    }

    // Filter neighbours packets
    int index = network_neighbour_id(src_addr);
    if (index == -1) {
        DEBUG("Packet from %04x: not neighbour\n", src_addr);
        return;
    } else {
        DEBUG("Packet from %04x\n", src_addr);
    }

    // Application packets
    switch (pkt_type) {
    case (PKT_VALUES):
        computing_handle_values(src_addr, data, length, index);
        break;
    case (PKT_CLOCK):
        clock_convergence_handle_time(src_addr, data, length);
        break;
    default:
        INFO("Unknown pkt type %01x from %04x\n", pkt_type, src_addr);
        break;
    }
}

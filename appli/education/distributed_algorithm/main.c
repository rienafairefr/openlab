#include <platform.h>
#include <stdint.h>
#include <stdlib.h>
#include <printf.h>
#include <string.h>

#include "phy.h"
#include "soft_timer.h"
#include "event.h"
#include "computing.h"
#include "radio_network.h"
#include "iotlab_uid.h"
#include "config.h"

#include "shell.h"

static int print_values(int argc, char **argv)
{
    if (argc != 1)
        return 1;
    (void)argv;
    int i;
    MSG("Values;%u", compute_number);
    for (i = 0; i < NUM_VALUES; i++)
        printf(";%f", my_values.v[i]);
    printf("\n");
    return 0;
}

static int print_final_value(int argc, char **argv)
{
    if (argc != 1)
        return 1;
    (void)argv;
    uint32_t final_value = compute_final_value();
    MSG("FinalValue;%u;%u\n", compute_number, final_value);
    return 0;
}


static int compute_all_values(int argc, char **argv)
{
    if (argc != 1)
        return 1;
    (void)argv;

    int i;
    compute_number++;
    num_neighbours = 0;
    for (i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
        if (neighbours_values[i].valid)
            num_neighbours++;
    }
    for (i = 0; i < NUM_VALUES; i++) {
        my_values.v[i] = compute_value_from_neighbours(
                my_values.v[i], num_neighbours, neighbours_values, i);
    }

    // Reset values for next run
    memset(neighbours_values, 0, sizeof(neighbours_values));
    return 0;
}


int init_values(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    memset(neighbours_values, 0, sizeof(neighbours_values));
    compute_number = 0;

    network_reset();

    memset(&my_values, 0, sizeof(my_values));
    int i;
    for (i = 0; i < NUM_VALUES; i++)
        my_values.v[i] = init_value();
    return 0;
}

static int send_values(int argc, char **argv)
{
    int compute_on_rx = 0;
    switch (argc) {
        case 1:
            break;
        case 2:
            if (0 == strcmp("compute", argv[1])) {
                compute_on_rx = 1;
                break;
            }
            // ERROR
        default:
            ERROR("%s: invalid arguments\n", argv[0]);
            return 1;
    }
    network_send_values(compute_on_rx, &my_values);
    return 0;
}

struct shell_command commands[] = {

    {"tx_power", "[low|high] Set tx power", network_set_tx_power},
    {"reset", "Reset neighbours and init sensor value", init_values},

    {"graph-create", "create connection graph for this node", network_neighbours_discover},
    {"graph-validate", "Validate Graph with neighbours", network_neighbours_acknowledge},
    {"graph-print", "print neighbours table", network_neighbours_print},

    {"send_values", "[|compute] send values to neighbours. May ask to also compute", send_values},
    {"compute_values", "compute values received from all neighbours", compute_all_values},

    {"print-values", "print current node values", print_values},
    {"print-final-value", "print a calculated final int value", print_final_value},
    {NULL, NULL, NULL},
};

int main()
{
    platform_init();
    event_init();
    soft_timer_init();

    // Radio communication init
    network_init(CHANNEL, GRAPH_RADIO_POWER, RADIO_POWER);
    // init values at start
    init_values(0, NULL);

    shell_init(commands, 0);

    platform_run();
    return 0;
}

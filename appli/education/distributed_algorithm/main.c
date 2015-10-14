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
#include "poisson_clock.h"
#include "clock_convergence.h"
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
    int all = argc < 2;  // default
    if (all || 0 == strcmp("network", argv[1]))
        network_reset();

    if (all || 0 == strcmp("values", argv[1])) {
        memset(neighbours_values, 0, sizeof(neighbours_values));
        compute_number = 0;
        memset(&my_values, 0, sizeof(my_values));
        int i;
        for (i = 0; i < NUM_VALUES; i++)
            my_values.v[i] = init_value();
    }
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
    computing_send_values(compute_on_rx);
    return 0;
}

/* Print poisson delay values */
int poisson_delay(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    double lambda = atof(argv[1]);
    unsigned int delay = poisson_step_ticks(lambda);

    float delay_s = (float)delay / (float)SOFT_TIMER_FREQUENCY;
    MSG("PoissonDelay;%f\n", delay_s);
    return 0;
}



struct shell_command commands[] = {

    {"tx_power", "[low|high|<dBm_power>] Set tx power", network_set_tx_power},
    {"reset", "[|network|values] Reset neighbours and values", init_values},

    {"graph-create", "create connection graph for this node", network_neighbours_discover},
    {"graph-validate", "Validate Graph with neighbours", network_neighbours_acknowledge},
    {"graph-print", "print neighbours table", network_neighbours_print},
    {"neighbours", "[node_id] [neigh] [neigh] ... Use given neighbours table", network_neighbours_load},

    {"send_values", "[|compute] send values to neighbours. May ask to also compute", send_values},
    {"compute_values", "compute values received from all neighbours", compute_all_values},

    {"print-values", "print current node values", print_values},
    {"print-final-value", "print a calculated final int value", print_final_value},

    {"poisson-delay", "[lambda] Get poisson clock delay with parameter lambda", poisson_delay},

    {"clock-convergence-start", "[lambda] Start clock convergence algorithm with poisson clock parameter lambda", clock_convergence_start},
    {"clock-convergence-stop", "Stop clock convergence algorithm", clock_convergence_stop},
    {NULL, NULL, NULL},
};

int main()
{
    platform_init();
    event_init();
    soft_timer_init();

    // Radio communication init
    network_init(CHANNEL);
    // init values at start
    init_values(0, NULL);
    clock_convergence_init(TIME_SCALE, TIME_SCALE_RANDOM, TIME_OFFSET_RANDOM);

    shell_init(commands, 0);

    platform_run();
    return 0;
}

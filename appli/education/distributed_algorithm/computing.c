#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "random.h"
#include "radio_network.h"
#include "computing.h"

double init_value()
{
    double value;
    value = (double)random_rand32();
    // Value between 0 and 1
    value /= (double)RAND_MAX;
    return value;
}

// Syncronous mode
double compute_value_from_neighbours(double my_value, uint32_t my_degree,
        struct received_values *neighbours_vals, uint8_t value_num)
{

    double new_value = my_value;
    int j;

    for (j = 0; j < MAX_NUM_NEIGHBOURS; j++) {
        if (!neighbours_vals[j].valid)
            continue;
        uint32_t neighbour_degree = neighbours_vals[j].num_neighbours;
        double neighbour_value = neighbours_vals[j].values.v[value_num];

        // Add contribution for this neighbour with
        // neighbour_degree and neighbour_value
        // new_value += ...;
        (void)neighbour_degree;
        new_value = fmax(new_value, neighbour_value);
    }

    return new_value;
}

// when running in gossip mode
double compute_value_from_gossip(double my_value, double neigh_value)
{
    return fmax(my_value, neigh_value);
}

// When needed to calculate one int result in gossip mode
uint32_t compute_final_value()
{
    double result = 0.0;
    int i;
    for (i = 0; i < NUM_VALUES; i++) {
        // Replace this function to calculate expected number of nodes
        // 'ln' function is available using 'log()' C function
        result += my_values.v[i];
    }
    return (uint32_t) result;  // floor(result) in int
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

uint32_t compute_number = 0;
struct values my_values = {.v={NAN}};
struct received_values neighbours_values[MAX_NUM_NEIGHBOURS] = {{0}};

/*
 * General functions managing multiples values
 */
void compute_all_values_from_gossip(struct received_values *neigh_values)
{
    int i;
    compute_number++;
    for (i = 0; i < NUM_VALUES; i++) {
        my_values.v[i] = compute_value_from_gossip(
                my_values.v[i], neigh_values->values.v[i]);
    }
}


/* --------------------------------------------------------------------------
 * Network management
 * -------------------------------------------------------------------------- */

/*
 * Values management
 */
struct values_pkt {
    uint8_t type;
    uint8_t should_compute;
    uint32_t num_neighbours;
    struct values values;
};

void computing_send_values(uint8_t should_compute)
{
    struct values_pkt pkt;
    memset(&pkt, 0, sizeof(pkt));

    // Header
    pkt.type           = PKT_VALUES;
    pkt.should_compute = should_compute;
    pkt.num_neighbours = num_neighbours;
    // Values
    memcpy(&pkt.values, &my_values, sizeof(struct values));

    network_send(&pkt, sizeof(struct values_pkt));
}


void computing_handle_values(uint16_t src_addr,
        const uint8_t *data, size_t length, int neighbour_index)
{
    if (sizeof(struct values_pkt) != length)
        ERROR("Invalid Values pkt len\n");

    struct values_pkt pkt;
    memcpy(&pkt, data, sizeof(struct values_pkt));

    struct received_values *neigh_values = &neighbours_values[neighbour_index];
    neigh_values->valid = 1;
    neigh_values->num_neighbours = pkt.num_neighbours;
    memcpy(&neigh_values->values, &pkt.values, sizeof(struct values));

    // Gossip mode, compute after each measures received
    if (pkt.should_compute)
        compute_all_values_from_gossip(neigh_values);
}

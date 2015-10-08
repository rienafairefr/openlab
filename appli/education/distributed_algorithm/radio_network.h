#ifndef RADIO_NETWORK_H
#define RADIO_NETWORK_H
#include "config.h"
#include "computing.h"

extern uint32_t num_neighbours;
extern uint16_t neighbours[MAX_NUM_NEIGHBOURS];


// Init and resets the network layer, forgets current neighbours
void network_init(uint32_t channel);

// Resets current neighbours
void network_reset(void);

int network_set_tx_power(int argc, char **argv);


// Broadcast messages at low power to make you known to your neighbours
int network_neighbours_discover(int argc, char **argv);
// Send acknowledge messages to your neighbours nodes
// to improve the neighbours connection symmetry
int network_neighbours_acknowledge(int argc, char **argv);

// Print the network neighbours table
int network_neighbours_print(int argc, char **argv);

// Load the network neighbours table
int network_neighbours_load(int argc, char **argv);

// Send my values to the neighbours
void network_send_values(uint8_t should_compute, struct values *values);

#endif//RADIO_NETWORK_H

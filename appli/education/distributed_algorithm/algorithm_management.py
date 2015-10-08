# -*- coding:utf-8 -*-
import time
import random


def broadcast_slow(aggregator, message, delay=0.05):
    """ Send message to all nodes with 'delay' between sends """
    for node in aggregator.iterkeys():
        aggregator._send(node, message + '\n')
        time.sleep(delay)


def random_gossip_send(aggregator, message, delay=0.05):
    """ Send message to a random node """
    node = random.choice(aggregator.keys())
    aggregator._send(node, message + '\n')
    time.sleep(delay)


def create_graph(aggregator, tx_power='low', **_):
    """ Create network graph

    Reset configuration
    * On 'low' tx power, broadcast discovery messages
    * On 'high' tx power ACK neighbours
    * Then print neighbours table """
    broadcast_slow(aggregator, 'reset network', 0)

    # create network
    broadcast_slow(aggregator, 'tx_power %s' % tx_power, 0)
    time.sleep(0.5)
    broadcast_slow(aggregator, 'graph-create')  # create connection graph

    # validate graph
    broadcast_slow(aggregator, 'tx_power high', 0)
    time.sleep(0.5)
    broadcast_slow(aggregator, 'graph-validate')  # validate graph with neighbours

    # Print neighbours graph
    print_graph(aggregator)


def load_graph(aggregator, neighbours=None, **_):
    """ Load neighbours graph on nodes """
    neigh_fmt = 'neighbours {node_id} {neighs}'
    broadcast_slow(aggregator, 'reset network', 0)

    for node_id, neighs_list in sorted(neighbours.items()):
        # no neighbours
        if not neighs_list:
            continue
        cmd = neigh_fmt.format(node_id=node_id, neighs=' '.join(neighs_list))
        broadcast_slow(aggregator, cmd, 0)
        time.sleep(0.5)


def print_graph(aggregator, **_):
    """ Print neighbours graph """
    broadcast_slow(aggregator, 'graph-print')


def syncronous_mode(aggregator, num_loop=0, **_):
    """ Run messages sending and all in syncronous mode """
    broadcast_slow(aggregator, 'reset values', 0)
    broadcast_slow(aggregator, 'print-values', 0)

    for _ in range(0, num_loop):
        broadcast_slow(aggregator, 'send_values')
        broadcast_slow(aggregator, 'compute_values', 0)
        broadcast_slow(aggregator, 'print-values', 0)

def gossip_mode(aggregator, num_loop=0, **_):
    """ Run messages sending and all in syncronous mode """
    broadcast_slow(aggregator, 'reset values', 0)
    broadcast_slow(aggregator, 'print-values', 0)

    for _ in range(0, num_loop):
        random_gossip_send(aggregator, 'send_values compute')
        broadcast_slow(aggregator, 'print-values', 0)

def find_num_node_gossip_mode(aggregator, num_loop=0, **_):
    """ Find the number of nodes after having run in gossip """
    gossip_mode(aggregator, num_loop)
    broadcast_slow(aggregator, 'print-final-values', 0)

# -*- coding:utf-8 -*-
import time
import random

import functools

DELAY = 0.05


def broadcast_slow(aggregator, message, delay=DELAY):
    """ Send message to all nodes with 'delay' between sends """
    for node in aggregator.keys():
        aggregator._send(node, message + '\n')
        time.sleep(delay)
    time.sleep(DELAY)


def random_gossip_send(aggregator, message, delay=DELAY):
    """ Send message to a random node """
    node = random.choice(aggregator.keys())
    aggregator._send(node, message + '\n')
    time.sleep(delay)


def with_neighbours_graph(func):
    """ Run experiment after setting neighbours graph """
    @functools.wraps(func)
    def _wrapped_f(aggregator, neighbours=None, **kwargs):
        """ Wrapped function, add new named parameter 'neighours' """
        load_graph(aggregator, neighbours=neighbours)
        broadcast_slow(aggregator, 'reset values', 0)
        time.sleep(1)
        try:
            return func(aggregator, **kwargs)
        except KeyboardInterrupt:
            pass
    return _wrapped_f


def create_graph(aggregator, tx_power='-17dBm', **_):
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
    broadcast_slow(aggregator, 'graph-validate')  # validate neighbours

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


def print_poisson(aggregator, lambda_t=5, num_loop=300, **_):
    """ Print neighbours graph """
    for _ in range(0, num_loop):
        broadcast_slow(aggregator, 'poisson-delay %f' % lambda_t, 0)
        time.sleep(0.5)


@with_neighbours_graph
def synchronous(aggregator, num_loop=0, **_):
    """ Run messages sending and all in synchronous mode """
    broadcast_slow(aggregator, 'print-values', 0)

    for _ in range(0, num_loop):
        broadcast_slow(aggregator, 'send_values')
        broadcast_slow(aggregator, 'compute_values', 0)
        broadcast_slow(aggregator, 'print-values', 0)


@with_neighbours_graph
def gossip(aggregator, num_loop=0, **_):
    """ Run messages sending and all in gossip mode """
    broadcast_slow(aggregator, 'print-values', 0)

    for _ in range(0, num_loop):
        random_gossip_send(aggregator, 'send_values compute')
        broadcast_slow(aggregator, 'print-values', 0)


@with_neighbours_graph
def num_nodes_gossip(aggregator, num_loop=0, **_):
    """ Find the number of nodes after having run in gossip """
    gossip(aggregator, num_loop=num_loop, **_)
    broadcast_slow(aggregator, 'print-final-values', 0)


@with_neighbours_graph
def clock_convergence(aggregator, lambda_t=0.02, duration=60, **_):
    """ Run a clock_convergence algorithm """
    broadcast_slow(aggregator, 'clock-convergence-start %f' % lambda_t, 0)
    try:
        time.sleep(duration)
    except KeyboardInterrupt as err:
        print err
    finally:
        broadcast_slow(aggregator, 'clock-convergence-stop', 0)
        time.sleep(0.5)

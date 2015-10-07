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


def init_network(aggregator):
    """ Init the values, create the network and print the original values """
    broadcast_slow(aggregator, 'reset', 0)   # init sensors values and reset graphs
    broadcast_slow(aggregator, 'print-values', 0) # get values

    # create network
    broadcast_slow(aggregator, 'tx_power low', 0)
    time.sleep(0.5)
    broadcast_slow(aggregator, 'graph-create')  # create connection graph

    # validate graph
    broadcast_slow(aggregator, 'tx_power high', 0)
    time.sleep(0.5)
    broadcast_slow(aggregator, 'graph-validate')  # validate graph with neighbours

    # print graph
    broadcast_slow(aggregator, 'graph-print')


def syncronous_mode(aggregator, loop_number):
    """ Run messages sending and all in syncronous mode """
    init_network(aggregator)

    for _ in range(0, loop_number):
        broadcast_slow(aggregator, 'send_values')
        broadcast_slow(aggregator, 'compute_values', 0)
        broadcast_slow(aggregator, 'print-values', 0)  # get values

def gossip_mode(aggregator, loop_number):
    """ Run messages sending and all in syncronous mode """
    init_network(aggregator)

    for _ in range(0, loop_number):
        random_gossip_send(aggregator, 'send_values compute')
        broadcast_slow(aggregator, 'print-values', 0)  # get values

def find_num_node_gossip_mode(aggregator, loop_number):
    """ Find the number of nodes after having run in gossip """
    gossip_mode(aggregator, loop_number)
    broadcast_slow(aggregator, 'print-final-values', 0)

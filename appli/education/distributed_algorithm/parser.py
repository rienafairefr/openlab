# -*- coding:utf-8 -*-
""" Common Parser functions """


from iotlabcli.parser import common as common_parser


def base_parser():
    """ Basic parser for all scripts """
    import argparse
    parser = argparse.ArgumentParser()
    common_parser.add_auth_arguments(parser)

    nodes_group = parser.add_argument_group(
        description="By default, select currently running experiment nodes",
        title="Nodes selection")

    nodes_group.add_argument('-i', '--id', dest='experiment_id', type=int,
                             help='experiment id submission')
    nodes_group.add_argument('-l', '--list', dest='nodes_list',
                             type=common_parser.nodes_list_from_str,
                             help='nodes list, may be given multiple times')

    output = parser.add_argument_group(title="Output selection")
    output.add_argument('-o', '--outdir', required=True,
                        dest='outdir', help='Output directory')
    return parser


def algorithm_parser():
    """ Algorithm parser """
    algo_parser = base_parser()
    algo_opts = algo_parser.add_argument_group(title="Algorithm options")

    neighbours_graph(algo_opts)
    num_loop(algo_opts)

    return algo_parser


def poisson_parser():
    """ Algorithm poisson parser """
    algo_parser = base_parser()
    algo_opts = algo_parser.add_argument_group(title="Algorithm options")

    neighbours_graph(algo_opts)
    lambda_t(algo_opts)
    duration(algo_opts)

    return algo_parser


def num_loop(parser, required=True):
    """ Add num_loop option """
    parser.add_argument('-n', '--num-loop', type=int,
                        required=required,
                        dest='num_loop', help='number_of_loops_to_run')


def _neighbours(file_path):
    """" Load neighbours graph file """
    neighbours = {}
    with open(file_path) as neigh_file:
        for line in neigh_file:
            node, neighs = line.strip().split(':')
            neighbours[node] = neighs.split(';')
    return neighbours


def neighbours_graph(parser, required=True):
    """ Add neighbours_graph option """
    parser.add_argument('-g', '--neighbours-graph', type=_neighbours,
                        required=required,
                        dest='neighbours', help='Neighbours csv')


def lambda_t(parser, required=True):
    """ Add lambda option """
    parser.add_argument('--lambda', dest='lambda_t', default=5, type=float,
                        required=required,
                        help='Poisson clock lambda parameter')

def duration(parser, required=True):
    """ Add duration option """
    parser.add_argument('-d', '--duration', dest='duration', type=float,
                        required=required,
                        help='Poisson experiment duration (s)')


TX_POWERS = [
    '-17dBm', '-12dBm', '-9dBm', '-7dBm',
    '-5dBm', '-4dBm', '-3dBm', '-2dBm',
    '-1dBm', '0dBm', '0.7dBm', '1.3dBm',
    '1.8dBm', '2.3dBm', '2.8dBm', '3dBm',
]


def txpower(parser):
    """ Add tx-power option """
    parser.add_argument('-t', '--txpower', dest='tx_power', choices=TX_POWERS,
                        default='-17dBm', help='Graph transmission power')

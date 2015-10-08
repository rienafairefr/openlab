#! /usr/bin/env python
# -*- coding:utf-8 -*-

""" Get the iotlab-uip for all experiment nodes """

import os
import time
import subprocess

from iotlabaggregator import serial
from iotlabcli.parser import common as common_parser
import json

import algorithm_management

def _neighbours(file_path):
    """" Load neighbours graph file """
    neighbours = {}
    with open(file_path) as neigh_file:
        for line in neigh_file:
            node, neighs = line.strip().split(':')
            neighbours[node] = neighs.split(';')
    return neighbours


def opts_parser():
    """ Argument parser object """
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

    algo_group = parser.add_argument_group(title="Algorithm selection")

    algo_group.add_argument('-a', '--algo', default='syncronous',
                            choices=ALGOS.keys(), help='Algorithm to run')
    algo_group.add_argument('-n', '--num-loop', type=int,
                            dest='num_loop', help='number_of_loops_to_run')

    algo_group.add_argument('-g', '--neighbours-graph', type=_neighbours,
                            dest='neighbours', help='Neighbours csv')

    output = parser.add_argument_group(title="Output selection")
    output.add_argument('-o', '--out-dir', required=True,
                        dest='outdir', help='Output directory')

    return parser


def mkdir_p(outdir):
    """ mkdir -p `outdir` """
    if not os.path.exists(outdir):
        os.makedirs(outdir)


class NodeResults(object):

    def __init__(self, outdir):
        self.outdir = outdir
        self.neighbours = {}

        self.node_measures = {}
        self.node_finale_measures = {}

        mkdir_p(self.outdir)

    def open(self, name, mode='w'):
        """ Open result file """
        outfile = os.path.join(self.outdir, name)
        return open(outfile, mode)

    def handle_line(self, identifier, line):
        """ Print one line prefixed by id in format: """
        _ = identifier
        if line.startswith(('DEBUG', 'INFO', 'ERROR')):
            return

        if 'Neighbours' in line:
            # A569;Neighbours;6;A869;A172;C280;9869;B679;A269
            node, _, _, neighs = line.split(';', 3)

            self.neighbours[node] = sorted(neighs.split(';'))

        elif 'Values' in line:
            # A869;Values;100;1.9330742E9;2.0307378E9
            node, _, num_compute, values = line.split(';', 3)

            values = [str(float(v)) for v in values.split(';')]
            values_d = {'num_compute': int(num_compute), 'values': values}

            self.node_measures.setdefault(node, []).append(values_d)

        elif 'FinalValue' in line:
            # A869;FinalValue;100;32
            node, _, num_compute, final_value  = line.split(';')

            self.node_finale_measures[node] = {
                'num_compute': int(num_compute),
                'value': str(int(final_value)),
            }

    def write_neighbours(self):
        """ Write neighbours output """
        # Write neighbours table
        with self.open('neighbours.csv') as neigh:
            print "Neighbours table written to %s" % neigh.name
            for key, values in sorted(self.neighbours.items()):
                neigh.write('%s:%s\n' % (key, ';'.join(values)))

        # Write 'dot' file
        neighb_graph = self._neighbours_graph()
        with self.open('graph.dot') as dot_f:
            out_dot = dot_f.name
            print "Neighbours dot-graph written to %s" % out_dot
            dot_f.write(neighb_graph)

        # Generate '.png' graph
        out_png = os.path.join(self.outdir, 'graph.png')
        cmd = ['dot', '-T', 'png', out_dot, '-o', out_png]
        try:
            subprocess.call(cmd)
            print "Neighbours graph written to %s" % out_png
        except OSError:
            print "graphviz not installed. Can't generate neighbours graph"
            print "You can run the following command on your comuter:"
            print "    dot -T png results_graph.dot -o results_graph.png"

    def _neighbours_graph(self):
        links = self._neighbours_links()
        res = ''
        res += 'digraph G {\n'
        res += '    center=""\n'
        res += '    concentrate=true\n'
        res += '    graph [ color=black ]\n'
        for (node, neigh) in links['double']:
            res += '    "%s" -> "%s"[color=black]\n' % (node, neigh)
            res += '    "%s" -> "%s"[color=black]\n' % (neigh, node)
        for (node, neigh) in links['simple']:
            res += '    "%s" -> "%s"[color=red]\n' % (node, neigh)
        res += '}\n'
        return res

    def _neighbours_links(self):
        """ List of neighbours graph links """
        simple_links = set()
        double_links = set()
        for node, neighbours in self.neighbours.items():
            # detect double and one sided links
            for neigh in neighbours:
                link = (node, neigh)
                rev_link = (neigh, node)
                # put the link in 'double_links' if reverse was already present
                if rev_link not in simple_links:
                    simple_links.add(link)
                else:
                    simple_links.remove(rev_link)
                    double_links.add(link)
        return {'simple': simple_links, 'double': double_links}

    def write_results(self, use_node_compute=True):
        """ Write all the experiment results """
        self.write_results_values(use_node_compute)
        self.write_results_final_value()

    def write_results_final_value(self):
        """ Write the 'final_value' result files if there is some data """
        if not len(self.node_finale_measures):
            return  # No final value

        all_measures = self.open('final_all.csv')
        print "Write all final value to %s" % all_measures.name

        # write data for each node
        for node, val_d in self.node_finale_measures.items():
            # create the lines
            line = '%s' % (val_d['value'])
            all_line = '%s,%s' % (node, val_d['value'])

            # write per node data
            name = 'final_%s.csv' % (node)
            with self.open(name) as measures:
                print "Write final value to %s" % measures.name
                measures.write(line + '\n')

            all_measures.write(all_line + '\n')
        all_measures.close()

    def write_results_values(self, use_node_compute=True):
        """ Write the results to files """
        all_measures = self.open('all.csv')
        print "Write all values to %s" % all_measures.name

        for node, values in self.node_measures.items():
            name = '%s.csv' % node
            measures = self.open(name)
            print "Write values to %s" % measures.name
            for i, val_d in enumerate(values):
                # use remote compute number or local compute number == num line
                compute_num = val_d['num_compute'] if use_node_compute else i

                # create the lines
                csv_vals = ','.join(val_d['values'])
                line = '%s,%s' % (compute_num, csv_vals)
                all_line = '%s,%s,%s' % (node, compute_num, csv_vals)

                measures.write(line + '\n')
                all_measures.write(all_line + '\n')

            measures.close()
        all_measures.close()


ALGOS = {
    'create_graph': algorithm_management.create_graph,
    'load_graph': algorithm_management.load_graph,
    'print_graph': algorithm_management.print_graph,
    'syncronous': algorithm_management.syncronous_mode,
    'gossip': algorithm_management.gossip_mode,
    'num_nodes': algorithm_management.find_num_node_gossip_mode,
}

def parse():
    parser = opts_parser()
    opts = parser.parse_args()
    opts.with_a8 = False  # HACK for the moment, required by 'select_nodes'

    if (opts.algo not in ['create_graph', 'print_graph'] and
            opts.neighbours is None):
        parser.error('neighbours not provided')
    if (opts.algo in ['syncronous', 'gossip', 'num_nodes'] and
            opts.num_loop is None):
        parser.error('num_loop not provided')

    try:
        opts.nodes_list = serial.SerialAggregator.select_nodes(opts)
    except (ValueError, RuntimeError) as err:
        parser.error('%s' % err)
    return opts


def main():
    """ Reads nodes from ressource json in stdin and
    aggregate serial links of all nodes
    """

    opts = parse()
    print "Using algorithm: %r" % opts.algo
    algorithm = ALGOS[opts.algo]

    # Connect to the nodes
    results = NodeResults(opts.outdir)
    with serial.SerialAggregator(opts.nodes_list, print_lines=True,
                                 line_handler=results.handle_line) as aggr:
        time.sleep(2)
        # Run the algorithm
        algorithm(aggr, num_loop=opts.num_loop, neighbours=opts.neighbours)
        time.sleep(3)


    if opts.algo in ['create_graph', 'print_graph']:
        results.write_neighbours()
    elif opts.algo == 'load_graph':
        pass
    else:
        results.write_results(opts.algo == 'syncronous')


if __name__ == "__main__":
    main()

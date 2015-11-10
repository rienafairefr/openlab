#! /usr/bin/env python
# -*- coding:utf-8 -*-

""" Get the iotlab-uip for all experiment nodes """

import sys
import os
import time
import subprocess

from iotlabaggregator import serial
from iotlabcli.parser import common as common_parser

import algorithm_management as _algos
import parser as _parser


def opts_parser():
    """ Argument parser object """
    parser = _parser.base_parser()

    algo_group = parser.add_argument_group(title="Algorithm selection")

    algo_group.add_argument('-a', '--algo', default='syncronous',
                            choices=ALGOS.keys(), help='Algorithm to run')
    _parser.num_loop(algo_group, required=False)
    _parser.neighbours_graph(algo_group, required=False)
    _parser.txpower(algo_group)

    # For poisson algorithms
    _parser.lambda_t(algo_group, required=False)
    _parser.duration(algo_group, required=False)

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

        self.poisson = {}
        self.clock = {}

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
            node, _, num_compute, final_value = line.split(';')

            self.node_finale_measures[node] = {
                'num_compute': int(num_compute),
                'value': str(int(final_value)),
            }
        elif 'PoissonDelay' in line:
            # 1062;PoissonDelay;4.9169922E-1
            node, _, delay = line.split(';')
            self.poisson.setdefault(node, []).append(float(delay))
        elif 'Clock' in line:
            # 1062;Clock;4.9169922E-1
            node, _, clock = line.split(';')
            val = (time.time(), float(clock))
            self.clock.setdefault(node, []).append(val)

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
            print "    %s" % ' '.join(cmd)

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

    @staticmethod
    def nop(**_):
        pass

    def write_algo_results(self):
        self._write_results_values(use_node_compute=False)
        self._write_results_final_value()

    def write_algo_results_num_compute(self):
        self._write_results_values(use_node_compute=True)
        self._write_results_final_value()

    def _write_results_values(self, use_node_compute=True):
        """ Write the results to files """
        all_measures = self.open('results_all.csv')
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

    def _write_results_final_value(self):
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

    def write_poisson(self):
        """ Write the poisson delay results """
        all_measures = self.open('delay_all.csv')
        print "Write all final value to %s" % all_measures.name

        for node, values in self.poisson.items():
            # Print 1/mean
            mean = sum(values) / float(len(values))
            print 'Poisson: %s: 1/mean == %f' % (node, 1./mean)
            # Save values in file
            for val in values:
                all_measures.write('%s,%f\n' % (node, val))

        all_measures.close()

    def write_clock(self):
        """ Write the clock results """
        all_measures = self.open('clock_all.csv')
        print "Write all final value to %s" % all_measures.name

        for node, values in self.clock.items():
            # Save values in file
            for timestamp, val in values:
                all_measures.write('%s,%f,%f\n' % (node, timestamp, val))

        all_measures.close()


ALGOS = {
    'create_graph': (_algos.create_graph, 'write_neighbours'),
    'print_graph': (_algos.print_graph, 'write_neighbours'),
    'load_graph': (_algos.load_graph, 'nop'),

    'syncronous': (_algos.syncronous, 'write_algo_results_num_compute'),
    'gossip': (_algos.gossip, 'write_algo_results'),
    'clock_convergence': (_algos.clock_convergence, 'write_clock'),
    'num_nodes': (_algos.num_nodes_gossip, 'write_algo_results'),

    'print_poisson': (_algos.print_poisson, 'write_poisson'),
}


def parse():
    parser = opts_parser()
    opts = parser.parse_args()

    if (opts.algo in ['load_graph', 'syncronous', 'gossip', 'num_nodes',
                      'clock_convergence'] and
            opts.neighbours is None):
        parser.error('neighbours not provided')
    if (opts.algo in ['syncronous', 'gossip', 'num_nodes', 'print_poisson'] and
            opts.num_loop is None):
        parser.error('num_loop not provided')
    if opts.algo in ['clock_convergence'] and opts.duration is None:
        parser.error('duration not provided')

    return opts


def algorithm_main(algorithm):
    """ Algorithm generic main function """
    opts = _parser.algorithm_parser().parse_args()
    run(algorithm, opts)


def run(algo, opts):
    """ Execute 'algorithm' and 'handle_result' function name with 'opts'"""

    algorithm, handle_result = ALGOS[algo]

    try:
        opts.with_a8 = False  # HACK for the moment, required by 'select_nodes'
        opts.nodes_list = serial.SerialAggregator.select_nodes(opts)
    except (ValueError, RuntimeError) as err:
        print >> sys.stderr, "Error while calculating nodes list:\n\t%s" % err
        exit(1)

    results = NodeResults(opts.outdir)

    handle_result_fct = getattr(results, handle_result)

    # Connect to the nodes
    with serial.SerialAggregator(opts.nodes_list, print_lines=True,
                                 line_handler=results.handle_line) as aggr:
        time.sleep(2)
        # Run the algorithm
        algorithm(aggr, **vars(opts))
        time.sleep(3)

    # Manage the results
    handle_result_fct()


def main():
    """ Reads nodes from ressource json in stdin and
    aggregate serial links of all nodes
    """
    opts = parse()
    print "Using algorithm: %r" % opts.algo
    run(opts.algo, opts)


if __name__ == "__main__":
    main()

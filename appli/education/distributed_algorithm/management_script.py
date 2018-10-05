#! /usr/bin/env python
# -*- coding:utf-8 -*-

""" Get the iotlab-uip for all experiment nodes """

import sys
import os
import time
import subprocess
import random
import math

from iotlabaggregator import serial
import iotlabcli

import algorithm_management as _algos
import parser as _parser


def opts_parser():
    """ Argument parser object """
    parser = _parser.base_parser()

    algo_group = parser.add_argument_group(title="Algorithm selection")

    algo_group.add_argument('-a', '--algo', default='synchronous',
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
            # 1062;Clock;4.9169922E-1;4.9169922E-1
            node, _, sys_clock, virt_clock = line.split(';')
            val = (time.time(), float(sys_clock), float(virt_clock))
            self.clock.setdefault(node, []).append(val)

    def write_neighbours(self):
        """ Write neighbours output """
        # Write neighbours table
        with self.open('neighbours.csv') as neigh:
            print "Neighbours table written to %s" % neigh.name
            for key, values in sorted(self.neighbours.items()):
                neigh.write('{}:{}\n'.format(key, ';'.join(values)))

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
        lookup_node_name.lookup = get_node_uids_lookup()
        for (node, neigh) in links['double']:
            node, neigh = (get_node_label(node), get_node_label(neigh))
            res += '    "%s" -> "%s"[color=black]\n' % (node, neigh)
            res += '    "%s" -> "%s"[color=black]\n' % (neigh, node)
        for (node, neigh) in links['simple']:
            node, neigh = (get_node_label(node), get_node_label(neigh))
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

    def write_results(self):
        self._write_results_values()
        self._write_results_final_value()

    def write_results_lambda_timestamp(self):
        self._write_results_values(lambda_=0.2)
        self._write_results_final_value()

    def _poisson_timestamps_measures(self, lambda_):
        """ Generate poisson distributed timestamps for measures """
        num_values = max([len(v) for v in self.node_measures.values()])

        # There is num_values + 1 values with 0 at first
        timestamps = [0.0]
        for i in range(0, num_values):
            delay = - math.log(random.random()) / lambda_
            timestamps.append(timestamps[i] + delay)

        return timestamps

    def _write_results_values(self, lambda_=0.0):
        """ Write the results to files """
        all_measures = self.open('results_all.csv')
        print "Write all values to %s" % all_measures.name

        # Generate a false poisson clock timestamp
        # Using a real one would make the experiment take too long.
        # Use lambda = lambda_ * num_nodes
        if lambda_:
            lambda_ *= len(self.node_measures)
            poisson_times = self._poisson_timestamps_measures(lambda_)

        for node, values in self.node_measures.items():
            for i, val_d in enumerate(values, start=1):

                # create the lines
                csv_vals = ','.join(val_d['values'])
                all_line = '%s,%s,%s' % (node, i, csv_vals)

                if lambda_:
                    all_line += ',%f' % poisson_times[i]

                all_measures.write(all_line + '\n')

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
            all_line = '%s,%s' % (node, val_d['value'])
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
            for timestamp, sys_clock, virt_clock in values:
                all_measures.write('%s,%f,%f,%f\n' % (node, timestamp,
                                                      sys_clock, virt_clock))

        all_measures.close()


ALGOS = {
    'create_graph': (_algos.create_graph, 'write_neighbours'),
    'print_graph': (_algos.print_graph, 'write_neighbours'),
    'load_graph': (_algos.load_graph, 'nop'),

    'synchronous': (_algos.synchronous, 'write_results'),

    'gossip': (_algos.gossip, 'write_results_lambda_timestamp'),
    'num_nodes': (_algos.num_nodes_gossip, 'write_results_lambda_timestamp'),

    'clock_convergence': (_algos.clock_convergence, 'write_clock'),

    'print_poisson': (_algos.print_poisson, 'write_poisson'),
}


def parse():
    parser = opts_parser()
    opts = parser.parse_args()

    if (opts.algo in ['load_graph', 'synchronous', 'gossip', 'num_nodes',
                      'clock_convergence'] and
            opts.neighbours is None):
        parser.error('neighbours not provided')
    if (opts.algo in ['synchronous', 'gossip', 'num_nodes', 'print_poisson']
            and opts.num_loop is None):
        parser.error('num_loop not provided')
    if opts.algo in ['clock_convergence'] and opts.duration is None:
        parser.error('duration not provided')

    return opts


def algorithm_main(algorithm):
    """ Algorithm generic main function """
    opts = _parser.algorithm_parser().parse_args()
    run(algorithm, opts)


def poisson_main(algorithm):
    """ Algorithm poisson generic main function """
    opts = _parser.poisson_parser().parse_args()
    run(algorithm, opts)


def run(algo, opts):
    """ Execute 'algorithm' and 'handle_result' function name with 'opts'"""

    algorithm, handle_result = ALGOS[algo]

    results = NodeResults(opts.outdir)

    handle_result_fct = getattr(results, handle_result)

    if not opts.nodes_list:
        opts.nodes_list = get_nodes_list(opts)

    # Connect to the nodes
    with serial.SerialAggregator(opts.nodes_list, print_lines=True,
                                 line_handler=results.handle_line) as aggr:
        time.sleep(2)
        # Run the algorithm
        algorithm(aggr, **vars(opts))
        time.sleep(3)

    # Manage the results
    handle_result_fct()


def get_nodes_list(opts):
    api = iotlabcli.Api(* iotlabcli.get_user_credentials())
    exp_id = iotlabcli.helpers.get_current_experiment(api, opts.experiment_id)
    return iotlabcli.parser.common._get_experiment_nodes_list(api, exp_id)


def get_node_uids_lookup():
    api = iotlabcli.Api(* iotlabcli.get_user_credentials())
    info = iotlabcli.experiment.info_experiment(api)
    return  { x["uid"]: x["network_address"].split('.')[0]
                  for x in info["items"] }


def get_node_label(node_uid):
    fmt = "{}\n{}"
    return fmt.format(lookup_node_name(node_uid), node_uid)


def lookup_node_name(node_uid):
    try:
        return lookup_node_name.lookup[node_uid.lower()]
    except:
        return "m3-???"

def main():
    """ Reads nodes from ressource json in stdin and
    aggregate serial links of all nodes
    """
    opts = parse()
    print "Using algorithm: %r" % opts.algo
    run(opts.algo, opts)


if __name__ == "__main__":
    main()

#! /usr/bin/env python
# -*- coding:utf-8 -*-

"""Plot csv values.

usage: plot_values.py [-h] [--split]
                     measures_csv [values[,color] [values[,color] ...]]

positional arguments:
  measures_csv
  values[,color]

optional arguments:
  -h, --help      show this help message and exit
  --split
"""

usage_examples="""

for exercices 1 and 2:

    ./plot_values.py results/ex1/results_all.csv


for exercice "clock_convergence"

    ./plot_values.py ex_clock_convergence/clock_all.csv  1,b 2,r

"""


import argparse
from matplotlib import pyplot


def plot_values(node, values, val_num, color=None):
    """Plot the values."""
    print "Printing %s datas" % node,
    args = []
    if color is not None:
        args += color

    t_s = [v[0] for v in values]
    val = [v[val_num] for v in values]
    print "Diff %r" % (val[0] - val[-1])
    pyplot.plot(t_s, val, *args)


def read_values(csv_fd):
    """Extract values as a per node dict."""
    ret_values = {}
    for rows in csv_fd:
        values = rows.strip().split(',')
        node = values[0]
        values = [float(v) for v in values[1:]]
        ret_values.setdefault(node, []).append(tuple(values))
    return ret_values


def plot_select(sel_str):
    """Select plots.

    >>> plot_select('0')
    (0, None)

    >>> plot_select('1,r')
    (1, 'r')
    """
    sel = sel_str.split(',')

    num = int(sel[0])
    args = sel[1] if len(sel) > 1 else None

    return num, args


PARSER = argparse.ArgumentParser()
PARSER.add_argument('--split', default=False, action='store_true')
PARSER.add_argument('--xkcd', default=False, action='store_true')
PARSER.add_argument('measures_csv', type=argparse.FileType())
PARSER.add_argument('plot_select', metavar='values[,color]', nargs='*',
                    type=plot_select)


def main():
    """Plot values from measures_csv according to plot_select.

    If --split is provided, print each plot selection in a different figure.
    """
    opts = PARSER.parse_args()
    values = read_values(opts.measures_csv)

    if not opts.plot_select:
        opts.plot_select = [plot_select('1')]

    if opts.xkcd:
        pyplot.xkcd()

    for num, plot_arg in opts.plot_select:
        if opts.split:
            pyplot.figure()

        for node, vals in values.items():
            plot_values(node, vals, num, plot_arg)

    pyplot.show()


if __name__ == '__main__':
    main()

#! /usr/bin/env python
# -*- coding:utf-8 -*-

import management_script as _manage
import parser as _parser


def main():
    """ Run 'graph_create' algorithm """
    parser = _parser.base_parser()
    _opts = parser.add_argument_group(title="Network Graph options")
    _parser.txpower(_opts)

    opts = parser.parse_args()
    _manage.run('create_graph', opts)


if __name__ == '__main__':
    main()

#! /usr/bin/env python
# -*- coding:utf-8 -*-

import os
import sys

import serial
import argparse

PARSER = argparse.ArgumentParser()
PARSER.add_argument('port')
PARSER.add_argument('baudrate')


class ConsumeBuff(Exception):
    def __init__(self, buff, num):
        self.buff = buff
        self.num = num

    def print_bytes(self):
        sys.stdout.write(self.buff[0: self.num])
        return self.buff[self.num:]


GCDA = 'GCDA:'

PRINT_GCDA_CONTENT = False


class GCDAParser(object):

    GCDA_START = '<GCDA>\n'
    GCDA_END = '</GCDA>\n'

    STATES = ['DEFAULT', 'READ_SIZE', 'READ_NAME', 'WAIT_GCDA_END']

    def __init__(self, print_fct):
        self.print_fct = print_fct
        self.state = 'DEFAULT'

        self.length = None
        self.filename = None
        self.data = ''

    def parse_line(self, line):

        if self.state == 'DEFAULT':
            self.data = ''
            if line == self.GCDA_START:
                self.state = 'READ_SIZE'

        elif self.state == 'READ_SIZE':
            try:
                self.length = int(line)
                self.state = 'READ_NAME'
            except ValueError:
                self.state = 'DEFAULT'


        elif self.state == 'READ_NAME':
            self.filename = line[:-1]
            self.state = 'WAIT_GCDA_END'

        elif self.state == 'WAIT_GCDA_END':
            if line == self.GCDA_END:

                if len(self.data) -1 == self.length:  # final '\n'
                    self.print_fct('    <GCDA_DATA:%d>\n' % (len(self.data) - 1))
                    handle_gcda(self.filename, self.data[:-1])
                else:
                    self.print_fct('    <Invalid GCDA: len %u != expected %u>\n',
                                   len(self.data) - 1, self.length)
                self.state = 'DEFAULT'

            elif len(self.data) -1  <= self.length:
                self.data += line
                line = ''  # for printing
            else:
                self.print_fct('    <Invalid GCDA: len %u != expected %u>\n',
                               len(self.data) - 1, self.length)
                self.state = 'DEFAULT'

        else:
            print "Invalid STATE: %s" % self.state
        self.print_fct(line)


    def _debug(self):
        print "\n%s:(%d): '%s'" % (self.state, len(self.data), self.data)



def handle_gcda(filename, data):
    print 'Got GCDA (%d) for filename %s' % (len(data), filename)
    write_gcda(filename, data)


def write_gcda(filename, data):
    filepath = os.path.expanduser(filename)
    print 'Writing %s bytes to %s' % (len(data), os.path.relpath(filepath))
    with open(filepath, 'wb') as gcda_file:
        gcda_file.write(data)


def read_serial(ser):
    gcda_parser = GCDAParser(sys.stdout.write)
    while True:
        gcda_parser.parse_line(ser.readline())

def main():
    opts = PARSER.parse_args()
    ser = serial.Serial(opts.port, opts.baudrate)
    try:
        read_serial(ser)
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()

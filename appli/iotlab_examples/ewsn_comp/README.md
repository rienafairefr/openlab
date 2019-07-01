EWSN 2020 Competition: GPS synced nodes
---------------------------------------

This example shows how to use the GPS to sync A8-M3 nodes

Some A8 open-nodes are equiped with a GPS module allowing a precise time synchronization. The hardware architecture is presented here https://github.com/iot-lab/iot-lab/wiki/Hardware_A8-GPS.

**This type of node with GPS is mandatory for this example**. If you want how to verify the GPS signal see the [FAQ](https://github.com/iot-lab/iot-lab/wiki/Verify-the-GPS-signal-on-a8-node).

## Prerequisites

- Tutorial : [Submit an experiment with A8 nodes using the web portal](https://www.iot-lab.info/tutorials/submit-an-experiment-with-a8-nodes-using-the-web-portal/)
- Tutorial : [Get and compile a M3 Firmware code](https://www.iot-lab.info/tutorials/get-compile-a-m3-firmware-code/)

## Overview

The software is divided in 4 components:
- receiver: prints zep on serial line
- sender: sends messages

## Running a demo

#### 1 . Grab at least 3 A8 nodes in saclay with GPS
- 1 sender node nodes
- n receiver nodes

        $ experiment-cli submit -d 15 -l saclay,a8,....


Note: use auth-cli if this is a first time init

#### 2. Build firmware files

    ~$ cd openlab/ && mkdir build.a8
    ~/openlab/$ cd build.a8/ && cmake .. -DPLATFORM=iotlab-a8-m3
    ~/openlab/build.a8/$ make ewsn_receiver ewsn_sender

#### 3. Check that all nodes have booted

    ~$ iotlab-ssh wait-for-boot

#### 4. Flash firmwares

    ~/openlab/build.a8/$ iotlab-ssh flash-m3 -l saclay,a8,<sender_id> bin/ewsn_sender.elf
    ~/openlab/build.a8/$ iotlab-ssh flash-m3 -e saclay,a8,<sender_id> bin/ewsn_receiver.elf

#### 5. See serial output(s)

    on all nodes: miniterm.py /dev/iotlab/ttyA8_M3 500000

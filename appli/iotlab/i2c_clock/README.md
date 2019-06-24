configure & build with 
```
cmake .. -DPLATFORM=iotlab-a8-m3

make i2c_clock
```

flash on an A8 M3

run 
```
miniterm.py --filter colorize /dev/iotlab/tty_A8_M3 500000
```

on the A8 to get the M3 serial log/debug

in another terminal, try to dialog with the A8-M3 node through the I2C bus on /dev/i2c-2

There should be a 'read node_id' when sending the header 0x01

- I used python-periphery, didn't work
- I opened /dev/i2c-2 in a C program, 
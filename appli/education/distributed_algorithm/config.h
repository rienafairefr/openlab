#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include "printf.h"
#include "iotlab_uid.h"
#include "phy.h"


// choose channel in [11-26]
#define CHANNEL 12

#define NUM_VALUES           1

// threshold for selecting neighbours, approx range: -90dBm to -50dBm
#define MIN_RSSI           -70

#define TIME_SCALE           1.0
#define TIME_SCALE_RANDOM    0.2
#define TIME_OFFSET_RANDOM   0.0

#define MAC_PKT_LEN (PHY_MAX_TX_LENGTH - 4)
#define MAX_NUM_NEIGHBOURS  ((MAC_PKT_LEN -2)/ sizeof(uint16_t))


#define MSG(fmt, ...) printf("%04x;" fmt, iotlab_uid(), ##__VA_ARGS__)

#define ERROR(fmt, ...) printf("ERROR:" fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) printf("INFO:" fmt, ##__VA_ARGS__)
#if 0
#define DEBUG(fmt, ...) printf("DEBUG:" fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif



#endif//CONFIG_H

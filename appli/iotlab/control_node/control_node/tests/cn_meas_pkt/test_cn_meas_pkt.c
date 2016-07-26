#include "platform.h"

#include <string.h>

#include "printf.h"
#include "debug.h"

#include "cn_meas_pkt.h"

/* Mocks */
int32_t iotlab_serial_send_frame_mock(uint8_t type, iotlab_packet_t *pkt);
#define iotlab_serial_send_frame iotlab_serial_send_frame_mock

#include "cn_meas_pkt.c"

#define ASSERT(exp)\
    do {\
        if(!(exp))\
        {\
            log_error("ASSERT(%s) failed at line %d", #exp, __LINE__);\
        }\
    }\
    while(0);



static void test_cn_meas_pkt_lazy_alloc()
{
    struct soft_timer_timeval timestamp = {0, 0};
    /* Init */
    iotlab_packet_queue_t free_packets;
    iotlab_packet_t packets[2];

    iotlab_packet_init_queue(&free_packets, packets, 2);
    ASSERT(free_packets.count == 2);  // Just to be sure

    iotlab_packet_t *current = NULL;
    iotlab_packet_t *prev = NULL;
    iotlab_packet_t content;

    /* Lazy alloc when no packet */
    timestamp = (struct soft_timer_timeval){1, 0};
    current = cn_meas_pkt_lazy_alloc(&free_packets, current, &timestamp);
    ASSERT(current != NULL);
    ASSERT(free_packets.count == 1);

    /* Lazy alloc when packet already allocated */
    timestamp = (struct soft_timer_timeval){2, 0};
    prev = current;
    memcpy(&content, current, sizeof(iotlab_packet_t));
    current = cn_meas_pkt_lazy_alloc(&free_packets, current, &timestamp);
    ASSERT(current != NULL);
    ASSERT(current == prev);  // Same packet as before
    ASSERT(free_packets.count == 1);  // None allocated

    /* Packet not updated by lazy alloc when already have a packet */
    ASSERT(memcmp(&content, current, sizeof(iotlab_packet_t)) == 0);

    /* Try allocating a new packet */
    current = cn_meas_pkt_lazy_alloc(&free_packets, NULL, &timestamp);
    ASSERT(current != prev);
    ASSERT(current != NULL);
    ASSERT(free_packets.count == 0);

    /* No more packets, return null */
    current = cn_meas_pkt_lazy_alloc(&free_packets, NULL, &timestamp);
    ASSERT(current == NULL);
    ASSERT(free_packets.count == 0);

    /* Now finally check packet content */
    uint32_t t_s = 0;
    ASSERT(content.p.data[0] == 0);  // No measures
    ASSERT(content.p.length  == 5);  // Num measure + timestamp
    memcpy(&t_s, &content.p.data[1], 4);
    ASSERT(t_s == 1);                // Timestamp seconds == 1
}



static void dump_pkt(const uint8_t *s1, size_t n)
{
    int i;
    printf("Packet content: ");

    for (i = 0; i < (n - 1); i++)
        printf("0x%02x, ", s1[i]);
    printf("0x%02x", s1[n-1]);

    printf("\n");
}

#define ASSERT_PKT_EQUALS(packet, s2, n) do {\
    uint8_t *s1 = (packet)->p.data;          \
    ASSERT((packet)->p.length == n);         \
    ASSERT(memcmp(s1, (s2), n) == 0);        \
    if (memcmp(s1, (s2), n))                 \
        dump_pkt(s1, n);                     \
} while(0);

static void test_cn_meas_add_measure()
{
    iotlab_packet_queue_t free_packets;
    iotlab_packet_t packets[1];
    iotlab_packet_init_queue(&free_packets, packets, 1);

    iotlab_packet_t *packet = NULL;
    struct soft_timer_timeval t0 = {1, 500000};
    int send = 0;


    /*
     *  Test until packet full
     */

    /* Alloc packet */
    packet = cn_meas_pkt_lazy_alloc(&free_packets, packet, &t0);
    ASSERT(packet != NULL);
    /* Contains no measures and timestamp == 1 */
    ASSERT_PKT_EQUALS(packet, ((uint8_t[]){0, 1, 0, 0, 0}), 5);

    /* Alloc many measures */
    uint32_t forty_two = 42;
    uint32_t max = ~0;
    struct cn_meas measure[] = {{&forty_two, sizeof(uint32_t)},
                                {&max, sizeof(uint32_t)},
                                {NULL, 0}};
    size_t meas_size = sizeof(uint32_t) + 2 * sizeof(uint32_t);

    /* Measure 1 */
    send = cn_meas_pkt_add_measure(packet,
            (struct soft_timer_timeval[]){{1, 0x927c0}},  // 600000
            meas_size, measure);
    ASSERT_PKT_EQUALS(packet,
            ((uint8_t[]){1, 1, 0, 0, 0,
             0xc0, 0x27, 0x09, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff}),
            5 + 12);
    ASSERT(send == 0);

    /* Measure 2 */
    send = cn_meas_pkt_add_measure(packet,
            (struct soft_timer_timeval[]){{1, 0xaae60}},  // 700000
            meas_size, measure);
    ASSERT_PKT_EQUALS(packet,
            ((uint8_t[]){2, 1, 0, 0, 0,
             0xc0, 0x27, 0x09, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
             0x60, 0xae, 0x0a, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff}),
            5 + 12 + 12);
    ASSERT(send == 0);

    int i;
    uint32_t meas_t_us = 800000;
    for (i = 3; i < (IOTLAB_SERIAL_DATA_MAX_SIZE - 5) / 12; i++) {
        send = cn_meas_pkt_add_measure(packet,
                (struct soft_timer_timeval[]){{1, meas_t_us + 10000 * i}},
                meas_size, measure);
        ASSERT(send == 0);  // Still room
    }

    /* Packet full */
    send = cn_meas_pkt_add_measure(packet,
            (struct soft_timer_timeval[]){{1, meas_t_us + 10000 * i}},
            meas_size, measure);
    ASSERT(send == 1);
    // Cleanup
    iotlab_packet_call_free(packet);
    packet = NULL;


    /*
     *  Packet with too big timestamp
     */

    /* Alloc packet */
    packet = cn_meas_pkt_lazy_alloc(&free_packets, packet, &t0);
    ASSERT(packet != NULL);
    ASSERT_PKT_EQUALS(packet, ((uint8_t[]){0, 1, 0, 0, 0}), 5);
    /* Measure 1 */
    send = cn_meas_pkt_add_measure(packet,
            (struct soft_timer_timeval[]){{1, 0x927c0}},  // 600000
            meas_size, measure);
    ASSERT_PKT_EQUALS(packet,
            ((uint8_t[]){1, 1, 0, 0, 0,
             0xc0, 0x27, 0x09, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff}),
            5 + 12);
    ASSERT(send == 0);

    /* Measure 2 */
    send = cn_meas_pkt_add_measure(packet,
            (struct soft_timer_timeval[]){{4, 0}}, // Diff 3s == 0x1e8480 us
            meas_size, measure);
    ASSERT_PKT_EQUALS(packet,
            ((uint8_t[]){2, 1, 0, 0, 0,
             0xc0, 0x27, 0x09, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
             0xc0, 0xc6, 0x2d, 0x00, 0x2a, 0, 0, 0, 0xff, 0xff, 0xff, 0xff}),
            5 + 12 + 12);

    /* 3 > 2s */
    ASSERT(send == 1);
}



static int packet_free_call_count = 0;
static void pkt_free()
{
    packet_free_call_count++;
}

static int iotlab_serial_send_frame_mock_call_count = 0;
static int32_t iotlab_serial_send_frame_mock_return_value = 0;
static int iotlab_serial_send_frame_mock_type = 0;
int32_t iotlab_serial_send_frame_mock(uint8_t type, iotlab_packet_t *pkt)
{
    iotlab_serial_send_frame_mock_call_count++;
    iotlab_serial_send_frame_mock_type = type;
    return iotlab_serial_send_frame_mock_return_value;
};

static void test_cn_meas_pkt_flush()
{
    iotlab_packet_t packet = {.free = pkt_free};
    iotlab_packet_t *pkt = &packet;

    /* Send regular packet */
    cn_meas_pkt_flush(&pkt, 42);
    ASSERT(iotlab_serial_send_frame_mock_call_count == 1);
    ASSERT(iotlab_serial_send_frame_mock_type == 42);
    ASSERT(packet_free_call_count == 0);
    ASSERT(pkt == NULL);
    iotlab_serial_send_frame_mock_call_count = 0;

    /* Issue while sending, packet should be freed */
    iotlab_serial_send_frame_mock_return_value = 1;  // Error
    pkt = &packet;

    cn_meas_pkt_flush(&pkt, 42);
    ASSERT(iotlab_serial_send_frame_mock_call_count == 1);
    ASSERT(packet_free_call_count == 1);
    ASSERT(pkt == NULL);
    packet_free_call_count = 0;
    iotlab_serial_send_frame_mock_call_count = 0;

    /* Ignore NULL packets */
    pkt = NULL;
    iotlab_serial_send_frame_mock_call_count = 0;

    cn_meas_pkt_flush(&pkt, 42);
    ASSERT(iotlab_serial_send_frame_mock_call_count == 0);
    ASSERT(packet_free_call_count == 0);
    ASSERT(pkt == NULL);
}


static void test_app(void *arg)
{
    (void)arg;
    log_info("Running tests");
    test_cn_meas_pkt_lazy_alloc();
    test_cn_meas_add_measure();
    test_cn_meas_pkt_flush();

    log_info("Tests finished");

    vTaskDelete(NULL);
}

int main()
{
    platform_init();
    soft_timer_init();

    // Create a task for the application
    xTaskCreate(test_app, (signed char *) "test_cn_meas_pkt",
            4 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);


    platform_run();

    return 0;
}

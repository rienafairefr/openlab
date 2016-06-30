#include "platform.h"

#include <string.h>

#include "printf.h"
#include "debug.h"

#include "iotlab_packet.h"

#define ASSERT(exp)\
    do {\
        if(!(exp))\
        {\
            log_error("ASSERT(%s) failed at line %d", #exp, __LINE__);\
        }\
    }\
    while(0);

iotlab_packet_queue_t free_packets;
iotlab_packet_queue_t free_packets_2;
iotlab_packet_queue_t packets_fifo;

#define NUM_PKTS 10
iotlab_packet_t packets[NUM_PKTS];
iotlab_packet_t packets_2[NUM_PKTS];


static void test_init_alloc_free()
{
    iotlab_packet_init_queue(&free_packets, packets, 10);
    iotlab_packet_init_queue(&free_packets_2, packets_2, 10);

    ASSERT(free_packets.count == NUM_PKTS);
    ASSERT(free_packets.head.next != NULL);
    int i;

    // Alloc all packets
    for (i = 0; i < NUM_PKTS; i++) {
        iotlab_packet_t *packet = iotlab_packet_alloc(&free_packets, 11);
        ASSERT(packet != NULL);
        ASSERT(free_packets.count == (NUM_PKTS -i -1));
    }

    ASSERT(free_packets.count == 0);
    iotlab_packet_t *packet = iotlab_packet_alloc(&free_packets, 11);
    ASSERT(packet == NULL);
    ASSERT(free_packets.head.next == NULL);
    // Not touched to the second one
    ASSERT(free_packets_2.count == NUM_PKTS);

    // Free packets
    for (i = 0; i < NUM_PKTS; i++) {
        iotlab_packet_call_free(&packets[i]);
        ASSERT(free_packets.count == i + 1);
    }

    // Not touched to the second one
    ASSERT(free_packets_2.count == NUM_PKTS);
}


#define packet_put_string(packet, string) iotlab_packet_append_data((packet), (string), (sizeof (string)))

#define set_prio_and_put(queue, packet, prio) do {     \
    (packet)->priority = (prio);                           \
    iotlab_packet_fifo_prio_append((queue), (packet)); \
} while(0)

static void test_fifo()
{

    char data[32] = {'\0'};

    iotlab_packet_init_queue(&packets_fifo, NULL, 0);

    iotlab_packet_t *packet = NULL;
    iotlab_packet_t *a = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "a");

    iotlab_packet_t *b = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "b");

    iotlab_packet_t *c = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "c");

    iotlab_packet_t *d = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "d");

    iotlab_packet_t *e = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "e");

    iotlab_packet_t *f = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "f");

    iotlab_packet_t *g = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "g");

    iotlab_packet_t *h = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "h");

    iotlab_packet_t *i = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "i");

    iotlab_packet_t *j = packet = iotlab_packet_alloc(&free_packets, 0);
    packet_put_string(packet, "j");


    iotlab_packet_t *k = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "k");

    iotlab_packet_t *l = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "l");

    iotlab_packet_t *m = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "m");

    iotlab_packet_t *n = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "n");

    iotlab_packet_t *o = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "o");

    iotlab_packet_t *p = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "p");

    iotlab_packet_t *q = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "q");

    iotlab_packet_t *r = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "r");

    iotlab_packet_t *s = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "s");

    iotlab_packet_t *t = packet = iotlab_packet_alloc(&free_packets_2, 0);
    packet_put_string(packet, "t");

    /*  a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   p   q   r   s   t
     * 42  10   8   7   7   6   5   5   5   4   3   2   1   1   1   1   1   0   0   0
     */

    set_prio_and_put(&packets_fifo, g, 5);

    set_prio_and_put(&packets_fifo, c, 8);

    set_prio_and_put(&packets_fifo, m, 1);
    set_prio_and_put(&packets_fifo, n, 1);
    set_prio_and_put(&packets_fifo, d, 7);

    set_prio_and_put(&packets_fifo, f, 6);

    set_prio_and_put(&packets_fifo, h, 5);


    set_prio_and_put(&packets_fifo, a, 42);
    set_prio_and_put(&packets_fifo, b, 10);

    set_prio_and_put(&packets_fifo, i, 5);

    set_prio_and_put(&packets_fifo, r, 0);
    set_prio_and_put(&packets_fifo, s, 0);

    set_prio_and_put(&packets_fifo, j, 4);
    set_prio_and_put(&packets_fifo, k, 3);
    set_prio_and_put(&packets_fifo, l, 2);

    set_prio_and_put(&packets_fifo, o, 1);

    set_prio_and_put(&packets_fifo, e, 7);

    set_prio_and_put(&packets_fifo, p, 1);
    set_prio_and_put(&packets_fifo, q, 1);

    set_prio_and_put(&packets_fifo, t, 0);

    ASSERT(packets_fifo.count == 20);
    int num = 0;
    while (iotlab_packet_fifo_count(&packets_fifo)) {
        packet = iotlab_packet_fifo_get(&packets_fifo);
        data[num++] = ((packet_t *)packet)->data[0];

        ASSERT(((packet_t *)packet)->length == 2);
        iotlab_packet_call_free(packet);
    }
    ASSERT(num == 20);
    memcpy(&data[num], "uvwxyz", sizeof("uvwxyz")); // include '\0'
    // values are sorted in correct order
    ASSERT(strcmp(data, "abcdefghijklmnopqrstuvwxyz") == 0);

    // Packets have been freed in correct queue
    ASSERT(free_packets.count == 10);
    ASSERT(free_packets_2.count == 10);
}


static void put_to_fifo(handler_arg_t packet)
{
    iotlab_packet_fifo_prio_append(&packets_fifo, (iotlab_packet_t *)packet);
}

static void test_blocking_fifo_get()
{
    uint32_t t_0, t_end;
    soft_timer_t timer;

    iotlab_packet_t *packet = iotlab_packet_alloc(&free_packets, 0);
    iotlab_packet_t *result = NULL;

    t_0 = soft_timer_time();

    soft_timer_set_handler(&timer, put_to_fifo, (handler_arg_t)packet);
    soft_timer_start(&timer, soft_timer_s_to_ticks(1), 0);

    result = iotlab_packet_fifo_get(&packets_fifo);
    t_end = soft_timer_time();
    ASSERT(result != NULL);
    ASSERT(result == packet);
    ASSERT(t_end - t_0 >= (soft_timer_s_to_ticks(1) - soft_timer_ms_to_ticks(1)));
}


static void test_app(void *arg)
{
    (void)arg;
    log_info("Running tests");
    test_init_alloc_free();

    test_fifo();
    test_blocking_fifo_get();

    log_info("Tests finished");

    vTaskDelete(NULL);
}

int main()
{
    platform_init();
    soft_timer_init();

    // Create a task for the application
    xTaskCreate(test_app, (signed char *) "test_iotlab_packet",
            4 * configMINIMAL_STACK_SIZE, NULL, 1, NULL);


    platform_run();

    return 0;
}

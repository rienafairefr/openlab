#include "cn_priority.h"
#include "event_priorities.h"


#define MAX_PRIO (configMAX_PRIORITIES - 1)

#define SERIAL_PRIO  (MAX_PRIO)
#define NETWORK_PRIO (SERIAL_PRIO -1)
#define APPLI_PRIO   (NETWORK_PRIO -2)

const unsigned cn_priority_serial        = SERIAL_PRIO;
const unsigned cn_priority_event_network = NETWORK_PRIO;
const unsigned cn_priority_event_appli   = APPLI_PRIO;


const event_priorities_t event_priorities = {
    APPLI_PRIO,
    NETWORK_PRIO,
};

// B debug = simulated sensor with one publication every 10 seconds and
// detailed Classical CAN logging.
#define NODE_ROLE_A 0
#define CANLAB_DEBUG 1
#define CANLAB_SAMPLE_INTERVAL_MS 10000UL
#define CANLAB_STALE_INTERVAL_MS 50000UL
#include "CanLabNode.h"

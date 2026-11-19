// A debug = simulated motor/consumer with detailed Classical CAN logging.
// The shared implementation keeps the normal A_Motor sketch unchanged.
#define NODE_ROLE_A 1
#define CANLAB_DEBUG 1
#define CANLAB_SAMPLE_INTERVAL_MS 10000UL
#define CANLAB_STALE_INTERVAL_MS 50000UL
#include "CanLabNode.h"

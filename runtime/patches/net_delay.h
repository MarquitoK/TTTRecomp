#pragma once
#include "patches/registry.h"

// WLAN lockstep input delay: -1 automatic from the round trip (host decides per scene), 0 the game's own (1 frame), N fixed frames
PATCH(net_delay, "TTT_NET_DELAY", -1)

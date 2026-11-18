#pragma once
#include "patches/registry.h"

// local multiplayer in one process: number of human players, each on his own device (item 20)
PATCH(local_players, "TTT_LOCAL_PLAYERS", 1)

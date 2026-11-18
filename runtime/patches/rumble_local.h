#pragma once
#include "patches/registry.h"

// animation rumble follows the local player (0x0885862C) instead of fighter slot 0
PATCH(rumble_local, "TTT_RUMBLE_LOCAL", 1)

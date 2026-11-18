#pragma once
#include "patches/registry.h"

// per-player HUD in split screen: 0 off, 1 copies per player, 2 and 3 development modes (item 20)
PATCH(split_hud, "TTT_SPLIT_HUD", 1)

#pragma once
#include "patches/registry.h"

// two screens: player 2's view is drawn into its own render target at full resolution instead of half the canvas
PATCH(dual_full, "TTT_DUAL_FULL", 1)

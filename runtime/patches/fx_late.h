#pragma once
#include "patches/registry.h"

// Screen effects at the end of the frame when no HUD draw came after the scene (bars behind the fighters, HUD hidden)
PATCH(fx_late, "TTT_FX_LATE", 1)

#pragma once
#include "patches/registry.h"

// Bars behind the fighters: the screen effects (water, rain, atmosphere) are not applied over the bars
PATCH(fx_keep_bars, "TTT_FX_KEEP_BARS", 1)

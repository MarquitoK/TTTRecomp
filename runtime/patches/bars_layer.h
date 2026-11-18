#pragma once
#include "patches/registry.h"

// Bars behind the fighters: the bars are drawn again on a float layer so the screen effects show through them by their real opacity
PATCH(bars_layer, "TTT_BARS_LAYER", 1)

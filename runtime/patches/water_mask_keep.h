#pragma once
#include "patches/registry.h"

// Enhanced water: the screen alpha with the water mask is kept from the scene batch to the atmosphere pass
PATCH(water_mask_keep, "TTT_WATER_MASK_KEEP", 1)

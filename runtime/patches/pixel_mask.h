#pragma once
#include "patches/registry.h"

// GL path: per-bit pixel mask inside a colour channel (fighter shadows write one bit each)
PATCH(pixel_mask, "TTT_PIXEL_MASK", 1)

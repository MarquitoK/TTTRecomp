#pragma once
#include "patches/registry.h"

// The scene camera read by the atmosphere (Enhanced water, Enhanced rain) may live in the port heap
PATCH(cam_heap, "TTT_CAM_HEAP", 1)

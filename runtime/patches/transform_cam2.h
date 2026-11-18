#pragma once
#include "patches/registry.h"

// Transformation type 2 (normal with cman, BT3 TransformC): TTT keeps the action (0xFC, 0x0897E024) but refuses the type
PATCH(transform_cam2, "TTT_TRANSFORM_CAM2", 1)

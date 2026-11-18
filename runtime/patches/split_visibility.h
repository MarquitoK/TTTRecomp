#pragma once
#include "patches/registry.h"

// split screen: model visibility and detail recomputed with each view's camera (item 20)
PATCH(split_visibility, "TTT_SPLIT_VIS", 1)

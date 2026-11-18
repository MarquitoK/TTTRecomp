#pragma once
#include "patches/registry.h"

// split screen: small render targets (below 480 wide) drawn only in the first view of each frame, reused by the rest;
// off: each view assigns the shadow bits and near/far silhouettes with its own camera (item 23)
PATCH(split_rt_once, "TTT_SPLIT_RT", 0)

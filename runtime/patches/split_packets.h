#pragma once
#include "patches/registry.h"

// split screen: the frame's render packet buffers enlarged so several views fit
PATCH(split_packets, "TTT_SPLIT_PACKETS", 1)

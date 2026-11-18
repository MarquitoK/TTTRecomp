#pragma once
#include "patches/registry.h"

// four views at 60: each frame draws two of them (top pair, bottom pair) and the other two keep their last image
PATCH(split_alt, "TTT_SPLIT_ALT", 1)

#pragma once
#include "patches/registry.h"

// GE fast paths with identical output: early scissor reject, one state per primitive, vertex reuse in indexed lists
PATCH(ge_fast, "TTT_GE_FAST", 1)

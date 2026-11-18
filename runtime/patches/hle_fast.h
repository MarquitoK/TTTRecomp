#pragma once
#include "patches/registry.h"

// Interrupt suspend/resume and memset answered before the import name chain
PATCH(hle_fast, "TTT_HLE_FAST", 1)

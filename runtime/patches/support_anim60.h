#pragma once
#include "patches/registry.h"

// Battle at 60: animations started without a speed setter (supports, extra characters) keep the 30 Hz default 2.0
PATCH(support_anim60, "TTT_SUPPORT60", 1)

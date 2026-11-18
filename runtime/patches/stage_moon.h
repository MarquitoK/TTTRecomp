#pragma once
#include "patches/registry.h"

// Night moon: the stage sprite (entry 0x14) skipped at stage init is loaded when a "Night Only" transformation starts
PATCH(stage_moon, "TTT_STAGE_MOON", 1)

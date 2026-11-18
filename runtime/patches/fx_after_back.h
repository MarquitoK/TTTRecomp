#pragma once
#include "patches/registry.h"

// Screen effects (atmosphere, water, rain) never start at the bars-behind-fighters packet: they wait for the HUD after the scene
PATCH(fx_after_back, "TTT_FX_AFTER_BACK", 1)

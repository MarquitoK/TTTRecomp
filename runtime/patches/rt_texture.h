#pragma once
#include "patches/registry.h"

// Render targets read at scale > 1 are not rebuilt as cached textures (one-time battle stall)
PATCH(rt_texture, "TTT_RT_TEX", 1)

#pragma once
#include "patches/registry.h"

// first-use hitches: texture mipmaps and filter on a worker, least recently used texture cache, sounds read ahead
PATCH(tex_async, "TTT_TEX_ASYNC", 1)
PATCH(tex_lru, "TTT_TEX_LRU", 1)
PATCH(sfx_prefetch, "TTT_SFX_PREFETCH", 1)
PATCH(mod_cache, "TTT_MOD_CACHE", 1)
PATCH(lip_early, "TTT_LIP_EARLY", 1)
PATCH(hold_battle, "TTT_HOLD_BATTLE", 1)
PATCH(sel_wait, "TTT_SEL_WAIT", 1)

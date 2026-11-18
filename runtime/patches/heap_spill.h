#pragma once
#include "patches/registry.h"

// the game's allocation gate 0x089BFD60 never returns null: what the game heap cannot hold goes to port memory
PATCH(heap_spill, "TTT_HEAP_SPILL", 1)

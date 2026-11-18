#pragma once
#include "patches/registry.h"

// object bone-matrix blocks (0x0880E0C8 size 0x7700) hold tables 0x3B80 apart: bone id 118 and up wrote past the block into the next heap chunk
PATCH(bone_room, "TTT_BONE_ROOM", 1)

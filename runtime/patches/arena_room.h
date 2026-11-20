#pragma once
#include "patches/registry.h"

// the battle arenas (0x088FE410) bump without a limit check: what an arena cannot hold comes from port memory instead of running into the next arena
PATCH(arena_room, "TTT_ARENA_ROOM", 1)

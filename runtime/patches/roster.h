#pragma once
#include "patches/registry.h"

// item 29.3: roster tables in port memory, cells from mods/<id>/Roster/roster.txt (0: the original roster, no mod layouts)
PATCH(roster, "TTT_ROSTER", 1)

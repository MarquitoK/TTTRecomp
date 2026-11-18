#pragma once
#include "patches/registry.h"

// item 65: a BT3 stage file (PCK2 with its BMA3) in Maps/ is read in place and served as TTT's map pak (0 off)
PATCH(bt3_maps, "TTT_BT3_MAPS", 1)

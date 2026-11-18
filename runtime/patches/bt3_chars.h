#pragma once
#include "patches/registry.h"

// item 29: a BT3 P file inside a colour's 1_p is read in place (1 model, hurtbox, textures and params; 2 without params; 0 off)
PATCH(bt3_chars, "TTT_BT3_CHARS", 1)

#pragma once
#include "patches/registry.h"

// item 28: a character texture container may carry one texture per model material (mNN.png), seen only by the port
PATCH(char_textures, "TTT_CHAR_TEXTURES", 1)

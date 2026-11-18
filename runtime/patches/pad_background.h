#pragma once
#include "patches/registry.h"

// pads read without window focus; with a pad per instance each instance reads its own
PATCH(pad_background, "TTT_PAD_BG", 1)

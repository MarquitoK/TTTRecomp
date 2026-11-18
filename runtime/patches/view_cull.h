#pragma once
#include "patches/registry.h"

// 3D primitives outside the view skipped before transforming: 0 off, 1 in split views, 2 always
PATCH(view_cull, "TTT_VIEW_CULL", 1)

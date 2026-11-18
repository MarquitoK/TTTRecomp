#pragma once
#include "patches/registry.h"

// Options audio page: own voices volume (categories 1 and 2), effects only on SFX, rows 3 and 4 of the page
PATCH(audio_menu, "TTT_AUDIO_MENU", 1)
// voice language row of the audio page, switched on by a voice pack mod (parches=voice_lang)
PATCH_MOD(voice_lang, "TTT_VOICE_LANG", 0)

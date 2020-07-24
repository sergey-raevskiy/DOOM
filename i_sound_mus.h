#pragma once

#include "doomtype.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

typedef void (* MUSCALLBACK)(MIDIEVENT *ev, void *ctx);

boolean I_MusRunMidiEvents(const byte *musBuf,
                           MUSCALLBACK callback,
                           void *context);

#ifndef VIRTUAL_BUTTONS_H
#define VIRTUAL_BUTTONS_H

#ifndef SDL_Rect
typedef struct { int x, y, w, h; } SDL_Rect;
#define SDL_Rect SDL_Rect
#endif
#include <mgba/internal/gba/input.h>

typedef struct {
    SDL_Rect rect;
    const char* label;
    int gbaKey;
} VirtualButton;

static const VirtualButton gbaButtons[] = {
    // D-Pad
    { {20, 300, 60, 60}, "UP",     GBA_KEY_UP    },
    { {20, 420, 60, 60}, "DOWN",   GBA_KEY_DOWN  },
    { {20, 360, 60, 60}, "LEFT",   GBA_KEY_LEFT  },
    { {80, 360, 60, 60}, "RIGHT",  GBA_KEY_RIGHT },
    // Action buttons
    { {400, 360, 60, 60}, "A",     GBA_KEY_A     },
    { {340, 420, 60, 60}, "B",     GBA_KEY_B     },
    // System
    { {160, 480, 80, 40}, "START", GBA_KEY_START },
    { {260, 480, 80, 40}, "SEL",   GBA_KEY_SELECT},
    // Shoulders
    { {20,  200, 80, 35}, "L",     GBA_KEY_L     },
    { {380, 200, 80, 35}, "R",     GBA_KEY_R     },
};

#endif

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#include "raylib.h"
#include <cstdint>

#include "utils.hpp"

namespace cellulon::constants {
    inline constexpr Color BACKGROUND_COLOR = Color {
        .r=0x22,
        .g=0x22,
        .b=0x22,
        .a=0xff
    };

    inline constexpr Color GRID_LINE_COLOR = Color {
        .r=0x50,
        .g=0x50,
        .b=0x50,
        .a=0xff
    };

    inline constexpr Color CELL_COLOR = Color {
        .r=181,
        .g=214,
        .b=184,
        .a=0xff,
    };
    inline constexpr u32 TARGET_FPS = 30;

    inline constexpr u16 CELL_SIZE = 25;
    inline constexpr u16 MARGIN_SIZE = 2;


    //Simulation parameters
    inline constexpr u32 INITIAL_CELL_COUNT = 100;
};

#endif
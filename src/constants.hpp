#include "raylib.h"
#include <cstdint>

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
    inline constexpr uint32_t TARGET_FPS = 30;

    inline constexpr uint16_t CELL_SIZE = 25;
    inline constexpr uint16_t MARGIN_SIZE = 2;
};
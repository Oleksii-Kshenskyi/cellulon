#ifndef _SIMULATION_HPP_
#define _SIMULATION_HPP_

#include "utils.hpp"
// NOTE: [[!!]] Using EnTT is fine here if absolutely necessary, but
//              !!UNDER ANY CIRCUMSTANCES, NEVER INCLUDE RAYLIB HERE!!
// This is because simulation has to be able to run headless, therefore
// it has to be completely decoupled from graphics.

namespace cellulon::simulation {
    struct XY {
        i32 x;
        i32 y;
    };

    struct Cell {};

    //TODO: in order to spawn an agent at a certain position, first I need to pick a random position.
    //TODO: Implement GameRNG struct with incapsulated RNG implementation for Cellulon.
    //TODO: Implement generic translation of grid coords into world coords (pixel x, y).
    //TODO: implement proper Grid/Board class [rows/cols, cell_to_pixel(), cell_center(), in_bounds()].
}

#endif
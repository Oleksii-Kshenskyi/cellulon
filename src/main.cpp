#include <format>

#include <cstdlib>

#include <entt/entt.hpp>
#include <raylib.h>

#include "constants.hpp"

namespace cnst = cellulon::constants;

template<typename T, typename U>
constexpr T as(U value) { return static_cast<T>(value); }

void draw_grid_lines() {
    const auto [screen_width, screen_height] = std::make_tuple(GetScreenWidth(), GetScreenHeight());
    const auto [x_cells, x_free_real_estate] = std::div(screen_width, cnst::CELL_SIZE);
    const auto [y_cells, y_free_real_estate] = std::div(screen_height, cnst::CELL_SIZE);

    auto half_x_margin = x_free_real_estate / 2;
    auto half_y_margin = y_free_real_estate / 2;
    auto x_pos = as<float>(half_x_margin);
    for(int x_cell_no = 0; x_cell_no <= x_cells; x_cell_no++) {
        Vector2 from {.x = x_pos, .y = as<float>(half_y_margin)};
        Vector2 to {.x = x_pos, .y = as<float>(screen_height - half_y_margin)};
        DrawLineEx(from, to, cnst::MARGIN_SIZE, cnst::GRID_LINE_COLOR);
        x_pos += cnst::CELL_SIZE;
    }

    auto y_pos = as<float>(half_y_margin);
    for(int y_cell_no = 0; y_cell_no <= y_cells; y_cell_no++) {
        Vector2 from {.x = as<float>(x_free_real_estate / 2), .y = y_pos};
        Vector2 to {.x = as<float>(screen_width - half_x_margin), .y = y_pos};
        DrawLineEx(from, to, cnst::MARGIN_SIZE, cnst::GRID_LINE_COLOR);
        y_pos += cnst::CELL_SIZE;
    }

    DrawText(std::format("CELL COUNT: {}", x_cells * y_cells).c_str(), 10, 10, 20, WHITE);
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
    InitWindow(0, 0, "Cellulon");
    ClearWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(cnst::TARGET_FPS);

    entt::registry registry;

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(cnst::BACKGROUND_COLOR);

            draw_grid_lines();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
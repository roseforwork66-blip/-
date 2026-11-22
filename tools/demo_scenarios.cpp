#include "demo_scenarios.h"

namespace cane::demo {
namespace {

void openSpace(Grid& grid) {
    grid.clear();
    for (int row = -59; row < 59; ++row) {
        for (int col = -59; col < 59; ++col) {
            grid.set({(col + 0.5f) * Grid::resolution, (row + 0.5f) * Grid::resolution}, Cell::Free);
        }
    }
}

void wall(Grid& grid, float x, int first, int last) {
    for (int row = first; row <= last; ++row) {
        grid.set({x, row * Grid::resolution}, Cell::Occupied);
    }
}

}  // namespace

std::vector<Frame> run(Scenario scenario) {
    Controller controller;
    Grid grid;
    openSpace(grid);
    if (scenario == Scenario::Left || scenario == Scenario::Right) {
        wall(grid, 1.78f, -4, 4);
        if (scenario == Scenario::Left) {
            wall(grid, 1.48f, -25, -13);
        } else {
            wall(grid, 1.48f, 13, 25);
        }
    } else if (scenario == Scenario::Blocked) {
        wall(grid, 1.62f, -30, 30);
    }

    Input input{100, false, true, true, 25, 100, 0, 0, 0, 0};
    std::vector<Frame> frames{{input.nowMs, controller.update(input, grid)}};
    input.nowMs += 50;
    input.held = true;
    Output output = controller.update(input, grid);
    frames.push_back({input.nowMs, output});

    if (scenario == Scenario::ScanLoss) {
        for (input.nowMs += 100; input.nowMs <= 450; input.nowMs += 100) {
            output = controller.update(input, grid);
        }
        frames.push_back({input.nowMs - 100, output});
    }
    if (output.state == State::Sidestep) {
        // Subsequent synthetic scans describe open space, with an ideal steering response.
        openSpace(grid);
        for (int tick = 0; tick < 300 && output.steeringEnabled; ++tick) {
            input.nowMs += 50;
            input.scanStartMs = input.nowMs - 100;
            input.scanEndMs = input.nowMs;
            input.speedMps = 0.25f;
            input.travelM = 0.0125f;
            input.wheelDeg = output.steeringDeg;
            const State previous = output.state;
            output = controller.update(input, grid);
            if (output.state != previous) {
                frames.push_back({input.nowMs, output});
                if (output.state == State::Straight) {
                    break;
                }
            }
        }
    }
    return frames;
}

}  // namespace cane::demo

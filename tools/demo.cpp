#include "control.h"

#include <iostream>
#include <string>

using namespace cane;

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

void print(uint32_t time, const Output& output) {
    std::cout << time << " ms  " << name(output.state) << "  " << name(output.stop)
              << "  servo=" << (output.steeringEnabled ? "on" : "off")
              << "  command=" << output.steeringDeg << " deg\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "left";
    if (scenario != "straight" && scenario != "left" && scenario != "right" &&
        scenario != "blocked" && scenario != "scan-loss") {
        std::cerr << "Usage: cane_demo [straight|left|right|blocked|scan-loss]\n";
        return 2;
    }
    Controller controller;
    Grid grid;
    openSpace(grid);
    if (scenario == "left" || scenario == "right") {
        wall(grid, 1.78f, -4, 4);
        if (scenario == "left") {
            wall(grid, 1.48f, -25, -13);
        } else {
            wall(grid, 1.48f, 13, 25);
        }
    } else if (scenario == "blocked") {
        wall(grid, 1.62f, -30, 30);
    }

    Input input{100, false, true, true, 25, 100, 0, 0, 0, 0};
    print(input.nowMs, controller.update(input, grid));
    input.nowMs += 50;
    input.held = true;
    Output output = controller.update(input, grid);
    print(input.nowMs, output);
    if (scenario == "scan-loss") {
        input.nowMs += 100;
        while (input.nowMs <= 450) {
            output = controller.update(input, grid);
            input.nowMs += 100;
        }
        print(input.nowMs - 100, output);
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
                print(input.nowMs, output);
                if (output.state == State::Straight) {
                    break;
                }
            }
        }
    }
}

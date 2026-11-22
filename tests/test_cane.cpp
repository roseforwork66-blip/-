#include "control.h"
#include "lidar.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cane;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void fillFree(Grid& grid) {
    grid.clear();
    for (int row = -59; row < 59; ++row) {
        for (int col = -59; col < 59; ++col) {
            grid.set({(col + 0.5f) * Grid::resolution, (row + 0.5f) * Grid::resolution}, Cell::Free);
        }
    }
}

Input tickInput(uint32_t now, bool held) {
    return {now, held, true, true, now - 100, now - 20, 0, 0, 0, 0};
}

bool sendBeam(Lidar& lidar, unsigned angle, unsigned distanceQ2, bool start, uint32_t now, unsigned quality = 15) {
    const unsigned angleBits = (angle * 64 << 1) | 1;
    const std::array<unsigned, 5> bytes{(quality << 2) | (start ? 1U : 2U), angleBits & 255,
                                        angleBits >> 8, distanceQ2 & 255, distanceQ2 >> 8};
    bool complete = false;
    for (const unsigned byte : bytes) {
        complete |= lidar.push(static_cast<uint8_t>(byte), now);
    }
    return complete;
}

void testLidar() {
    Lidar lidar;
    constexpr std::array<uint8_t, 9> descriptor{0x99, 0xA5, 0xA5, 0x5A, 5, 0, 0, 0x40, 0x81};
    for (const uint8_t byte : descriptor) {
        expect(!lidar.push(byte, 0), "descriptor may not publish a scan");
    }
    expect(lidar.descriptorSeen(), "standard scan descriptor was rejected");
    for (unsigned angle = 0; angle < 360; ++angle) {
        expect(!sendBeam(lidar, angle, 8000, angle == 0, 50 + angle / 2, angle == 180 ? 0 : 15),
               "partial scan published");
    }
    expect(sendBeam(lidar, 0, 8000, true, 230), "complete scan did not publish");
    expect(lidar.latest().returns == 359 && lidar.latest().startMs == 50 && lidar.latest().endMs == 230,
           "scan timing or invalid return handling was wrong");

    Grid grid;
    projectScan(lidar.latest(), {}, 0, -1, grid);
    expect(grid.at({1.0f, 0.0f}) == Cell::Free, "beam traversal was not marked free");
    expect(grid.at({2.0f, 0.0f}) == Cell::Occupied, "beam endpoint was not marked occupied");
    expect(grid.at({2.5f, 0.0f}) == Cell::Unknown, "region behind a beam endpoint became free");
    Scan single;
    single.beams[0] = {90, 1};
    projectScan(single, {0.13f, 0.025f}, 0, -1, grid);
    expect(grid.at({0.13f, -0.975f}) == Cell::Occupied, "scan orientation or lidar offset was wrong");
    expect(grid.at({0.13f, 0.975f}) == Cell::Unknown, "unknown return was treated as clear");

    for (unsigned i = 0; i < 7; ++i) {
        lidar.push(0, 240);
    }
    for (unsigned angle = 20; angle < 360; ++angle) {
        sendBeam(lidar, angle, 8000, false, 230 + angle / 2);
    }
    expect(!sendBeam(lidar, 0, 8000, true, 410) && lidar.latest().endMs == 230 && lidar.badNodes() > 0,
           "a damaged revolution replaced the last good scan");
    for (unsigned angle = 1; angle < 360; ++angle) {
        sendBeam(lidar, angle, 8000, false, 410 + angle / 2);
    }
    expect(sendBeam(lidar, 0, 8000, true, 590), "parser failed to recover after a damaged sweep");
}

void testStops() {
    Grid grid;
    fillFree(grid);
    Controller controller;
    expect(!controller.update(tickInput(100, true), grid).steeringEnabled,
           "a held button at boot must not arm the controller");
    controller.update(tickInput(125, false), grid);
    const Output armed = controller.update(tickInput(150, true), grid);
    expect(armed.steeringEnabled && armed.state == State::Straight, "clear lane failed to arm");

    auto input = tickInput(175, true);
    input.scanStartMs = 0;
    input.scanEndMs = 0;
    input.scanReady = false;
    const Output lost = controller.update(input, grid);
    expect(lost.stop == Stop::ScanStale && !lost.steeringEnabled && lost.vibration == Vibration::Continuous,
           "a missing scan must disable the servo and vibrate");
    expect(!controller.update(tickInput(200, true), grid).steeringEnabled,
           "controller rearmed while button stayed held");
    input = tickInput(225, false);
    input.speedMps = 0.10f;
    expect(controller.update(input, grid).state == State::Stopped, "moving release cleared the stop latch");
    controller.update(tickInput(250, false), grid);
    expect(controller.update(tickInput(275, true), grid).steeringEnabled, "stationary rearm failed");

    input = tickInput(300, true);
    input.speedMps = 0.41f;
    expect(controller.update(input, grid).stop == Stop::Speed, "overspeed was not detected");
    controller.update(tickInput(325, false), grid);
    controller.update(tickInput(350, true), grid);
    input = tickInput(375, true);
    input.wheelDeg = std::numeric_limits<float>::quiet_NaN();
    expect(controller.update(input, grid).stop == Stop::Sensors, "invalid feedback was accepted");

    controller.update(tickInput(400, false), grid);
    controller.update(tickInput(425, true), grid);
    expect(controller.update(tickInput(575, true), grid).stop == Stop::ControlLate, "late tick was accepted");
    controller.update(tickInput(600, false), grid);
    controller.update(tickInput(625, true), grid);
    for (uint32_t now = 650; now < 650 + 450; now += 25) {
        input = tickInput(now, true);
        input.wheelDeg = 10;
        const Output out = controller.update(input, grid);
        if (now >= 650 + 425) {
            expect(out.stop == Stop::Steering, "persistent steering error did not trip");
        }
    }

    controller.update(tickInput(1125, false), grid);
    grid.clear();
    expect(controller.update(tickInput(1150, true), grid).stop == Stop::UnknownPath,
           "an unobserved lane was treated as free");
    controller.update(tickInput(1175, false), grid);
    fillFree(grid);
    grid.set({0.60f, 0.0f}, Cell::Occupied);
    expect(controller.update(tickInput(1200, true), grid).stop == Stop::Obstacle,
           "close obstacle failed to latch a stop");
}

void testDetour(int direction) {
    Grid grid;
    fillFree(grid);
    for (int row = -4; row <= 4; ++row) {
        grid.set({1.78f, row * Grid::resolution}, Cell::Occupied);
    }
    for (int row = 13; row <= 25; ++row) {
        grid.set({1.48f, -direction * row * Grid::resolution}, Cell::Occupied);
    }
    Controller controller;
    controller.update(tickInput(100, false), grid);
    Output out = controller.update(tickInput(150, true), grid);
    expect(out.state == State::Sidestep && out.steeringDeg * direction > 0 &&
               std::abs(out.steeringDeg) <= 3.251f && out.vibration == Vibration::Pulse,
           "the open side or initial command rate was wrong");

    fillFree(grid);
    bool passed = false;
    for (uint32_t now = 200; now <= 15000; now += 50) {
        Input input = tickInput(now, true);
        input.wheelDeg = out.steeringDeg;
        input.speedMps = 0.25f;
        input.travelM = 0.0125f;
        out = controller.update(input, grid);
        expect(out.steeringEnabled, "detour stopped in confirmed open space");
        passed |= out.state == State::Pass;
        if (out.state == State::Straight) {
            expect(passed && std::abs(out.steeringDeg) <= 5, "detour exited without passing or straightening");
            return;
        }
    }
    expect(false, "detour did not complete");
}

void testBothSidesBlocked() {
    Grid grid;
    fillFree(grid);
    for (int row = -30; row <= 30; ++row) {
        grid.set({1.62f, row * Grid::resolution}, Cell::Occupied);
    }
    Controller controller;
    controller.update(tickInput(100, false), grid);
    const Output out = controller.update(tickInput(150, true), grid);
    expect(out.stop == Stop::Obstacle && !out.steeringEnabled, "blocked detour should stop");
}

}  // namespace

int main() {
    testLidar();
    testStops();
    testDetour(1);
    testDetour(-1);
    testBothSidesBlocked();
    std::cout << "Guide cane checks passed\n";
}

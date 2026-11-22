#pragma once

#include "control.h"

namespace cane {

struct Beam {
    float angleDeg = 0;
    float rangeM = 0;
};

struct Scan {
    static constexpr unsigned binCount = 720;
    std::array<Beam, binCount> beams{};
    uint32_t startMs = 0;
    uint32_t endMs = 0;
    unsigned returns = 0;
};

class Lidar {
public:
    // Consumes the SCAN response descriptor followed by standard 5-byte nodes.
    bool push(uint8_t byte, uint32_t nowMs);
    const Scan& latest() const { return latest_; }
    bool descriptorSeen() const { return descriptorSeen_; }
    unsigned badNodes() const { return badNodes_; }

private:
    bool measurement(uint32_t nowMs);
    std::array<uint8_t, 7> bytes_{};
    unsigned used_ = 0;
    unsigned badNodes_ = 0;
    bool descriptorSeen_ = false;
    bool synchronized_ = false;
    float lastAngle_ = 0;
    float sweptDeg_ = 0;
    Scan pending_{};
    Scan latest_{};
};

void projectScan(const Scan& scan, Point origin, float zeroDeg, int direction, Grid& grid);

}  // namespace cane

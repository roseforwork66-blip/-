#pragma once

#include "control.h"

#include <vector>

namespace cane::demo {

enum class Scenario { Straight, Left, Right, Blocked, ScanLoss };

struct Frame {
    uint32_t milliseconds;
    Output output;
};

std::vector<Frame> run(Scenario scenario);

}  // namespace cane::demo

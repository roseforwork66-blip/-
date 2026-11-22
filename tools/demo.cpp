#include "demo_scenarios.h"

#include <iostream>
#include <string_view>
#include <utility>

int main(int argc, char** argv) {
    using namespace cane::demo;
    constexpr std::pair<std::string_view, Scenario> choices[] = {
        {"straight", Scenario::Straight}, {"left", Scenario::Left}, {"right", Scenario::Right},
        {"blocked", Scenario::Blocked}, {"scan-loss", Scenario::ScanLoss},
    };
    const std::string_view requested = argc > 1 ? argv[1] : "left";
    for (const auto& [label, scenario] : choices) {
        if (requested != label) {
            continue;
        }
        for (const auto& [time, output] : run(scenario)) {
            std::cout << time << " ms  " << cane::name(output.state) << "  " << cane::name(output.stop)
                      << "  servo=" << (output.steeringEnabled ? "on" : "off")
                      << "  command=" << output.steeringDeg << " deg\n";
        }
        return 0;
    }
    std::cerr << "Usage: cane_demo [straight|left|right|blocked|scan-loss]\n";
    return 2;
}

#include "lidar.h"

#include <cmath>

namespace cane {

bool Lidar::push(uint8_t byte, uint32_t nowMs) {
    if (!descriptorSeen_) {
        constexpr std::array<uint8_t, 7> descriptor{0xA5, 0x5A, 5, 0, 0, 0x40, 0x81};
        if (byte != descriptor[used_]) {
            used_ = byte == descriptor[0] ? 1 : 0;
            return false;
        }
        if (++used_ == descriptor.size()) {
            descriptorSeen_ = true;
            used_ = 0;
        }
        return false;
    }
    bytes_[used_++] = byte;
    if (used_ < 5) {
        return false;
    }

    const bool start = (bytes_[0] & 1U) != 0;
    const bool inverse = (bytes_[0] & 2U) != 0;
    const unsigned angleQ6 = (bytes_[1] >> 1) | (bytes_[2] << 7);
    if (start == inverse || (bytes_[1] & 1U) == 0 || angleQ6 >= 360 * 64) {
        synchronized_ = false;
        ++badNodes_;
        for (unsigned i = 0; i < 4; ++i) {
            bytes_[i] = bytes_[i + 1];
        }
        used_ = 4;
        return false;
    }
    used_ = 0;
    return measurement(nowMs);
}

bool Lidar::measurement(uint32_t nowMs) {
    const bool start = (bytes_[0] & 1U) != 0;
    const unsigned quality = bytes_[0] >> 2;
    const float angle = ((bytes_[1] >> 1) | (bytes_[2] << 7)) / 64.0f;
    const float distance = (bytes_[3] | (bytes_[4] << 8)) / 4000.0f;
    float delta = angle - lastAngle_;
    if (delta < -180) {
        delta += 360;
    }
    lastAngle_ = angle;
    if (delta < 0 || delta > 15) {
        synchronized_ = false;
    }

    bool completed = false;
    if (start) {
        if (synchronized_ && sweptDeg_ + delta >= 340 && sweptDeg_ + delta <= 375 &&
            pending_.returns >= 120 && nowMs - pending_.startMs <= 300) {
            pending_.endMs = nowMs;
            latest_ = pending_;
            completed = true;
        }
        pending_ = {};
        pending_.startMs = nowMs;
        sweptDeg_ = 0;
        synchronized_ = true;
    } else if (synchronized_) {
        sweptDeg_ += delta;
    }
    if (synchronized_ && quality > 0 && distance >= 0.15f && distance <= 8.0f) {
        Beam& bin = pending_.beams[static_cast<unsigned>(angle * 2)];
        if (bin.rangeM == 0) {
            ++pending_.returns;
        }
        if (bin.rangeM == 0 || distance < bin.rangeM) {
            bin = {angle, distance};
        }
    }
    return completed;
}

void projectScan(const Scan& scan, Point origin, float zeroDeg, int direction, Grid& grid) {
    grid.clear();
    for (const Beam& beam : scan.beams) {
        if (beam.rangeM < 0.15f || beam.rangeM > 8.0f) {
            continue;
        }
        const float heading = degreesToRadians(direction * (beam.angleDeg - zeroDeg));
        grid.ray(origin, {origin.x + std::cos(heading) * beam.rangeM,
                          origin.y + std::sin(heading) * beam.rangeM});
    }
}

}  // namespace cane

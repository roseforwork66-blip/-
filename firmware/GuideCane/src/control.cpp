#include "control.h"

#include <algorithm>
#include <cmath>

namespace cane {
namespace {

Point advance(Point p, float angleDeg, float distanceM) {
    const float angle = radians(angleDeg);
    return {p.x + std::cos(angle) * distanceM, p.y + std::sin(angle) * distanceM};
}

}  // namespace

int Grid::index(Point p) {
    constexpr float half = side * resolution / 2;
    if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
        p.x < -half || p.x >= half || p.y < -half || p.y >= half) {
        return -1;
    }
    const int col = static_cast<int>(std::floor(p.x / resolution)) + side / 2;
    const int row = static_cast<int>(std::floor(p.y / resolution)) + side / 2;
    return col >= 0 && col < side && row >= 0 && row < side ? row * side + col : -1;
}

void Grid::clear() { cells_.fill(Cell::Unknown); }

void Grid::set(Point point, Cell value) {
    const int i = index(point);
    if (i >= 0 && cells_[i] != Cell::Occupied) {
        cells_[i] = value;
    }
}

Cell Grid::at(Point point) const {
    const int i = index(point);
    return i >= 0 ? cells_[i] : Cell::Unknown;
}

void Grid::ray(Point origin, Point hit) {
    const float dx = hit.x - origin.x;
    const float dy = hit.y - origin.y;
    const float length = std::hypot(dx, dy);
    if (!std::isfinite(length) || length < 0.15f || length > 8.0f) {
        return;
    }
    for (float d = 0; d < length; d += resolution / 2) {
        set({origin.x + dx * d / length, origin.y + dy * d / length}, Cell::Free);
    }
    set(hit, Cell::Occupied);
}

Controller::Lane Controller::checkLeg(const Grid& grid, Point start, float headingDeg,
                                      float lengthM) const {
    Lane lane{false, false, lengthM};
    const float angle = radians(headingDeg);
    const float forwardX = std::cos(angle);
    const float forwardY = std::sin(angle);
    constexpr float sampleSpacing = Grid::resolution / 2;
    const int stations = std::max(1, static_cast<int>(std::ceil(lengthM / sampleSpacing)));
    const int width = std::max(1, static_cast<int>(std::ceil(config_.halfWidthM * 2 / sampleSpacing)));
    for (int i = 0; i <= stations; ++i) {
        const float distance = lengthM * i / stations;
        const Point center{start.x + forwardX * distance, start.y + forwardY * distance};
        for (int j = 0; j <= width; ++j) {
            const float side = config_.halfWidthM * (2.0f * j / width - 1.0f);
            const Point p{center.x - forwardY * side, center.y + forwardX * side};
            const Cell cell = grid.at(p);
            if (cell == Cell::Occupied) {
                lane.occupied = true;
                lane.firstHitM = std::min(lane.firstHitM, distance);
            }
            // The operator clears the immediate footprint before arming. Hits still count here.
            const bool nearBase = p.x >= -0.10f && p.x <= 0.25f && std::abs(p.y) <= config_.halfWidthM;
            lane.unknown |= cell == Cell::Unknown && !nearBase;
        }
    }
    return lane;
}

Stop Controller::checkTransition(const Grid& grid, float fromDeg, float toDeg) const {
    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(toDeg - fromDeg) / 7.5f)));
    for (int i = 0; i <= steps; ++i) {
        const Lane lane = checkLeg(grid, {}, fromDeg + (toDeg - fromDeg) * i / steps, config_.stopM);
        if (lane.occupied || lane.unknown) {
            return lane.occupied ? Stop::Obstacle : Stop::UnknownPath;
        }
    }
    return Stop::None;
}

float Controller::chooseTurn(const Grid& grid, float obstacleM) const {
    for (const float magnitude : {15.0f, 25.0f, 35.0f}) {
        if (magnitude > config_.steeringLimitDeg) {
            continue;
        }
        const float length = config_.sideShiftM / std::sin(radians(magnitude));
        for (const float angle : {magnitude, -magnitude}) {
            const Point corner = advance({}, angle, length);
            if (corner.x >= obstacleM) {
                continue;
            }
            const Lane diagonal = checkLeg(grid, {}, angle, length);
            const Lane parallel = checkLeg(grid, corner, 0, config_.stopM);
            if (!diagonal.occupied && !diagonal.unknown && !parallel.occupied && !parallel.unknown) {
                return angle;
            }
        }
    }
    return 0;
}

Output Controller::halt(Stop reason) {
    state_ = State::Stopped;
    stop_ = reason;
    mismatchMs_ = 0;
    commandDeg_ = 0;
    return {state_, stop_, Vibration::Continuous, false, 0};
}

Output Controller::drive(float target, float dt, Vibration vibration) {
    target = std::clamp(target, -config_.steeringLimitDeg, config_.steeringLimitDeg);
    const float step = config_.slewDegPerSec * dt;
    commandDeg_ += std::clamp(target - commandDeg_, -step, step);
    return {state_, Stop::None, vibration, true, commandDeg_};
}

Output Controller::update(const Input& in, const Grid& grid) {
    const uint32_t elapsed = tickSeen_ ? in.nowMs - tickMs_ : 0;
    tickSeen_ = true;
    tickMs_ = in.nowMs;
    const bool fresh = in.scanReady && in.nowMs - in.scanEndMs <= config_.scanTimeoutMs &&
                       in.nowMs - in.scanStartMs <= config_.oldestBeamMs &&
                       in.scanEndMs - in.scanStartMs <= config_.scanTimeoutMs;
    const bool sensors = in.sensorsOk && std::isfinite(in.speedMps) && std::isfinite(in.travelM) &&
                         std::isfinite(in.wheelDeg) && std::isfinite(in.yawDeltaDeg);
    const bool still = sensors && std::abs(in.speedMps) <= config_.stationaryMps &&
                       std::abs(in.travelM) < 0.002f;

    if (!in.held) {
        if (state_ == State::Stopped && (!still || !fresh)) {
            return halt(stop_);
        }
        released_ = true;
        state_ = State::Idle;
        stop_ = Stop::None;
        commandDeg_ = 0;
        mismatchMs_ = 0;
        return {};
    }
    if (state_ == State::Stopped) {
        return halt(stop_);
    }
    if (state_ == State::Idle && !released_) {
        return {};
    }
    if (!sensors) {
        return halt(Stop::Sensors);
    }
    if (elapsed > config_.maxTickMs) {
        return halt(Stop::ControlLate);
    }
    if (!fresh) {
        return halt(Stop::ScanStale);
    }
    if (in.speedMps > config_.maxSpeedMps || in.speedMps < -config_.stationaryMps) {
        return halt(Stop::Speed);
    }
    if (std::abs(in.wheelDeg) > config_.steeringLimitDeg + 4.0f) {
        return halt(Stop::Steering);
    }
    if (state_ == State::Idle) {
        if (!still || std::abs(in.wheelDeg) > config_.mismatchDeg) {
            return halt(still ? Stop::Steering : Stop::Speed);
        }
        state_ = State::Straight;
        yawDeg_ = 0;
    }

    if (std::abs(in.wheelDeg - commandDeg_) > config_.mismatchDeg) {
        mismatchMs_ += elapsed;
        if (mismatchMs_ >= config_.mismatchMs) {
            return halt(Stop::Steering);
        }
    } else {
        mismatchMs_ = 0;
    }
    const float dt = elapsed / 1000.0f;
    const Lane stopLane = checkLeg(grid, {}, in.wheelDeg, config_.stopM);
    if (stopLane.occupied) {
        return halt(Stop::Obstacle);
    }
    if (stopLane.unknown) {
        return halt(Stop::UnknownPath);
    }

    if (state_ == State::Straight) {
        const Lane ahead = checkLeg(grid, {}, 0, config_.lookaheadM);
        if (!ahead.occupied) {
            return ahead.unknown ? halt(Stop::UnknownPath) : drive(0, dt);
        }
        detourDeg_ = chooseTurn(grid, ahead.firstHitM);
        if (detourDeg_ == 0) {
            return halt(Stop::Obstacle);
        }
        const Stop swing = checkTransition(grid, in.wheelDeg, detourDeg_);
        if (swing != Stop::None) {
            return halt(swing);
        }
        state_ = State::Sidestep;
        yawDeg_ = alongM_ = lateralM_ = 0;
        passUntilM_ = ahead.firstHitM + 1.25f;
        detourStartMs_ = in.nowMs;
        return drive(detourDeg_, dt, Vibration::Pulse);
    }

    yawDeg_ += in.yawDeltaDeg;
    const Point position = advance({alongM_, lateralM_}, yawDeg_ + in.wheelDeg, in.travelM);
    alongM_ = position.x;
    lateralM_ = position.y;
    if (in.nowMs - detourStartMs_ > 20000 || std::abs(yawDeg_) > 45.0f) {
        return halt(Stop::Timeout);
    }
    if (state_ == State::Sidestep && lateralM_ * (detourDeg_ > 0 ? 1.0f : -1.0f) >= config_.sideShiftM) {
        state_ = State::Pass;
    }
    const float target = (state_ == State::Sidestep ? detourDeg_ : 0.0f) - yawDeg_;
    if (std::abs(target) > config_.steeringLimitDeg + 2.0f) {
        return halt(Stop::Steering);
    }
    const Stop swing = checkTransition(grid, in.wheelDeg, target);
    if (swing != Stop::None) {
        return halt(swing);
    }
    if (state_ == State::Pass && alongM_ >= passUntilM_ &&
        std::abs(yawDeg_) <= 5.0f && std::abs(in.wheelDeg) <= 5.0f) {
        state_ = State::Straight;
    }
    return drive(target, dt);
}

const char* name(State state) {
    switch (state) {
        case State::Idle: return "idle";
        case State::Straight: return "straight";
        case State::Sidestep: return "sidestep";
        case State::Pass: return "pass";
        case State::Stopped: return "stopped";
    }
    return "invalid";
}

const char* name(Stop stop) {
    switch (stop) {
        case Stop::None: return "none";
        case Stop::Sensors: return "sensors";
        case Stop::ScanStale: return "scan-stale";
        case Stop::ControlLate: return "control-late";
        case Stop::Speed: return "speed";
        case Stop::Steering: return "steering";
        case Stop::Obstacle: return "obstacle";
        case Stop::UnknownPath: return "unknown-path";
        case Stop::Timeout: return "timeout";
    }
    return "invalid";
}

}  // namespace cane

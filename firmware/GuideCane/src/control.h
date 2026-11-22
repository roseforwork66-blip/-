#pragma once

#include <array>
#include <cstdint>

namespace cane {

constexpr float pi = 3.14159265358979323846f;
constexpr float radians(float degrees) { return degrees * pi / 180.0f; }
constexpr float degrees(float radiansValue) { return radiansValue * 180.0f / pi; }

struct Point {
    float x = 0;
    float y = 0;
};

enum class Cell : uint8_t { Unknown, Free, Occupied };

class Grid {
public:
    static constexpr float resolution = 0.05f;
    static constexpr int side = 120;

    void clear();
    void set(Point point, Cell value);
    Cell at(Point point) const;
    void ray(Point origin, Point hit);

private:
    static int index(Point point);
    std::array<Cell, side * side> cells_{};
};

enum class State : uint8_t { Idle, Straight, Sidestep, Pass, Stopped };
enum class Stop : uint8_t {
    None, Sensors, ScanStale, ControlLate, Speed, Steering, Obstacle, UnknownPath, Timeout
};
enum class Vibration : uint8_t { Off, Pulse, Continuous };

struct Config {
    float halfWidthM = 0.50f;
    float stopM = 0.80f;
    float lookaheadM = 1.80f;
    float sideShiftM = 0.78f;
    float maxSpeedMps = 0.40f;
    float stationaryMps = 0.035f;
    float steeringLimitDeg = 25.0f;
    float slewDegPerSec = 65.0f;
    float mismatchDeg = 8.0f;
    uint32_t mismatchMs = 420;
    uint32_t scanTimeoutMs = 300;
    uint32_t oldestBeamMs = 450;
    uint32_t maxTickMs = 125;
};

struct Input {
    uint32_t nowMs = 0;
    bool held = false;
    bool sensorsOk = false;
    bool scanReady = false;
    uint32_t scanStartMs = 0;
    uint32_t scanEndMs = 0;
    float speedMps = 0;
    float travelM = 0;
    float yawDeltaDeg = 0;
    float wheelDeg = 0;
};

struct Output {
    State state = State::Idle;
    Stop stop = Stop::None;
    Vibration vibration = Vibration::Off;
    bool steeringEnabled = false;
    float steeringDeg = 0;
};

class Controller {
public:
    explicit Controller(Config config = {}) : config_(config) {}
    Output update(const Input& input, const Grid& grid);

private:
    struct Lane {
        bool occupied = false;
        bool unknown = false;
        float firstHitM = 0;
    };

    Lane checkLeg(const Grid& grid, Point start, float headingDeg, float lengthM) const;
    Stop checkTransition(const Grid& grid, float fromDeg, float toDeg) const;
    float chooseTurn(const Grid& grid, float obstacleM) const;
    Output halt(Stop reason);
    Output drive(float target, float dt, Vibration vibration = Vibration::Off);

    Config config_;
    State state_ = State::Idle;
    Stop stop_ = Stop::None;
    bool released_ = false;
    bool tickSeen_ = false;
    uint32_t tickMs_ = 0;
    uint32_t mismatchMs_ = 0;
    uint32_t detourStartMs_ = 0;
    float commandDeg_ = 0;
    float detourDeg_ = 0;
    float yawDeg_ = 0;
    float alongM_ = 0;
    float lateralM_ = 0;
    float passUntilM_ = 0;
};

const char* name(State state);
const char* name(Stop stop);

}  // namespace cane

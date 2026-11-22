#pragma once

#include "src/control.h"

namespace board {

// Turn this on only after completing docs/bringup.md on the actual bench prototype.
constexpr bool enableSteering = false;

constexpr int holdPin = 4;          // Active low input, separate from the physical power interlock.
constexpr int powerRequestPin = 5;  // Active high input of the external power interlock.
constexpr int servoPin = 6;
constexpr int vibrationPin = 7;
constexpr int batterySensePin = 8;
constexpr int lidarRxPin = 9;
constexpr int lidarTxPin = 10;
constexpr int sdaPin = 11;
constexpr int sclPin = 12;
constexpr int leftAPin = 13;
constexpr int leftBPin = 14;
constexpr int rightAPin = 15;
constexpr int rightBPin = 16;

constexpr cane::Point lidarOrigin{0.12f, 0.0f};
constexpr float lidarForwardDeg = 0.0f;
constexpr int lidarDirection = -1;
constexpr uint8_t imuAddress = 0x68;
constexpr int gyroSign = 1;

// Example placeholders. Replace all mechanical calibration values before enabling steering.
constexpr int wheelAngleCenterRaw = 2048;
constexpr int wheelAngleSign = 1;
constexpr int leftCountSign = 1;
constexpr int rightCountSign = 1;
constexpr float leftCountsPerMeter = 400.0f;
constexpr float rightCountsPerMeter = 400.0f;
constexpr int centerPulseUs = 1500;
constexpr float leftPulseUsPerDeg = 10.0f;
constexpr float rightPulseUsPerDeg = 10.0f;
constexpr int servoSign = 1;
constexpr int minPulseUs = 1150;
constexpr int maxPulseUs = 1850;
constexpr float batteryDividerRatio = 6.1f;  // Example 51 kOhm / 10 kOhm divider.
constexpr float batteryLowV = 10.5f;         // Example threshold for a compatible 3S pack.

inline cane::Config controllerConfig() {
    return {};
}

}  // namespace board

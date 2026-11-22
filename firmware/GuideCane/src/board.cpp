#include <Arduino.h>
#include <Wire.h>

#include <algorithm>
#include <cmath>

#include "../board_config.h"
#include "lidar.h"

namespace {

constexpr uint8_t angleAddress = 0x36;
constexpr uint32_t tickPeriodMs = 25;
constexpr uint8_t servoResolutionBits = 14;
constexpr uint32_t servoFullDuty = 1U << servoResolutionBits;
HardwareSerial lidarPort(1);
cane::Lidar lidar;
cane::Grid grid;
cane::Controller controller(board::controllerConfig());
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

struct Encoder {
    uint8_t pinA;
    uint8_t pinB;
    volatile uint32_t count = 0;
    volatile uint32_t errors = 0;
    volatile uint8_t phase = 0;
};

Encoder left{board::leftAPin, board::leftBPin};
Encoder right{board::rightAPin, board::rightBPin};

struct Counts {
    uint32_t left = 0;
    uint32_t right = 0;
    uint32_t errors = 0;
};

Counts previous;
bool imuReady = false;
bool pwmReady = false;
bool scanReady = false;
float gyroBias = 0;
uint32_t lastTickMs = 0;
uint32_t lastReportMs = 0;
uint32_t vibrationUntilMs = 0;

void ARDUINO_ISR_ATTR onEncoder(void* argument) {
    Encoder& encoder = *static_cast<Encoder*>(argument);
    portENTER_CRITICAL_ISR(&encoderMux);
    const uint8_t next = (digitalRead(encoder.pinA) << 1) | digitalRead(encoder.pinB);
    const uint8_t changed = encoder.phase ^ next;
    if (changed == 3) {
        ++encoder.errors;
    } else if (changed != 0) {
        const bool forward = ((encoder.phase >> 1) ^ next) & 1;
        encoder.count += forward ? 1U : UINT32_MAX;
    }
    encoder.phase = next;
    portEXIT_CRITICAL_ISR(&encoderMux);
}

void initializeEncoder(Encoder& encoder) {
    pinMode(encoder.pinA, INPUT_PULLUP);
    pinMode(encoder.pinB, INPUT_PULLUP);
    encoder.phase = (digitalRead(encoder.pinA) << 1) | digitalRead(encoder.pinB);
    attachInterruptArg(encoder.pinA, onEncoder, &encoder, CHANGE);
    attachInterruptArg(encoder.pinB, onEncoder, &encoder, CHANGE);
}

Counts readCounts() {
    portENTER_CRITICAL(&encoderMux);
    const Counts result{left.count, right.count, left.errors + right.errors};
    portEXIT_CRITICAL(&encoderMux);
    return result;
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t size) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, size) != size) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readGyro(float& rate) {
    uint8_t data[2]{};
    if (!readRegisters(board::imuAddress, 0x2D, data, 1) || !(data[0] & 0x08) ||
        !readRegisters(board::imuAddress, 0x29, data, 2)) {
        return false;
    }
    const uint16_t bits = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    const int value = bits < 32768 ? bits : static_cast<int>(bits) - 65536;
    rate = value / 16.4f;
    return value != -32768;
}

bool initializeImu() {
    uint8_t id = 0;
    if (!readRegisters(board::imuAddress, 0x75, &id, 1) || id != 0x47 ||
        !writeRegister(board::imuAddress, 0x4F, 0x08) ||  // ±2000 °/s, 100 Hz
        !writeRegister(board::imuAddress, 0x4E, 0x0C)) {   // Gyroscope low-noise mode
        return false;
    }
    delay(70);
    const Counts start = readCounts();
    float total = 0;
    float minimum = 100;
    float maximum = -100;
    for (unsigned i = 0; i < 100; ++i) {
        delay(12);
        float sample = 0;
        if (!readGyro(sample)) {
            return false;
        }
        total += sample;
        minimum = std::min(minimum, sample);
        maximum = std::max(maximum, sample);
    }
    const Counts end = readCounts();
    gyroBias = total / 100;
    return std::abs(gyroBias) < 10 && maximum - minimum < 3 &&
           end.left == start.left && end.right == start.right && end.errors == start.errors;
}

bool readWheelAngle(float& angle, int& raw) {
    uint8_t data[3]{};
    if (!readRegisters(angleAddress, 0x0B, data, sizeof(data)) || (data[0] & 0x38) != 0x20) {
        return false;
    }
    raw = (static_cast<int>(data[1] & 0x0F) << 8) | data[2];
    const int delta = ((raw - board::wheelAngleCenterRaw + 6144) % 4096) - 2048;
    angle = board::wheelAngleSign * delta * (360.0f / 4096.0f);
    return true;
}

float travel(uint32_t count, uint32_t prior, int sign, float countsPerMeter) {
    const uint32_t delta = count - prior;
    const auto signedDelta = delta <= INT32_MAX ? static_cast<int64_t>(delta) :
                                                 static_cast<int64_t>(delta) - (INT64_C(1) << 32);
    return sign * static_cast<float>(signedDelta) / countsPerMeter;
}

void applyOutput(const cane::Output& out, uint32_t now) {
    if (board::enableSteering && out.steeringEnabled && pwmReady && digitalRead(board::holdPin) == LOW) {
        const float scale = out.steeringDeg >= 0 ? board::leftPulseUsPerDeg : board::rightPulseUsPerDeg;
        const int pulse = std::clamp(board::centerPulseUs +
                                        static_cast<int>(board::servoSign * out.steeringDeg * scale),
                                    board::minPulseUs, board::maxPulseUs);
        if (ledcWrite(board::servoPin, static_cast<uint32_t>(pulse) * servoFullDuty / 20000U)) {
            digitalWrite(board::powerRequestPin, HIGH);
        } else {
            digitalWrite(board::powerRequestPin, LOW);
            pwmReady = false;
        }
    } else {
        digitalWrite(board::powerRequestPin, LOW);
        if (pwmReady) {
            ledcWrite(board::servoPin, 0);
        }
    }
    if (out.vibration == cane::Vibration::Pulse) {
        vibrationUntilMs = now + 120;
    } else if (!out.steeringEnabled) {
        vibrationUntilMs = now;
    }
    const bool pulsing = static_cast<int32_t>(vibrationUntilMs - now) > 0;
    digitalWrite(board::vibrationPin, out.vibration == cane::Vibration::Continuous || pulsing ? HIGH : LOW);
}

}  // namespace

void setup() {
    pinMode(board::powerRequestPin, OUTPUT);
    digitalWrite(board::powerRequestPin, LOW);
    pinMode(board::vibrationPin, OUTPUT);
    digitalWrite(board::vibrationPin, LOW);
    pinMode(board::holdPin, INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(board::batterySensePin, ADC_11db);
    pwmReady = ledcAttach(board::servoPin, 50, servoResolutionBits);

    Serial.begin(115200);
    Wire.begin(board::sdaPin, board::sclPin, 100000);
    Wire.setTimeOut(5);
    initializeEncoder(left);
    initializeEncoder(right);
    lidarPort.setRxBufferSize(4096);
    lidarPort.begin(115200, SERIAL_8N1, board::lidarRxPin, board::lidarTxPin);
    imuReady = initializeImu();

    const uint8_t stop[] = {0xA5, 0x25};
    const uint8_t start[] = {0xA5, 0x20};
    lidarPort.write(stop, sizeof(stop));
    delay(100);
    while (lidarPort.available()) {
        lidarPort.read();
    }
    lidarPort.write(start, sizeof(start));
    previous = readCounts();
    lastTickMs = millis();
    vibrationUntilMs = lastTickMs;
    Serial.printf("GuideCane: steering=%s imu=%s pwm=%s\n", board::enableSteering ? "armed-capable" : "dry-run",
                  imuReady ? "ok" : "fault", pwmReady ? "ok" : "fault");
}

void loop() {
    if (digitalRead(board::holdPin) != LOW) {
        digitalWrite(board::powerRequestPin, LOW);
    }

    for (unsigned i = 0; i < 512 && lidarPort.available(); ++i) {
        if (lidar.push(static_cast<uint8_t>(lidarPort.read()), millis())) {
            cane::projectScan(lidar.latest(), board::lidarOrigin, board::lidarForwardDeg,
                              board::lidarDirection, grid);
            scanReady = true;
        }
    }

    const uint32_t now = millis();
    const uint32_t elapsed = now - lastTickMs;
    if (elapsed < tickPeriodMs) {
        delay(1);
        return;
    }
    lastTickMs = now;
    const float dt = elapsed / 1000.0f;
    const Counts counts = readCounts();
    const float leftM = travel(counts.left, previous.left, board::leftCountSign, board::leftCountsPerMeter);
    const float rightM = travel(counts.right, previous.right, board::rightCountSign, board::rightCountsPerMeter);
    const float travelM = (leftM + rightM) * 0.5f;
    const bool encodersOk = counts.errors == previous.errors && std::abs(leftM - rightM) / dt < 0.35f;
    previous = counts;

    float rate = 0;
    float angle = 0;
    int raw = -1;
    const bool gyroOk = imuReady && readGyro(rate) && std::abs(rate - gyroBias) < 150;
    const bool angleOk = readWheelAngle(angle, raw);
    const float voltage = analogReadMilliVolts(board::batterySensePin) * board::batteryDividerRatio / 1000.0f;
    const bool batteryOk = voltage >= board::batteryLowV && voltage < 13.2f;
    const bool healthy = pwmReady && encodersOk && angleOk && gyroOk && batteryOk;
    const auto& scan = lidar.latest();
    const cane::Input input{now, digitalRead(board::holdPin) == LOW, healthy, scanReady, scan.startMs,
                           scan.endMs, travelM / dt, travelM, (rate - gyroBias) * dt * board::gyroSign, angle};
    const cane::Output out = controller.update(input, grid);
    applyOutput(out, now);

    if (now - lastReportMs >= 250) {
        lastReportMs = now;
        Serial.printf("%lu,%s,%s,pts=%u,age=%lu,bad=%u,w=%d/%.1f,cmd=%.1f,L=%ld,R=%ld,v=%.2f,V=%.2f,gz=%.2f,ok=%d%d%d%d%d\n",
                      static_cast<unsigned long>(now), cane::name(out.state), cane::name(out.stop), scan.returns,
                      static_cast<unsigned long>(scanReady ? now - scan.endMs : 0), lidar.badNodes(), raw, angle,
                      out.steeringDeg, static_cast<long>(static_cast<int32_t>(counts.left)),
                      static_cast<long>(static_cast<int32_t>(counts.right)), input.speedMps, voltage, rate - gyroBias,
                      pwmReady, encodersOk, angleOk, gyroOk, batteryOk);
    }
}

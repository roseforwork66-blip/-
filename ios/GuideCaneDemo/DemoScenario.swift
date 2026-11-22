import Foundation

enum CaneState: Equatable {
    case idle, straight, sidestep, pass, stopped

    var title: String {
        switch self {
        case .idle: "待命"
        case .straight: "直行"
        case .sidestep: "侧向绕行"
        case .pass: "通过障碍"
        case .stopped: "提示停步"
        }
    }

    var symbol: String {
        switch self {
        case .idle: "pause.circle"
        case .straight: "arrow.up"
        case .sidestep: "arrow.turn.up.right"
        case .pass: "checkmark.circle"
        case .stopped: "exclamationmark.octagon"
        }
    }
}

struct DemoFrame {
    let milliseconds: Int
    let state: CaneState
    let steeringEnabled: Bool
    let steeringDegrees: Double
    var reason: String? = nil

    var steeringText: String {
        if steeringDegrees == 0 { return "0°" }
        return "\(steeringDegrees.formatted(.number.sign(strategy: .always()).precision(.fractionLength(2))))°"
    }
}

enum DemoScenario: String, CaseIterable, Identifiable {
    case straight, left, right, blocked
    case scanLoss = "scan-loss"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .straight: "前方通畅"
        case .left: "左侧可绕"
        case .right: "右侧可绕"
        case .blocked: "前方阻塞"
        case .scanLoss: "雷达超时"
        }
    }

    var symbol: String {
        switch self {
        case .straight: "arrow.up"
        case .left: "arrow.turn.up.left"
        case .right: "arrow.turn.up.right"
        case .blocked: "xmark.octagon"
        case .scanLoss: "sensor.tag.radiowaves.forward"
        }
    }

    var detail: String {
        switch self {
        case .straight: "前方路径通畅，控制程序给出直行指令。"
        case .left: "正前方遇到障碍，控制程序选择从左侧通过。"
        case .right: "正前方遇到障碍，控制程序选择从右侧通过。"
        case .blocked: "候选路线受阻，控制程序关闭舵机输出并提示停步。"
        case .scanLoss: "模拟雷达数据超时，控制程序关闭舵机输出并提示停步。"
        }
    }

    var frames: [DemoFrame] {
        let idle = DemoFrame(milliseconds: 100, state: .idle, steeringEnabled: false, steeringDegrees: 0)
        let straight = DemoFrame(milliseconds: 150, state: .straight, steeringEnabled: true, steeringDegrees: 0)
        switch self {
        case .straight:
            return [idle, straight]
        case .left, .right:
            // The positive steering command represents left in the C++ demo.
            let direction: Double = self == .left ? 1 : -1
            return [idle,
                    DemoFrame(milliseconds: 150, state: .sidestep, steeringEnabled: true, steeringDegrees: 3.25 * direction),
                    DemoFrame(milliseconds: 7_700, state: .pass, steeringEnabled: true, steeringDegrees: 21.75 * direction),
                    DemoFrame(milliseconds: 12_850, state: .straight, steeringEnabled: true, steeringDegrees: 0)]
        case .blocked:
            return [idle, DemoFrame(milliseconds: 150, state: .stopped, steeringEnabled: false,
                                    steeringDegrees: 0, reason: "检测到障碍")]
        case .scanLoss:
            return [idle, straight, DemoFrame(milliseconds: 450, state: .stopped, steeringEnabled: false,
                                              steeringDegrees: 0, reason: "雷达数据超时")]
        }
    }
}

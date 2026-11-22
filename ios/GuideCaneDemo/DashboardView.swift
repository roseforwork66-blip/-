import SwiftUI

struct DashboardView: View {
    @State private var scenario: DemoScenario = .left
    @State private var frameIndex = 0
    @State private var playing = false
    @ScaledMetric(relativeTo: .subheadline) private var scenarioWidth: CGFloat = 105

    private let accent = Color(red: 0.43, green: 0.94, blue: 0.80)
    private let background = Color(red: 0.04, green: 0.08, blue: 0.12)
    private let panel = Color(red: 0.08, green: 0.14, blue: 0.19)
    private var frames: [DemoFrame] { scenario.frames }
    private var frame: DemoFrame { frames[frameIndex] }
    private var stateColor: Color { frame.state == .stopped ? .orange : accent }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                header
                scenarioPicker
                replayPanel
                telemetry
                Link(destination: URL(string: "https://github.com/roseforwork66-blip/-/blob/44f6ed0f3c668a8c75a35ee4fd9bcabce3492421/docs/competition-test-report.md")!) {
                    HStack {
                        Label("查看原型软件测试记录", systemImage: "doc.text.magnifyingglass")
                        Spacer()
                        Image(systemName: "arrow.up.right")
                            .accessibilityHidden(true)
                    }
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(accent)
                    .padding(14)
                    .background(panel, in: RoundedRectangle(cornerRadius: 14))
                }
            }
            .frame(maxWidth: 620)
            .padding(.horizontal, 20)
            .padding(.vertical, 20)
            .frame(maxWidth: .infinity)
        }
        .background(background.ignoresSafeArea())
        .tint(accent)
        .task(id: playing) { await runPlayback() }
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("离线仿真", systemImage: "waveform.path")
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(accent)

            Text("双轮式智能导盲杖")
                .font(.largeTitle.bold())
                .accessibilityAddTraits(.isHeader)

            Text("回放五个原型测试场景，查看舵机指令和停步提示。")
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
    }

    private var scenarioPicker: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("选择场景")
                .font(.headline)
                .accessibilityAddTraits(.isHeader)

            LazyVGrid(columns: [GridItem(.adaptive(minimum: scenarioWidth), spacing: 8)], spacing: 8) {
                ForEach(DemoScenario.allCases) { option in
                    let selected = scenario == option
                    Button {
                        select(option)
                    } label: {
                        Label(option.title, systemImage: option.symbol)
                            .font(.subheadline.weight(.medium))
                            .multilineTextAlignment(.center)
                            .padding(6)
                            .frame(maxWidth: .infinity, minHeight: 44)
                            .foregroundStyle(selected ? background : .primary)
                            .background(selected ? accent : panel, in: RoundedRectangle(cornerRadius: 10))
                    }
                    .buttonStyle(.plain)
                    .accessibilityAddTraits(selected ? .isSelected : [])
                    .accessibilityIdentifier("scenario-\(option.rawValue)")
                }
            }
            Text(scenario.detail)
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
    }

    private var replayPanel: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .firstTextBaseline) {
                Text("路线示意")
                    .font(.headline)
                    .accessibilityAddTraits(.isHeader)
                Spacer()
                Text("\(frame.milliseconds) ms")
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(.secondary)
                    .accessibilityLabel("仿真时间 \(frame.milliseconds) 毫秒")
            }

            RouteDiagram(scenario: scenario, state: frame.state, accent: stateColor)
                .frame(height: 154)
                .accessibilityHidden(true)

            HStack(spacing: 12) {
                Button(action: togglePlayback) {
                    Label(playing ? "暂停" : "播放", systemImage: playing ? "pause.fill" : "play.fill")
                        .font(.subheadline.weight(.semibold))
                        .frame(maxWidth: .infinity, minHeight: 44)
                        .foregroundStyle(background)
                        .background(accent, in: RoundedRectangle(cornerRadius: 10))
                }
                .buttonStyle(.plain)
                .accessibilityIdentifier("playback")

                Button {
                    playing = false
                    frameIndex += 1
                } label: {
                    Label("下一帧", systemImage: "forward.end")
                        .font(.subheadline.weight(.semibold))
                        .padding(.horizontal, 8)
                        .frame(minHeight: 44)
                }
                .buttonStyle(.plain)
                .foregroundStyle(frameIndex < frames.count - 1 ? .primary : .secondary)
                .disabled(frameIndex == frames.count - 1)
                .accessibilityIdentifier("next-frame")

                Button(action: reset) {
                    Image(systemName: "arrow.counterclockwise")
                        .frame(width: 44, height: 44)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("重置回放")
                .accessibilityIdentifier("reset")
            }
        }
        .padding(14)
        .background(panel, in: RoundedRectangle(cornerRadius: 18))
    }

    private var telemetry: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("控制程序输出")
                .font(.headline)
                .accessibilityAddTraits(.isHeader)

            VStack(alignment: .leading, spacing: 12) {
                Label(frame.state.title, systemImage: frame.state == .sidestep ? scenario.symbol : frame.state.symbol)
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(stateColor)
                    .accessibilityLabel(frame.state.title)
                    .accessibilityIdentifier("cane-state")

                if let reason = frame.reason {
                    Text(reason).font(.subheadline)
                }

                Divider()
                metric("舵机输出", frame.steeringEnabled ? "开启" : "关闭", id: "servo-output")
                metric("模拟目标角度", frame.steeringText, id: "steering-angle")
            }
            .padding(14)
            .background(panel, in: RoundedRectangle(cornerRadius: 16))
        }
    }

    private func metric(_ name: String, _ value: String, id: String) -> some View {
        HStack {
            Text(name).foregroundStyle(.secondary)
            Spacer()
            Text(value).fontWeight(.semibold).monospacedDigit()
        }
        .font(.subheadline)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(name)
        .accessibilityValue(value)
        .accessibilityIdentifier(id)
    }

    private func select(_ option: DemoScenario) {
        if scenario == option { return }
        reset()
        scenario = option
    }

    private func reset() {
        playing = false
        frameIndex = 0
    }

    private func togglePlayback() {
        if playing {
            playing = false
        } else {
            if frameIndex == frames.count - 1 { frameIndex = 0 }
            playing = true
        }
    }

    @MainActor
    private func runPlayback() async {
        guard playing else { return }
        while frameIndex < frames.count - 1 {
            do {
                try await Task.sleep(for: .milliseconds(950))
            } catch {
                return
            }
            guard !Task.isCancelled, playing else { return }
            frameIndex += 1
        }
        playing = false
    }
}

import SwiftUI

struct RouteDiagram: View {
    let scenario: DemoScenario
    let state: CaneState
    let accent: Color

    var body: some View {
        Canvas { context, size in
            func point(_ x: Double, _ y: Double) -> CGPoint {
                CGPoint(x: size.width * CGFloat(x), y: size.height * CGFloat(y))
            }

            var grid = Path()
            for x in stride(from: 0.1, through: 0.9, by: 0.1) {
                grid.move(to: point(x, 0))
                grid.addLine(to: point(x, 1))
            }
            for y in stride(from: 0.15, to: 1, by: 0.15) {
                grid.move(to: point(0, y))
                grid.addLine(to: point(1, y))
            }
            context.stroke(grid, with: .color(.white.opacity(0.09)), lineWidth: 1)

            var route = Path()
            route.move(to: point(0.5, 0.81))
            let side: Double = scenario == .left ? 0.27 : 0.73
            switch scenario {
            case .left, .right:
                route.addQuadCurve(to: point(side, 0.49), control: point(side, 0.73))
                route.addLine(to: point(side, 0.16))
            case .straight, .scanLoss:
                route.addLine(to: point(0.5, 0.16))
            case .blocked:
                route.addLine(to: point(0.5, 0.65))
            }
            let routeActive = state != .idle && state != .stopped
            context.stroke(route, with: .color(accent.opacity(routeActive ? 0.85 : 0.45)),
                           style: StrokeStyle(lineWidth: 2.5, lineCap: .round, dash: [5, 5]))

            if scenario == .left || scenario == .right || scenario == .blocked {
                let middle = scenario == .blocked ? (0.15, 0.85) : (0.43, 0.57)
                drawObstacle(context, from: point(middle.0, 0.39), to: point(middle.1, 0.49))
                if scenario != .blocked {
                    let other = scenario == .left ? (0.67, 0.90) : (0.10, 0.33)
                    drawObstacle(context, from: point(other.0, 0.53), to: point(other.1, 0.62))
                }
            }

            let marker: CGPoint
            switch state {
            case .idle: marker = point(0.5, 0.81)
            case .straight: marker = point(scenario == .left || scenario == .right ? side : 0.5, 0.20)
            case .sidestep: marker = point(side, 0.57)
            case .pass: marker = point(side, 0.34)
            case .stopped: marker = point(0.5, 0.67)
            }
            let halo = CGRect(x: marker.x - 10, y: marker.y - 10, width: 20, height: 20)
            let dot = CGRect(x: marker.x - 4, y: marker.y - 4, width: 8, height: 8)
            context.fill(Path(ellipseIn: halo), with: .color(accent.opacity(0.2)))
            context.fill(Path(ellipseIn: dot), with: .color(accent))
        }
        .background(Color(red: 0.04, green: 0.09, blue: 0.13))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private func drawObstacle(_ context: GraphicsContext, from start: CGPoint, to end: CGPoint) {
        let area = CGRect(x: start.x, y: start.y, width: end.x - start.x, height: end.y - start.y)
        let outline = Path(roundedRect: area, cornerRadius: 4)
        context.fill(outline, with: .color(.orange.opacity(0.26)))
        context.stroke(outline, with: .color(.orange.opacity(0.85)), lineWidth: 1)
    }
}

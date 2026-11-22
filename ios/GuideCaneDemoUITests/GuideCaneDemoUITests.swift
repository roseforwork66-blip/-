import XCTest

final class GuideCaneDemoUITests: XCTestCase {
    override func setUpWithError() throws {
        continueAfterFailure = false
    }

    func testPlaybackCanStepThroughLeftDetourAndReset() {
        let app = XCUIApplication()
        app.launch()
        let next = app.buttons["next-frame"]
        let state = app.descendants(matching: .any)["cane-state"].firstMatch
        XCTAssertTrue(next.waitForExistence(timeout: 5))

        next.tap()
        XCTAssertEqual(state.label, "侧向绕行")
        next.tap()
        XCTAssertEqual(state.label, "通过障碍")
        next.tap()
        XCTAssertEqual(state.label, "直行")
        XCTAssertFalse(next.isEnabled)

        app.buttons["reset"].tap()
        XCTAssertEqual(state.label, "待命")
        XCTAssertTrue(next.isEnabled)
    }

    func testBlockedScenarioStopsAndTurnsSteeringOff() {
        let app = XCUIApplication()
        app.launch()
        let blocked = app.buttons["scenario-blocked"]
        XCTAssertTrue(blocked.waitForExistence(timeout: 5))
        blocked.tap()
        app.buttons["playback"].tap()

        XCTAssertTrue(app.staticTexts["检测到障碍"].waitForExistence(timeout: 4))
        let servoOutput = app.descendants(matching: .any)["servo-output"].firstMatch
        XCTAssertEqual(servoOutput.value as? String, "关闭")
    }
}

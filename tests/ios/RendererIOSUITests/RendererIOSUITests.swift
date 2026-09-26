import XCTest

final class VectorControlsUITests: XCTestCase {
  private let bundleID = "opengothic.gothic2.vector-qa"

  override func setUpWithError() throws {
#if !targetEnvironment(simulator)
    throw XCTSkip("This candidate is authorized for Simulator only")
#endif
    continueAfterFailure = false
    XCUIDevice.shared.orientation = .landscapeRight
  }

  private func wait(_ seconds: TimeInterval) {
    let timer = expectation(description: "game settles")
    DispatchQueue.main.asyncAfter(deadline: .now() + seconds) { timer.fulfill() }
    wait(for: [timer], timeout: seconds + 5)
  }

  private func screenshot(_ name: String, _ app: XCUIApplication) {
    XCTAssertEqual(app.state, .runningForeground)
    let attachment = XCTAttachment(screenshot: XCUIScreen.main.screenshot())
    attachment.name = name
    attachment.lifetime = .keepAlways
    add(attachment)
  }

  private func point(_ app: XCUIApplication, _ x: CGFloat, _ y: CGFloat) -> XCUICoordinate {
    app.coordinate(withNormalizedOffset: CGVector(dx: x, dy: y))
  }

  private func holdStick(_ app: XCUIApplication, x: CGFloat, y: CGFloat,
                         dx: CGFloat, dy: CGFloat, seconds: TimeInterval) {
    point(app,x,y).press(forDuration: 0.1,
                         thenDragTo: point(app,x+dx,y+dy),
                         withVelocity: .slow, thenHoldForDuration: seconds)
  }

  func testSlot1() { exercise(slot: 1) }
  func testSlot4() { exercise(slot: 4) }

  func testCameraHoldAndRelease() {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", "1"]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(30)
    screenshot("camera-00-neutral",app)
    holdStick(app,x: 0.856,y: 0.80,dx: 0.015,dy: 0.00,seconds: 1)
    wait(4)
    screenshot("camera-01-short-hold-released",app)
    holdStick(app,x: 0.856,y: 0.80,dx: 0.015,dy: 0.00,seconds: 4)
    screenshot("camera-02-long-hold-released",app)
    wait(12)
    screenshot("camera-03-neutral-again",app)
  }

  private func exercise(slot: Int) {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", String(slot)]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(40)
    screenshot("slot\(slot)-00-loaded",app)

    holdStick(app,x: 0.13,y: 0.80,dx: 0.00,dy: -0.10,seconds: 1.5)
    wait(1)
    screenshot("slot\(slot)-01-forward-released",app)
    holdStick(app,x: 0.13,y: 0.80,dx: 0.06,dy: -0.04,seconds: 1.5)
    wait(1)
    screenshot("slot\(slot)-02-diagonal-released",app)
    holdStick(app,x: 0.13,y: 0.80,dx: -0.06,dy: 0.00,seconds: 1.5)
    wait(1)
    screenshot("slot\(slot)-03-left-released",app)
    holdStick(app,x: 0.13,y: 0.80,dx: 0.00,dy: 0.10,seconds: 1.5)
    wait(1)
    screenshot("slot\(slot)-04-back-released",app)

    holdStick(app,x: 0.856,y: 0.80,dx: 0.03,dy: 0.00,seconds: 2)
    screenshot("slot\(slot)-05-camera-released",app)
    wait(3)
    screenshot("slot\(slot)-06-camera-settled",app)

    point(app,0.807,0.575).tap() // R3: directly below X, left of A
    screenshot("slot\(slot)-06-r3",app)
    point(app,0.807,0.47).tap() // iPhone 16 Pro Max: X
    wait(2)
    screenshot("slot\(slot)-07-jump",app)
    point(app,0.856,0.365).tap() // Y: classic combat locomotion
    wait(3)
    holdStick(app,x: 0.13,y: 0.80,dx: 0.04,dy: -0.07,seconds: 1)
    screenshot("slot\(slot)-08-weapon-movement",app)

    let settings = XCUIApplication(bundleIdentifier: "com.apple.Preferences")
    settings.activate()
    XCTAssertTrue(settings.wait(for: .runningForeground,timeout: 15))
    wait(2)
    app.activate()
    XCTAssertTrue(app.wait(for: .runningForeground,timeout: 20))
    wait(2)
    screenshot("slot\(slot)-09-resumed",app)
  }
}

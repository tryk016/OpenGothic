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

  func testOrientationAndResume() {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", "1"]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(35)

    for (orientation, name) in [
      (UIDeviceOrientation.landscapeLeft, "left"),
      (.portrait, "portrait-device"),
      (.landscapeRight, "right")
    ] {
      XCUIDevice.shared.orientation = orientation
      wait(2)
      screenshot("orientation-\(name)", app)
    }

    let settings = XCUIApplication(bundleIdentifier: "com.apple.Preferences")
    settings.activate()
    XCTAssertTrue(settings.wait(for: .runningForeground, timeout: 15))
    XCUIDevice.shared.orientation = .portrait
    wait(2)
    app.activate()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 20))
    wait(2)
    screenshot("orientation-resumed-portrait-device", app)
    XCUIDevice.shared.orientation = .landscapeRight
    wait(2)
    point(app,0.528,0.07).tap()
    screenshot("orientation-menu", app)
    point(app,0.902,0.90).tap()
    wait(2)
    screenshot("orientation-back-to-world", app)
  }

  func testContextualAttackA() {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", "1"]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(35)

    point(app,0.472,0.07).tap() // View: inventory.
    point(app,0.845,0.879).tap() // A toggles equipment, not attack.
    point(app,0.845,0.879).tap() // Restore the save's equipped weapon.
    screenshot("attack-00-inventory-A",app)
    point(app,0.902,0.90).tap()
    point(app,0.856,0.365).tap() // Y: draw.
    wait(3)
    screenshot("attack-01-armed",app)
    for hit in 1...3 {
      point(app,0.856,0.575).tap() // A: same attack as RT.
      screenshot("attack-02-A-\(hit)",app)
      wait(1)
    }
    point(app,0.903,0.07).tap() // RT remains an alternative.
    screenshot("attack-03-RT",app)
    wait(2)
    point(app,0.856,0.365).tap()
    wait(3)
    point(app,0.856,0.575).tap()
    screenshot("attack-04-unarmed-A",app)
    point(app,0.528,0.07).tap() // Menu: no carried combat input.
    screenshot("attack-05-menu",app)
    point(app,0.902,0.90).tap()
    wait(2)
    screenshot("attack-06-back-to-world",app)
  }

  func testTouchItemAssignment() {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", "1"]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(35)
    point(app,0.472,0.07).tap() // View: inventory
    wait(2)
    screenshot("ring-00-inventory",app)

    let edit = point(app,0.845,0.755) // R3 above inventory A
    let assign = point(app,0.097,0.79) // RT in the editor
    let clear = point(app,0.097,0.90) // LT in the editor
    let cancel = point(app,0.902,0.90) // B
    edit.tap()
    wait(3)
    screenshot("ring-01-editor-neutral",app)
    assign.tap() // No selection: must not assign or close.
    screenshot("ring-02-no-selection",app)
    cancel.tap()
    screenshot("ring-02-cancel-without-changes",app)
    edit.tap()
    point(app,0.50,0.20).press(forDuration: 0.1,
      thenDragTo: point(app,0.64,0.50),
      withVelocity: .slow, thenHoldForDuration: 1)
    wait(2)
    screenshot("ring-03-selected-after-release",app)
    assign.tap()
    wait(2)
    screenshot("ring-04-assigned-inventory",app)

    edit.tap()
    wait(1)
    point(app,0.64,0.50).tap()
    screenshot("ring-05-binding-reopened",app)
    clear.tap()
    screenshot("ring-06-binding-cleared",app)
    clear.tap() // Already empty: no other slot should change.
    cancel.tap()
    screenshot("ring-07-cancelled-inventory",app)

    edit.tap()
    point(app,0.50,0.35).tap() // Inner row, release keeps the selection.
    assign.tap()
    screenshot("ring-08-inner-assigned",app)
    cancel.tap() // Close inventory.
    point(app,0.463,0.68).tap() // D-pad Up: use item ring.
    wait(1)
    screenshot("ring-09-world-items",app)
    point(app,0.50,0.35).tap() // Use the assigned weapon via touch release.
    wait(2)
    screenshot("ring-10-used-from-world",app)
    point(app,0.463,0.68).tap()
    cancel.tap()
    screenshot("ring-11-cancelled-world",app)
  }

  func testGoldCannotBeAssigned() {
    let app = XCUIApplication(bundleIdentifier: bundleID)
    app.launchArguments = ["-nomenu", "-save", "1"]
    addTeardownBlock { app.terminate() }
    app.launch()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 30))
    wait(35)
    point(app,0.472,0.07).tap()
    point(app,0.148,0.879).tap() // Two rows down, one column right: gold in slot 1.
    point(app,0.148,0.879).tap()
    point(app,0.205,0.747).tap()
    screenshot("ring-gold-00-selected",app)
    point(app,0.845,0.755).tap() // No R3 shown; this must not open the editor.
    wait(2)
    screenshot("ring-gold-01-editor-rejected",app)
    point(app,0.902,0.90).tap()
    screenshot("ring-gold-02-back-to-world",app)
  }

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

    point(app,0.795,0.585).tap() // R3: lower-left of X, slightly below A
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

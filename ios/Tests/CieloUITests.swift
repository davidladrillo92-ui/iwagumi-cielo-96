import XCTest
import UIKit

final class CieloUITests: XCTestCase {
    private var app: XCUIApplication!
    override func setUpWithError() throws {
        continueAfterFailure = false
        XCUIDevice.shared.orientation = .portrait
        app = XCUIApplication()
        app.launchArguments = ["-AppleLanguages","(es)","-AppleLocale","es_ES"]
        app.launch()
        XCTAssertTrue(app.staticTexts["Chihiros"].firstMatch.waitForExistence(timeout:15))
    }
    private func selectTab(_ name:String) {
        let tab=app.tabBars.buttons[name].firstMatch
        if tab.waitForExistence(timeout:3) && tab.isHittable { tab.tap();return }
        // iPad may expose the native top tabs outside the tabBars subtree.
        let alternate=app.buttons[name].firstMatch
        XCTAssertTrue(alternate.waitForExistence(timeout:10));alternate.tap()
    }
    private func reveal(_ element: XCUIElement) {
        for _ in 0..<14 {
            if element.exists && element.isHittable { return }
            app.swipeUp()
        }
        XCTAssertTrue(element.exists && element.isHittable)
    }
    private func capture(_ name:String) {
        let image = XCTAttachment(screenshot:app.screenshot());image.name=name;image.lifetime = .keepAlways;add(image)
    }
    func testNavigationPreviewAndReport() {
        capture("01_Luz_demo")
        selectTab("Curvas")
        let slider=app.sliders["preview.slider"];reveal(slider)
        capture("02_Curvas_demo")
        slider.adjust(toNormalizedSliderPosition:0)
        XCTAssertEqual(app.staticTexts["preview.time"].label,"00:00")
        slider.adjust(toNormalizedSliderPosition:1)
        XCTAssertEqual(app.staticTexts["preview.time"].label,"24:00")
        selectTab("Centro")
        let report=app.buttons["report.prepare"];reveal(report);report.tap()
        XCTAssertTrue(app.buttons["report.share"].waitForExistence(timeout:10))
        capture("03_Centro_demo")
    }
    func testEditAndSaveDemo() {
        selectTab("Curvas")
        let points=app.descendants(matching:.any)["curves.points"].firstMatch;reveal(points);points.tap()
        let point=app.buttons["curve.point.1"];reveal(point);point.tap()
        let stepper=app.steppers["point.channel.1"]
        XCTAssertTrue(stepper.waitForExistence(timeout:5))
        let previousLabel=stepper.label
        let increment=stepper.buttons.matching(NSPredicate(format:"label CONTAINS[c] %@ OR label CONTAINS[c] %@","Increment","Aument")).firstMatch
        if increment.exists { increment.tap() } else { stepper.buttons.element(boundBy:1).tap() }
        XCTAssertNotEqual(stepper.label,previousLabel)
        app.buttons["point.apply"].tap()
        let save=app.buttons["curves.save"]
        for _ in 0..<6 { if save.isHittable { break };app.swipeDown() }
        XCTAssertTrue(save.isEnabled);save.tap()
        app.buttons["curves.confirm"].tap()
        XCTAssertTrue(app.alerts.buttons["Aceptar"].waitForExistence(timeout:10))
        app.alerts.buttons["Aceptar"].tap()
        XCTAssertFalse(app.staticTexts["curves.dirty"].exists)
        XCTAssertFalse(save.isEnabled)
    }
    func testMaintenanceStartAndStopDemo() {
        let start=app.buttons["maintenance.start"];reveal(start)
        XCTAssertTrue(start.isEnabled)
        XCTAssertFalse(app.buttons["maintenance.stop"].isEnabled)
        start.tap();app.buttons["maintenance.confirm"].tap()
        let stop=app.buttons["maintenance.stop"]
        let ready=NSPredicate(format:"enabled == true")
        expectation(for:ready,evaluatedWith:stop)
        waitForExpectations(timeout:10)
        capture("04_Mantenimiento_demo")
        stop.tap()
        expectation(for:ready,evaluatedWith:start)
        waitForExpectations(timeout:10)
        XCTAssertFalse(stop.isEnabled)
    }
    func testInvalidAddressStaysInSettings() {
        app.buttons["connection.open"].tap()
        let mode=app.switches["connection.demo"]
        let control=mode.switches.firstMatch
        if control.exists { control.tap() } else { mode.coordinate(withNormalizedOffset:CGVector(dx:0.92,dy:0.5)).tap() }
        let address=app.textFields["connection.host"]
        XCTAssertTrue(address.waitForExistence(timeout:5))
        expectation(for:NSPredicate(format:"enabled == true"),evaluatedWith:address)
        waitForExpectations(timeout:5);address.tap()
        let current=address.value as? String ?? ""
        address.typeText(String(repeating:XCUIKeyboardKey.delete.rawValue,count:current.count)+"8.8.8.8")
        let apply=app.buttons["connection.apply"];reveal(apply);apply.tap()
        XCTAssertTrue(app.staticTexts["connection.error"].waitForExistence(timeout:5))
        XCTAssertTrue(address.exists)
        capture("05_Conexion_direccion_rechazada")
    }
    @MainActor func testAccessibilityTextAndDescriptions() throws {
        try app.performAccessibilityAudit(for:[.dynamicType,.textClipped,.sufficientElementDescription])
        selectTab("Curvas")
        XCTAssertTrue(app.sliders["preview.slider"].waitForExistence(timeout:5))
        try app.performAccessibilityAudit(for:[.dynamicType,.textClipped,.sufficientElementDescription])
        selectTab("Centro")
        try app.performAccessibilityAudit(for:[.dynamicType,.textClipped,.sufficientElementDescription])
        capture("06_Accesibilidad_Centro")
    }
    func testLandscapeAndForegroundReturn() {
        XCUIDevice.shared.orientation = .landscapeLeft
        selectTab("Curvas")
        let slider=app.sliders["preview.slider"];reveal(slider)
        slider.adjust(toNormalizedSliderPosition:0)
        XCTAssertEqual(app.staticTexts["preview.time"].label,"00:00")
        capture("07_Curvas_horizontal")
        XCUIDevice.shared.press(.home)
        app.activate()
        XCTAssertTrue(app.sliders["preview.slider"].waitForExistence(timeout:15))
        selectTab("Luz")
        XCTAssertTrue(app.staticTexts["ATOM accesible"].waitForExistence(timeout:15))
        capture("08_Regreso_primer_plano")
        XCUIDevice.shared.orientation = .portrait
    }

}

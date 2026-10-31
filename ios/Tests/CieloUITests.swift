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
    private func tapAction(_ identifier:String) {
        let query=app.buttons.matching(identifier:identifier)
        XCTAssertTrue(query.firstMatch.waitForExistence(timeout:10))
        // Native iPad popovers can expose both the cell and its contained button.
        for i in 0..<query.count {
            let candidate=query.element(boundBy:i)
            if tappable(candidate) { candidate.tap();return }
        }
        XCTFail("No visible action for \(identifier)")
    }
    private func tappable(_ element:XCUIElement) -> Bool {
        guard element.exists else { return false }
        let frame=element.frame
        guard !frame.isEmpty && !frame.isNull && frame.origin.x.isFinite && frame.origin.y.isFinite && frame.width.isFinite && frame.height.isFinite && app.frame.intersects(frame) else { return false }
        return element.isHittable
    }
    private func scroll(towardTop:Bool = false) {
        // Stay inside content in compact landscape, clear of the floating tab bar.
        let start=app.coordinate(withNormalizedOffset:CGVector(dx:0.5,dy:towardTop ? 0.45 : 0.62))
        let end=app.coordinate(withNormalizedOffset:CGVector(dx:0.5,dy:towardTop ? 0.65 : 0.36))
        start.press(forDuration:0.05,thenDragTo:end)
    }
    private func reveal(_ element: XCUIElement, towardTop:Bool = false) {
        for _ in 0..<20 {
            if tappable(element) { return }
            scroll(towardTop:towardTop)
        }
        if !tappable(element) {
            let tree=XCTAttachment(string:app.debugDescription);tree.name="Elemento_no_visible";tree.lifetime = .keepAlways;add(tree)
            capture("Elemento_no_visible")
            XCTFail("No se pudo mostrar el control")
        }
    }
    private func capture(_ name:String) {
        let image = XCTAttachment(screenshot:app.screenshot());image.name=name;image.lifetime = .keepAlways;add(image)
    }
    func testNavigationPreviewAndReport() {
        capture("01_Luz_demo")
        selectTab("Curvas")
        let slider=app.sliders["preview.slider"];reveal(slider)
        capture("02_Curvas_demo")
        let previousTime=app.staticTexts["preview.time"].label
        slider.adjust(toNormalizedSliderPosition:0.25)
        XCTAssertNotEqual(app.staticTexts["preview.time"].label,previousTime)
        let start=app.buttons["preview.start"];reveal(start);start.tap()
        XCTAssertEqual(app.staticTexts["preview.time"].label,"00:00")
        app.buttons["preview.end"].tap()
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
        reveal(save,towardTop:true)
        XCTAssertTrue(save.isEnabled);save.tap()
        tapAction("curves.confirm")
        XCTAssertTrue(app.alerts.buttons["Aceptar"].waitForExistence(timeout:10))
        app.alerts.buttons["Aceptar"].tap()
        XCTAssertFalse(app.staticTexts["curves.dirty"].exists)
        XCTAssertFalse(save.isEnabled)
    }
    func testMaintenanceStartAndStopDemo() {
        let start=app.buttons["maintenance.start"];reveal(start)
        XCTAssertTrue(start.isEnabled)
        XCTAssertFalse(app.buttons["maintenance.stop"].isEnabled)
        start.tap();tapAction("maintenance.confirm")
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
        let target=control.exists ? control : mode
        target.coordinate(withNormalizedOffset:CGVector(dx:0.85,dy:0.5)).tap()
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
        let start=app.buttons["preview.start"];reveal(start);start.tap()
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

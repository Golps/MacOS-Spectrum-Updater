import AppKit

// Offscreen only: construct the pure view; no AppDelegate, app run loop,
// windows, device discovery, monitor reads, or monitor writes are invoked.
@main
enum GuidedUITests {
    static var checks = 0
    static func require(_ value: @autoclosure () -> Bool, _ message: String) {
        checks += 1
        if !value() { fputs("FAIL: \(message)\n", stderr); exit(1) }
    }
    static func resolved(_ color: NSColor, appearance: NSAppearance) -> NSColor {
        var result = color
        appearance.performAsCurrentDrawingAppearance { result = color.usingColorSpace(.sRGB)! }
        return result
    }
    static func luminance(_ color: NSColor) -> Double {
        func linear(_ x: CGFloat) -> Double { let v = Double(x); return v <= 0.04045 ? v/12.92 : pow((v+0.055)/1.055, 2.4) }
        return 0.2126*linear(color.redComponent)+0.7152*linear(color.greenComponent)+0.0722*linear(color.blueComponent)
    }
    static func contrast(_ first: NSColor, _ second: NSColor) -> Double {
        let a = luminance(first), b = luminance(second)
        return (max(a,b)+0.05)/(min(a,b)+0.05)
    }
    static func descendants(_ view: NSView) -> [NSView] { [view] + view.subviews.flatMap { descendants($0) } }

    static func main() throws {
        _ = NSApplication.shared
        let destination = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "build/ui-validation", isDirectory: true)
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
        for appearanceName in [NSAppearance.Name.aqua, .darkAqua] {
            let appearance = NSAppearance(named: appearanceName)!
            for width in [CGFloat(800), CGFloat(840)] {
                let host = NSView(frame: NSRect(x: 0, y: 0, width: width, height: 740))
                host.appearance = appearance
                let view = GuidedUpdaterView(frame: NSRect(x: 0, y: 0, width: width, height: 740))
                host.addSubview(view)
                require(view.steps[0].isHighlighted && view.steps.dropFirst().allSatisfy { !$0.isHighlighted }, "startup highlights only Select monitor")
                view.present(GuidedPresentation(step: 2, title: "Monitor connected", message: "Check your selected monitor and firmware, then continue.", primaryTitle: "Continue to Install", primaryEnabled: true))
                require(view.steps[1].isHighlighted && view.steps.enumerated().allSatisfy { $0.offset == 1 || !$0.element.isHighlighted }, "connection step highlights only Connect")
                NSLayoutConstraint.activate([
                    view.leadingAnchor.constraint(equalTo: host.leadingAnchor),
                    view.trailingAnchor.constraint(equalTo: host.trailingAnchor),
                    view.topAnchor.constraint(equalTo: host.topAnchor),
                    view.bottomAnchor.constraint(equalTo: host.bottomAnchor)
                ])
                view.firmwarePopup.addItems(withTitles: ["Scaler + included HDMI/bridge · V108 Beta 3", "USB hub · 06A4", "USB-C / Power Delivery · 19.02"])
                view.firmwarePopup.isHidden = false
                view.componentsLabel.stringValue = "Scaler + included HDMI/bridge: V108 Beta 3\nUSB hub: USB hub 06A4\nUSB-C / Power Delivery: USB-C / PD 19.02"
                view.firmwareName.stringValue = "Monitor version: D03.V108T"
                view.firmwareSummary.stringValue = "Vendor scaler package for ES07D03, ES07DC9 and ES07E30."
                view.setConnections(["Spectrum monitor"], selectedIndex: 0)
                require(view.connectionLabel.stringValue == "Spectrum monitor detected over USB", "connection names identify the monitor instead of the internal billboard")
                require(view.devicePopup.isHidden, "a single connection needs no selection")
                view.setConnections(["First bridge · USB 1", "Second bridge · USB 2"], selectedIndex: nil)
                require(!view.devicePopup.isHidden && view.devicePopup.indexOfSelectedItem == 0, "multiple connections require an explicit selection")
                view.setConnections(["Spectrum monitor"], selectedIndex: 0)
                view.setBackupTitles(["September 30, 2026 at 12:30 · V108 Beta 3"], canRestore: true)
                view.append("Reading 327680/1376256\nFirmware integrity checks passed.\nUSB failure: expected 32 bytes, got 16. Session aborted.\nImage: /Volumes/Fixture/Folder With Spaces/update.bin\n")
                require(!view.detailsScroll.isHidden, "diagnostics are always visible")
                require(!descendants(view).compactMap { $0 as? NSButton }.contains { $0.title == "Hide Log" || $0.title == "Show Log" }, "no log visibility controls")
                require(view.steps.allSatisfy { descendants($0).allSatisfy { !($0 is NSControl) || $0 is NSTextField } }, "steps are informational rather than buttons")
                view.present(GuidedPresentation(step: 3, title: "Ready to install Beta03", message: "Click Install to begin. A verified backup will be saved automatically. Keep monitor power and USB connected until installation finishes.", primaryTitle: "Install Beta03", primaryEnabled: true))
                require(view.primaryButton.isEnabled, "ready state enables the primary action")
                require(view.steps[2].isHighlighted && !view.steps[0].isHighlighted && !view.steps[1].isHighlighted && !view.steps[3].isHighlighted, "ready to install highlights only Install")
                host.layoutSubtreeIfNeeded()
                view.applyColors()
                host.layoutSubtreeIfNeeded()
                require(view.frame.width >= width-1, "view follows window width")
                require(view.frame.height == 740, "main page fits the fixed viewport")
                require(descendants(view).compactMap { $0 as? NSScrollView }.count == 1, "only diagnostics has a scroll view")
                require(view.modelPopup.numberOfItems == 4, "all four supported model profiles are selectable")
                require(view.modelPopup.itemTitles.allSatisfy { $0.contains("Hz") }, "model labels include refresh rate")
                require(view.steps[2].state == .current && view.steps[0].state == .complete, "step badges distinguish current and completed steps")
                for field in descendants(view).compactMap({ $0 as? NSTextField }).filter({ !$0.isHidden }) {
                    require(field.textColor != nil, "labels have an explicit foreground")
                    let frame = view.convert(field.bounds, from: field)
                    require(frame.minX >= -1 && frame.maxX <= width+1, "labels fit horizontally: \(field.stringValue)")
                    require(frame.minY >= -1 && frame.maxY <= 741, "labels fit the page vertically: \(field.stringValue)")
                    require(frame.height >= 11, "visible label is not collapsed: \(field.stringValue)")
                }
                let foreground = resolved(view.log.textColor!, appearance: appearance)
                let background = resolved(view.log.backgroundColor, appearance: appearance)
                require(contrast(foreground, background) >= 4.5, "log contrast meets 4.5:1 in \(appearanceName.rawValue)")
                let label = resolved(NSColor.labelColor, appearance: appearance)
                let card = resolved(NSColor.controlBackgroundColor, appearance: appearance)
                require(contrast(label, card) >= 4.5, "label contrast meets 4.5:1 in \(appearanceName.rawValue)")
                let attributed = view.log.textStorage!.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor
                require(attributed != nil, "existing log entries have explicit foreground attributes")
                require(contrast(resolved(attributed!, appearance: appearance), background) >= 4.5, "attributed log keeps contrast")
                require(!view.log.string.contains("/Volumes/") && !view.log.string.contains("update.bin"), "visible logs omit file paths")
                for marker in ["✓", "✕"] {
                    let range = (view.log.string as NSString).range(of: marker)
                    require(range.location != NSNotFound, "success/error entries have text indicators")
                    let color = view.log.textStorage!.attribute(.foregroundColor, at: range.location, effectiveRange: nil) as! NSColor
                    require(contrast(resolved(color, appearance: appearance), background) >= 4.5, "status log colors meet contrast in both themes")
                    require(color != NSColor.textColor, "success/error keep semantic color after appearance update")
                }
                let originalItems = view.devicePopup.itemArray
                view.setConnections(["Spectrum monitor"], selectedIndex: 0)
                require(zip(originalItems, view.devicePopup.itemArray).allSatisfy { $0 === $1 }, "unchanged discovery preserves popup items without rebuilding")
                view.present(GuidedPresentation(step: 3, title: "Ready to install Beta03", message: "Click Install to begin. A verified backup will be saved automatically. Keep monitor power and USB connected until installation finishes.", primaryTitle: "Install Beta03", primaryEnabled: true))
                require(!view.needsLayout, "identical presentation does not invalidate layout")
                let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds)!
                appearance.performAsCurrentDrawingAppearance { view.cacheDisplay(in: view.bounds, to: bitmap) }
                let name = appearanceName == .darkAqua ? "dark" : "light"
                try bitmap.representation(using: .png, properties: [:])!.write(to: destination.appendingPathComponent("\(name)-\(Int(width)).png"))
                for index in 0..<100 { view.append("Reading firmware step \(index)\n") }
                host.layoutSubtreeIfNeeded()
                view.detailsScroll.contentView.scroll(to: NSPoint(x: 0, y: 20))
                view.detailsScroll.reflectScrolledClipView(view.detailsScroll.contentView)
                let previousY = view.detailsScroll.contentView.bounds.origin.y
                view.append("A new firmware read arrived\n")
                require(abs(view.detailsScroll.contentView.bounds.origin.y - previousY) < 1, "new log lines preserve a reader's scroll position")
                view.log.scrollToEndOfDocument(nil)
                view.append("Another firmware read arrived\n")
                require(view.detailsScroll.contentView.bounds.maxY >= view.log.bounds.maxY - 30, "logs follow new entries when already at the bottom")
                view.present(GuidedPresentation(step: 3, title: "Installing D03.V108T", message: "Keep monitor power and USB connected.", activity: "Installing the selected firmware · 38% of this step", primaryTitle: "Please Wait…", primaryEnabled: true, running: true, progress: 38))
                require(!view.primaryButton.isEnabled && !view.firmwarePopup.isEnabled && !view.devicePopup.isEnabled && !view.restoreButton.isEnabled, "running state blocks write and selection actions")
                require(!view.progress.isIndeterminate && view.progress.doubleValue == 38, "phase progress is displayed")
                view.present(GuidedPresentation(step: 4, title: "D03.V108T installed", message: "Unplug the monitor’s DC power for at least 10 seconds. If Processing update appears, wait until it finishes and power-cycle again.", primaryTitle: "I’ve Finished the Power Cycle", primaryEnabled: true))
                require(view.primaryButton.isEnabled && view.progress.isHidden, "finish state gives the next action")
                for state in [
                    GuidedPresentation(step: 1, title: "Choose your firmware", message: "Select the model on your monitor’s label, then choose the original vendor ZIP, .bin, or USB installer. USB packages load both hub and PD updates. Scaler packages include their HDMI/bridge update.", primaryTitle: "Choose Firmware File…", primaryEnabled: true),
                    GuidedPresentation(step: 3, title: "Ready to install 19.02", message: "Click Install to begin. A verified backup will be saved automatically. Keep monitor power and USB connected until installation finishes. For USB updates, prefer USB-B with video on HDMI or DisplayPort. Disconnect USB storage and accessories, and keep your Mac powered independently.", primaryTitle: "Install 19.02", primaryEnabled: true),
                    GuidedPresentation(step: 3, title: "Installing the selected firmware", message: "Keep monitor power and USB connected. The picture can go black during maintenance. Wait for the app to finish before disconnecting anything.", activity: "Installing the selected firmware · 38% of this step", primaryTitle: "Please Wait…", running: true, progress: 38),
                    GuidedPresentation(step: 4, title: "Firmware installed", message: "Next: unplug the monitor’s DC power cable for at least 10 seconds, then reconnect it. Keep USB connected. For scaler packages, if “Processing update” appears, wait until it finishes and perform another 10-second DC power cycle.", activity: "The written firmware was verified successfully.", primaryTitle: "I’ve Finished the Power Cycle", primaryEnabled: true)
                ] {
                    view.present(state)
                    view.compareButton.isHidden = false
                    host.layoutSubtreeIfNeeded()
                    let logFrame = view.convert(view.detailsScroll.bounds, from: view.detailsScroll)
                    require(logFrame.maxY <= 741, "expanded diagnostics fit the page during \(state.title)")
                    for badge in view.steps {
                        require(descendants(badge).compactMap { $0 as? NSTextField }.allSatisfy { $0.frame.width >= 20 }, "step titles remain visible")
                    }
                }
                host.layoutSubtreeIfNeeded()
                require(!view.detailsScroll.isHidden, "diagnostics stay visible after finishing")
            }
        }
        print("\(checks) guided UI checks passed. Four light/dark offscreen renderings saved. No monitor operations or app launch.")
    }
}

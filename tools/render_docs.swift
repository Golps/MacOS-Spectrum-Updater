import AppKit

// Documentation-only: render real presentation views with simulated states.
// No AppDelegate, USB transport, registry discovery, or hardware operation runs.
@main enum RenderDocumentation {
    static func save(_ view: NSView, appearance: NSAppearance, to url: URL) throws {
        view.layoutSubtreeIfNeeded()
        let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds)!
        appearance.performAsCurrentDrawingAppearance { view.cacheDisplay(in: view.bounds, to: bitmap) }
        try bitmap.representation(using: .png, properties: [:])!.write(to: url)
    }
    static func main() throws {
        _ = NSApplication.shared
        let destination = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "docs/assets", isDirectory: true)
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
        for theme in ["dark", "light"] {
            let appearance = NSAppearance(named: theme == "dark" ? .darkAqua : .aqua)!
            for index in 1...5 {
                let host = NSView(frame: NSRect(x: 0, y: 0, width: 840, height: 740))
                host.appearance = appearance
                let view = GuidedUpdaterView(frame: host.bounds)
                host.addSubview(view)
                NSLayoutConstraint.activate([
                    view.leadingAnchor.constraint(equalTo: host.leadingAnchor),
                    view.trailingAnchor.constraint(equalTo: host.trailingAnchor),
                    view.topAnchor.constraint(equalTo: host.topAnchor),
                    view.bottomAnchor.constraint(equalTo: host.bottomAnchor)
                ])
                view.restoreButton.isEnabled = false
                view.backupLabel.stringValue = "Backups are saved privately. This monitor has no unique serial; use a vendor file to revert."
                if index == 1 {
                    view.backupLabel.stringValue = "Backups are saved privately. Only backups matching this monitor are offered for restore."
                    view.firmwareName.stringValue = "No firmware selected"
                    view.firmwareSummary.stringValue = "Choose an original vendor ZIP, .bin file or USB installer. Firmware is not bundled with this app."
                    view.componentsLabel.stringValue = "Scaler + included HDMI/bridge: no file selected\nUSB hub: no file selected\nUSB-C / Power Delivery: no file selected"
                    view.setConnections([], selectedIndex: nil)
                    view.present(GuidedPresentation(step: 1, title: "Select your monitor & firmware", message: "Select the model on your monitor’s label, then choose the original vendor ZIP, .bin, or USB installer. USB packages load both hub and PD updates. Scaler packages include their HDMI/bridge update.", primaryTitle: "Choose Firmware File…", primaryEnabled: true))
                } else {
                    view.firmwarePopup.addItems(withTitles: ["Scaler + included HDMI/bridge · V108 Beta 3", "USB hub · USB hub 06A4", "USB-C / Power Delivery · USB-C / PD 19.02"])
                    view.firmwarePopup.isHidden = false
                    view.firmwareName.stringValue = "V108 Beta 3 · Scaler + included HDMI/bridge"
                    view.firmwareSummary.stringValue = "Vendor Beta03, revision 1169-E771. Includes scaler and secondary components; no trailing Processing update payload."
                    view.componentsLabel.stringValue = "Scaler + included HDMI/bridge: V108 Beta 3\nUSB hub: USB hub 06A4\nUSB-C / Power Delivery: USB-C / PD 19.02"
                    view.setConnections(["Spectrum monitor"], selectedIndex: 0)
                    view.appendLog(.init(text: "Firmware integrity checks passed", tone: .success))
                    if index == 2 {
                        view.present(GuidedPresentation(step: 2, title: "Monitor connected", message: "Your Spectrum’s USB connection is ready. Check the model and selected firmware on the left, then continue to the installation step. No firmware has been written.", primaryTitle: "Continue to Install", primaryEnabled: true))
                    } else if index == 3 {
                        view.present(GuidedPresentation(step: 3, title: "Ready to install D03.V108T", message: "Click Install to begin. A verified backup will be saved automatically. Keep monitor power and USB connected until installation finishes.", primaryTitle: "Install D03.V108T", primaryEnabled: true))
                    } else if index == 4 {
                        view.appendLog(.init(text: "Installed scaler firmware validated", tone: .success))
                        view.appendLog(.init(text: "Backup saved and integrity verified", tone: .success))
                        view.appendLog(.init(text: "Programming firmware · 38%", tone: .information))
                        view.present(GuidedPresentation(step: 3, title: "Installing the selected firmware", message: "Keep monitor power and USB connected. The picture can go black during maintenance. Wait for the app to finish before disconnecting anything.", activity: "Installing the selected firmware · 38% of this step", primaryTitle: "Please Wait…", running: true, progress: 38))
                    } else {
                        view.appendLog(.init(text: "Programming and readback verification succeeded", tone: .success))
                        view.appendLog(.init(text: "Operation completed successfully", tone: .success))
                        view.present(GuidedPresentation(step: 4, title: "Firmware installed", message: "Next: unplug the monitor’s DC power cable for at least 10 seconds, then reconnect it. Keep USB connected. For scaler packages, if “Processing update” appears, wait until it finishes and perform another 10-second DC power cycle.", activity: "The written firmware was verified successfully.", primaryTitle: "I’ve Finished the Power Cycle", primaryEnabled: true))
                        view.importButton.isEnabled = false
                        view.modelPopup.isEnabled = false
                        view.firmwarePopup.isEnabled = false
                    }
                }
                host.layoutSubtreeIfNeeded()
                view.applyColors()
                host.layoutSubtreeIfNeeded()
                let names = ["01-select", "02-connected", "03-ready", "04-installing", "05-finish"]
                try save(view, appearance: appearance, to: destination.appendingPathComponent("\(names[index-1])-\(theme).png"))
            }
        }
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 256, pixelsHigh: 256, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
        SpectrumBrand.draw(in: NSRect(x: 0, y: 0, width: 256, height: 256))
        NSGraphicsContext.restoreGraphicsState()
        try bitmap.representation(using: .png, properties: [:])!.write(to: destination.appendingPathComponent("logo.png"))
        print("10 light/dark UI images and original logo rendered with simulated data. No monitor operations.")
    }
}

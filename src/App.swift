import AppKit
import UniformTypeIdentifiers

final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    private enum Stage { case checking, ready, working, powerCycle, finished, issue }
    private var window: NSWindow!
    private var ui: GuidedUpdaterView!
    private var process: Process?
    private var discoveryTimer: Timer?
    private var confirmationOpen = false
    private var stage: Stage = .checking
    private var release = FirmwareCatalog.placeholder
    private var selectedModel: String {
        let index = ui.modelPopup.indexOfSelectedItem
        return MonitorModel.supported.indices.contains(index) ? MonitorModel.supported[index].id : ""
    }
    private var checkedFiles: [String: URL] = [:]
    private var importedEntries: [FirmwareRelease] { FirmwareCatalog.entries.filter { checkedFiles[$0.id] != nil } }
    private var firmwareErrors: [String: String] = [:]
    private var connections: [MonitorConnection] = []
    private var selectedID: String?
    private var connectionReviewed = false
    private var backups: [SavedBackup] = []
    private var problem = ""
    private var activity = ""
    private var progressValue: Double?
    private var progressBuffer = ""
    private var lastWrittenRelease: FirmwareRelease?
    private var completedTitle = "Installation completed"
    private var completedMessage = ""
    private var workingTitle = "Updating the monitor"

    private var selectedMonitor: MonitorConnection? {
        connections.first { $0.registryID == selectedID }
    }
    private var busy: Bool { process != nil || confirmationOpen || stage == .checking || stage == .working }
    private var uniqueSelectedSerial: Bool {
        guard let monitor = selectedMonitor, ManagedBackups.usableSerial(monitor.serial) else { return false }
        let normalized = monitor.serial.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return connections.filter { $0.serial.trimmingCharacters(in: .whitespacesAndNewlines).lowercased() == normalized }.count == 1
    }

    func applicationWillTerminate(_ notification: Notification) {
        for url in checkedFiles.values { try? FileManager.default.removeItem(at: url.deletingLastPathComponent()) }
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        let menu = NSMenu()
        let root = NSMenuItem()
        let submenu = NSMenu()
        submenu.addItem(withTitle: "Quit Spectrum Updater", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        root.submenu = submenu
        menu.addItem(root)
        app.mainMenu = menu

        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 840, height: 740), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "Spectrum Updater"
        window.delegate = self
        window.contentMinSize = NSSize(width: 800, height: 740)
        ui = GuidedUpdaterView(frame: NSRect(x: 0, y: 0, width: 840, height: 740))
        window.contentView!.addSubview(ui)
        NSLayoutConstraint.activate([
            ui.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor),
            ui.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor),
            ui.topAnchor.constraint(equalTo: window.contentView!.topAnchor),
            ui.bottomAnchor.constraint(equalTo: window.contentView!.bottomAnchor)
        ])
        ui.firmwarePopup.addItems(withTitles: FirmwareCatalog.entries.map(\.title))
        ui.importButton.target = self
        ui.importButton.action = #selector(importFirmware)
        ui.modelPopup.target = self
        ui.modelPopup.action = #selector(modelChanged)
        ui.firmwarePopup.target = self
        ui.firmwarePopup.action = #selector(firmwareChanged)
        ui.devicePopup.target = self
        ui.devicePopup.action = #selector(deviceChanged)
        ui.refreshButton.target = self
        ui.refreshButton.action = #selector(refreshConnection)
        ui.primaryButton.target = self
        ui.primaryButton.action = #selector(primaryAction)
        ui.restoreButton.target = self
        ui.restoreButton.action = #selector(restoreBackup)
        ui.compareButton.target = self
        ui.compareButton.action = #selector(compareInstalled)
        ui.copyButton.target = self
        ui.copyButton.action = #selector(copyDetails)
        render()
        window.center()
        window.makeKeyAndOrderFront(nil)
        app.activate(ignoringOtherApps: true)

        // Startup checks local resources and enumerates the OS USB registry.
        // It never enters ISP, opens a USB device, or reads monitor firmware.
        stage = .ready
        discoverConnections(quiet: false)
        render()
        discoveryTimer = Timer.scheduledTimer(withTimeInterval: 6, repeats: true) { [weak self] _ in
            guard let self, !self.busy else { return }
            self.discoverConnections(quiet: true)
        }
    }

    @objc private func modelChanged() {
        guard !busy else { return }
        lastWrittenRelease = nil
        connectionReviewed = false
        stage = .ready
        render()
    }

    @objc private func importFirmware() {
        guard !busy, stage != .powerCycle else { return }
        let panel = NSOpenPanel()
        panel.title = "Choose an original Spectrum firmware package"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.allowedContentTypes = ["bin", "zip", "exe"].compactMap { UTType(filenameExtension: $0) }
        guard panel.runModal() == .OK, let url = panel.url else { return }
        var snapshots: [(FirmwareRelease, URL)] = []
        do {
            let items = try VendorImport.load(url)
            for item in items {
                let folder = FileManager.default.temporaryDirectory.appendingPathComponent("SpectrumImport-" + UUID().uuidString, isDirectory: true)
                try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
                let snapshot = folder.appendingPathComponent(item.release.filename)
                try item.data.write(to: snapshot, options: .withoutOverwriting)
                _ = try FirmwareCatalog.validate(item.release, url: snapshot)
                snapshots.append((item.release, snapshot))
            }
            validateImports(snapshots, index: 0)
        } catch {
            for (_, snapshot) in snapshots { try? FileManager.default.removeItem(at: snapshot.deletingLastPathComponent()) }
            showProblem(error.localizedDescription)
        }
    }

    private func validateImports(_ items: [(FirmwareRelease, URL)], index: Int) {
        guard index < items.count else {
            release = items[0].0
            lastWrittenRelease = nil
            connectionReviewed = false
            stage = .ready
            render()
            return
        }
        let (entry, snapshot) = items[index]
        run([entry.component == .scaler ? "inspect" : "inspect-usb", snapshot.path]) { status, output in
            do {
                _ = try FirmwareCatalog.confirmInspection(entry, exitStatus: status, output: output)
                // Keep the other component files; replace only this component.
                for previous in self.importedEntries where previous.component == entry.component {
                    if let old = self.checkedFiles.removeValue(forKey: previous.id) { try? FileManager.default.removeItem(at: old.deletingLastPathComponent()) }
                }
                self.checkedFiles[entry.id] = snapshot
                self.validateImports(items, index: index + 1)
            } catch {
                for (_, unused) in items.dropFirst(index) { try? FileManager.default.removeItem(at: unused.deletingLastPathComponent()) }
                self.showProblem(error.localizedDescription)
            }
        }
    }

    private func checkedFirmware(_ entry: FirmwareRelease) throws -> URL {
        guard let url = checkedFiles[entry.id], entry.canFlash, entry.models.contains(selectedModel) else { throw FirmwareCatalogError.inspectionFailed }
        return try FirmwareCatalog.validate(entry, url: url)
    }

    @objc private func refreshConnection() {
        guard !busy else { return }
        discoverConnections(quiet: false)
    }

    private func discoverConnections(quiet: Bool, done: (() -> Void)? = nil) {
        guard process == nil, !confirmationOpen else { return }
        run(["devices"], showOutput: !quiet) { result, output in
            guard result == 0, let data = output.data(using: .utf8),
                  let rows = try? JSONSerialization.jsonObject(with: data) as? [[String: String]] else {
                if !quiet { self.showProblem("The USB connection list could not be read. Try Refresh Connection.", output: output) }
                done?()
                return
            }
            let previousConnections = self.connections
            let previousSelection = self.selectedID
            self.connections = rows.compactMap { row in
                guard let id = row["registry_id"] else { return nil }
                return MonitorConnection(registryID: id, locationID: row["location_id"] ?? "", product: row["product"] ?? "VIA USB bridge", serial: row["serial"] ?? "")
            }
            if self.connections.count == 1 { self.selectedID = self.connections[0].registryID }
            else if !self.connections.contains(where: { $0.registryID == self.selectedID }) { self.selectedID = nil }
            let changed = previousConnections != self.connections || previousSelection != self.selectedID
            if changed { self.connectionReviewed = false }
            if changed || !quiet {
                self.updateBackupList()
                self.activity = ""
                self.render()
            }
            done?()
        }
    }

    @objc private func firmwareChanged() {
        guard !busy, stage != .powerCycle else { return }
        let index = ui.firmwarePopup.indexOfSelectedItem
        guard importedEntries.indices.contains(index) else { return }
        release = importedEntries[index]
        connectionReviewed = false
        problem = ""
        stage = .ready
        render()
    }

    @objc private func deviceChanged() {
        guard !busy else { return }
        let index = ui.devicePopup.indexOfSelectedItem - 1
        selectedID = connections.indices.contains(index) ? connections[index].registryID : nil
        connectionReviewed = false
        updateBackupList()
        render()
    }

    private func updateBackupList() {
        let previousID = backups.indices.contains(ui.backupPopup.indexOfSelectedItem) ? backups[ui.backupPopup.indexOfSelectedItem].id : nil
        do { backups = uniqueSelectedSerial ? try selectedMonitor.map { try ManagedBackups.list(for: $0) } ?? [] : [] }
        catch {
            backups = []
            ui.append("Saved backups: \(error.localizedDescription)\n")
        }
        ui.setBackupTitles(backups.map(\.displayTitle), canRestore: !backups.isEmpty)
        if let index = backups.firstIndex(where: { $0.id == previousID }) { ui.backupPopup.selectItem(at: index) }
    }

    @objc private func primaryAction() {
        guard !busy else { return }
        if stage == .powerCycle {
            // User acknowledgement, followed only by another passive registry check.
            stage = .finished
            discoverConnections(quiet: false)
            render()
        } else if release.id == "none" || !release.models.contains(selectedModel) || !release.canFlash {
            importFirmware()
        } else if selectedMonitor == nil {
            discoverConnections(quiet: false)
        } else if !connectionReviewed {
            connectionReviewed = true
            render()
        } else { installSelected() }
    }

    private func confirm(title: String, message: String, button: String) -> Bool {
        confirmationOpen = true
        render()
        defer { confirmationOpen = false; render() }
        let alert = NSAlert()
        alert.messageText = title
        alert.informativeText = message
        alert.addButton(withTitle: button)
        alert.addButton(withTitle: "Cancel")
        return alert.runModal() == .alertFirstButtonReturn
    }

    private func confirmPhysicalModel() -> Bool {
        confirmationOpen = true
        render()
        defer { confirmationOpen = false; render() }
        let alert = NSAlert()
        alert.messageText = "Confirm the monitor’s physical label"
        alert.informativeText = "Read the model printed on the monitor itself and type it exactly below. Do not copy the selection from the app. This extra check prevents a shared firmware image from being mistaken for a different physical model."
        let field = NSTextField(string: "")
        field.placeholderString = selectedModel
        field.frame = NSRect(x: 0, y: 0, width: 260, height: 24)
        alert.accessoryView = field
        alert.addButton(withTitle: "Confirm Model")
        alert.addButton(withTitle: "Cancel")
        guard alert.runModal() == .alertFirstButtonReturn else { return false }
        let entered = field.stringValue.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        guard entered == selectedModel else {
            let mismatch = NSAlert()
            mismatch.alertStyle = .critical
            mismatch.messageText = "Model label does not match"
            mismatch.informativeText = "You selected \(selectedModel), but entered \(entered.isEmpty ? "nothing" : entered). Choose the model printed on the physical monitor before installing scaler firmware."
            mismatch.runModal()
            return false
        }
        return true
    }

    private func installSelected() {
        guard let monitor = selectedMonitor else { return }
        let entry = release
        do {
            let image = try checkedFirmware(entry)
            let routeAdvice = entry.component == .scaler ? "" : "\n\nFor this USB update, prefer a direct USB-B connection with video on HDMI or DisplayPort. Disconnect USB storage and accessories from the monitor, and keep your Mac powered independently."
            if entry.component == .scaler && !confirmPhysicalModel() { return }
            guard confirm(title: "Install \(entry.displayVersion)?", message: "The updater will check your \(selectedModel), save a verified backup on this Mac, then install and verify the selected firmware.\n\nSelected update: \(entry.title)\n\n\(entry.summary)\n\nKeep monitor power and USB connected until installation finishes.\(routeAdvice)", button: "Back Up and Install") else { return }
            let pending = try ManagedBackups.prepare(for: monitor, firmware: entry)
            stage = .working
            workingTitle = "Installing \(entry.version)"
            problem = ""
            progressValue = nil
            activity = "Checking the monitor and preparing its backup…"
            completedTitle = "\(entry.version) installed"
            completedMessage = "The selected vendor firmware passed write and readback verification."
            var args = [entry.component == .scaler ? "flash" : "flash-usb", image.path, "--device", monitor.registryID, "--auto-model", selectedModel, "--sha256", entry.sha256, "--backup", pending.url.path]
            if entry.component == .scaler { args += ["--label-confirm", selectedModel] }
            run(args, maintenance: true) { result, output in
                self.finishWrite(result: result, output: output, pending: pending, monitor: monitor, target: entry)
            }
        } catch {
            if error is FirmwareCatalogError { checkedFiles.removeValue(forKey: entry.id) }
            showProblem(error.localizedDescription)
        }
    }

    private func finishWrite(result: Int32, output: String, pending: PendingBackup, monitor: MonitorConnection, target: FirmwareRelease?, restoring: Bool = false) {
        var backupIssue: String?
        do { _ = try ManagedBackups.complete(pending, for: monitor, firmware: restoring ? nil : target, processOutput: output) }
        catch { backupIssue = error.localizedDescription; ui.appendLog(.init(text: "Backup record error: " + error.localizedDescription + ". The private backup has been retained.", tone: .error)) }
        updateBackupList()
        if result == 0 {
            lastWrittenRelease = target
            if output.contains("Requested firmware is already installed.") {
                completedTitle = "Selected firmware is already installed"
                completedMessage = "The installed firmware already matches this selection. The app saved a verified backup and performed no erase."
            }
            if let backupIssue {
                completedMessage += " The monitor operation succeeded, but the saved-backup record needs attention: \(backupIssue) See the log for the backup result."
            }
            stage = .powerCycle
            activity = ""
            render()
        } else {
            lastWrittenRelease = nil
            showProblem(failureExplanation(output), output: output)
            if let backupIssue { problem += "\n\nSaved-backup record: \(backupIssue)" }
            if output.contains("ISP session ended.") {
                problem += "\n\nThe maintenance session ended. Unplug monitor DC power for 10 seconds, then reconnect before retrying."
            }
            render()
        }
    }

    @objc private func restoreBackup() {
        guard !busy, stage != .powerCycle, uniqueSelectedSerial, let monitor = selectedMonitor,
              backups.indices.contains(ui.backupPopup.indexOfSelectedItem) else { return }
        let saved = backups[ui.backupPopup.indexOfSelectedItem]
        do {
            let image = try ManagedBackups.validateForRestore(saved, monitor: monitor)
            guard confirmPhysicalModel() else { return }
            guard confirm(title: "Restore the saved firmware?", message: "Saved firmware: \(saved.firmwareTitle)\nBackup date: \(saved.displayTitle)\n\nThis backup matches the selected USB serial number. The app checks its integrity and the currently installed firmware for \(selectedModel), then saves another complete backup before restoring. If the current firmware cannot be recognized, restoration stops before erase. Keep power and USB connected.", button: "Back Up and Restore") else { return }
            let pending = try ManagedBackups.prepare(for: monitor, firmware: nil)
            stage = .working
            workingTitle = "Restoring the saved firmware"
            problem = ""
            activity = "Checking the monitor and saved backup…"
            progressValue = nil
            completedTitle = "Saved firmware restored"
            completedMessage = "The saved complete firmware region passed write and readback checks. Try the monitor after its power cycle."
            let expected = FirmwareCatalog.entries.first { $0.sha256 == saved.sourceFirmwareSHA256 }
            run(["restore", image.path, "--device", monitor.registryID, "--auto-model", selectedModel, "--label-confirm", selectedModel, "--sha256", saved.sha256, "--backup", pending.url.path], maintenance: true) { result, output in
                self.finishWrite(result: result, output: output, pending: pending, monitor: monitor, target: expected, restoring: true)
            }
        } catch { showProblem(error.localizedDescription) }
    }

    @objc private func compareInstalled() {
        guard !busy, stage == .finished, let monitor = selectedMonitor, let entry = lastWrittenRelease else { return }
        do {
            let image = try checkedFirmware(entry)
            guard confirm(title: "Compare the installed firmware?", message: "The installation already included a readback check of this component. This optional additional comparison enters monitor maintenance mode, can interrupt the picture, and needs another 10-second DC power cycle when it finishes. It compares the installed bytes with \(entry.title).", button: "Compare Firmware") else { return }
            stage = .working
            workingTitle = "Comparing installed firmware"
            activity = "Reading firmware for comparison…"
            progressValue = nil
            run([entry.component == .scaler ? "verify" : "verify-usb", image.path, "--device", monitor.registryID, "--auto-model", selectedModel], maintenance: true) { result, output in
                if result == 0 {
                    self.completedTitle = "Installed firmware matches \(entry.version)"
                    self.completedMessage = "The optional comparison matched every byte in the selected firmware file. Power-cycle the monitor to return to normal use."
                    self.stage = .powerCycle
                    self.activity = ""
                    self.render()
                } else {
                    self.showProblem(self.failureExplanation(output), output: output)
                    if output.contains("ISP session ended.") { self.problem += "\n\nUnplug monitor DC power for 10 seconds, then reconnect before retrying." }
                    self.render()
                }
            }
        } catch { showProblem(error.localizedDescription) }
    }

    private func render() {
        guard ui != nil else { return }
        let imports = importedEntries
        ui.firmwarePopup.removeAllItems()
        ui.firmwarePopup.addItems(withTitles: imports.map { $0.component.rawValue + " · " + $0.title })
        ui.firmwarePopup.selectItem(at: imports.firstIndex(of: release) ?? 0)
        ui.firmwarePopup.isHidden = imports.count < 2
        ui.firmwareName.stringValue = release.id == "none" ? "No firmware selected" : "\(release.title) · \(release.component.rawValue)"
        ui.componentsLabel.stringValue = UpdateComponent.allCases.map { component in
            let imported = imports.first { $0.component == component }
            return component.rawValue + ": " + (imported?.title ?? "no file selected")
        }.joined(separator: "\n")
        ui.firmwareSummary.stringValue = release.summary
        let index = connections.firstIndex { $0.registryID == selectedID }
        ui.setConnections(connections.enumerated().map { ordinal, monitor in
            connections.count == 1 ? "Spectrum monitor" : "Spectrum monitor \(ordinal + 1) · USB port \(monitor.locationID)"
        }, selectedIndex: index)
        let valid = checkedFiles[release.id] != nil
        let compatible = release.models.contains(selectedModel)
        let canInstall = valid && release.canFlash && compatible && selectedMonitor != nil && !busy
        var presentation: GuidedPresentation
        switch stage {
        case .checking:
            presentation = GuidedPresentation(step: 1, title: "Checking the updater", message: "Looking for your monitor’s USB connection.", activity: activity, primaryTitle: "Checking…", running: true)
        case .working:
            presentation = GuidedPresentation(step: 3, title: workingTitle, message: "Keep monitor power and USB connected. The picture can go black during maintenance. Wait for the app to finish before disconnecting anything.", activity: activity, primaryTitle: "Please Wait…", running: true, progress: progressValue)
        case .powerCycle:
            presentation = GuidedPresentation(step: 4, title: completedTitle, message: "Next: unplug the monitor’s DC power cable for at least 10 seconds, then reconnect it. Keep USB connected. For scaler packages, if “Processing update” appears, wait until it finishes and perform another 10-second DC power cycle.", activity: "\(completedMessage)", primaryTitle: "I’ve Finished the Power Cycle", primaryEnabled: process == nil)
        case .finished:
            presentation = GuidedPresentation(step: 4, title: "Update complete", message: "\(completedMessage)\n\nCheck the installed version in the monitor’s information screen. Choose another loaded component from the firmware menu, or choose an original vendor file to downgrade. Matching scaler backups are available below.", primaryTitle: "Install \(release.displayVersion)", primaryEnabled: canInstall)
        case .issue:
            presentation = GuidedPresentation(step: 3, title: "The operation stopped", message: problem, primaryTitle: selectedMonitor == nil ? "Refresh Connection" : "Retry \(release.displayVersion)", primaryEnabled: selectedMonitor == nil ? !busy : canInstall)
        case .ready:
            if release.id == "none" {
                presentation = GuidedPresentation(step: 1, title: "Select your monitor & firmware", message: "Select the model on your monitor’s label, then choose the original vendor ZIP, .bin, or USB installer. USB packages load both hub and PD updates. Scaler packages include their HDMI/bridge update.", primaryTitle: "Choose Firmware File…", primaryEnabled: !busy)
            } else if !compatible {
                presentation = GuidedPresentation(step: 1, title: "Firmware does not match this model", message: "This file supports: \(release.models.joined(separator: ", ")). Choose the correct model or a matching firmware file. OLED and unlisted model profiles are not supported.", primaryTitle: "Choose Another File…", primaryEnabled: !busy)
            } else if !release.canFlash {
                presentation = GuidedPresentation(step: 1, title: "\(release.component.rawValue) file recognized", message: "\(release.summary) This file cannot be sent through the scaler flash engine. Select a scaler package to install an update with this build.", primaryTitle: "Choose Another File…", primaryEnabled: !busy)
            } else if !valid {
                presentation = GuidedPresentation(step: 1, title: "Selected firmware could not be checked", message: "\(firmwareErrors[release.id] ?? "This firmware has not passed its offline checks.")\n\nChoose an original vendor firmware file. Saved-backup restoration remains available when a matching backup and connection are found.", primaryTitle: "Installation Unavailable")
            } else if selectedMonitor == nil {
                presentation = GuidedPresentation(step: 2, title: connections.count > 1 ? "Choose the monitor’s connection" : "Connect your Spectrum", message: "Connect the monitor’s USB data cable to this Mac. In the monitor settings, set “Select USB hub source” to the connected Type-B or Type-C port. Detection is automatic. The model is checked when you start installation.", primaryTitle: "Refresh Connection", primaryEnabled: !busy)
            } else if !connectionReviewed {
                presentation = GuidedPresentation(step: 2, title: "Monitor connected", message: "Your Spectrum’s USB connection is ready. Check the model and selected firmware on the left, then continue to the installation step. No firmware has been written.", primaryTitle: "Continue to Install", primaryEnabled: !busy)
            } else {
                let advice = release.component == .scaler ? "" : " For USB updates, prefer USB-B with video on HDMI or DisplayPort. Disconnect USB storage and accessories, and keep your Mac powered independently."
                presentation = GuidedPresentation(step: 3, title: "Ready to install \(release.displayVersion)", message: "Click Install to begin. A verified backup will be saved automatically. Keep monitor power and USB connected until installation finishes." + advice, primaryTitle: "Install \(release.displayVersion)", primaryEnabled: canInstall)
            }
        }
        ui.present(presentation)
        ui.firmwarePopup.isEnabled = !busy && stage != .powerCycle
        ui.refreshButton.isEnabled = !busy
        ui.devicePopup.isEnabled = !busy && stage != .powerCycle
        ui.recoveryButton.isEnabled = false
        ui.importButton.isEnabled = !busy && stage != .powerCycle
        ui.modelPopup.isEnabled = !busy && stage != .powerCycle
        ui.restoreButton.isEnabled = !busy && stage != .powerCycle && uniqueSelectedSerial && !backups.isEmpty
        ui.backupPopup.isEnabled = ui.restoreButton.isEnabled
        ui.compareButton.isHidden = stage != .finished || lastWrittenRelease == nil
        ui.compareButton.isEnabled = !busy && selectedMonitor != nil
        if selectedMonitor != nil, !uniqueSelectedSerial {
            ui.backupLabel.stringValue = "Backups are saved privately. This monitor has no unique serial; use a vendor file to revert."
        } else { ui.backupLabel.stringValue = "Backups are saved privately. Only backups matching this monitor are offered for restore." }
    }

    private func run(_ args: [String], showOutput: Bool = true, maintenance: Bool = false, done: @escaping (Int32, String) -> Void) {
        guard process == nil else { return }
        guard let executable = Bundle.main.executableURL?.deletingLastPathComponent().appendingPathComponent("spectrum-updater") else {
            done(-1, "The updater engine is missing.")
            return
        }
        let p = Process()
        let pipe = Pipe()
        p.executableURL = executable
        p.arguments = args
        p.standardOutput = pipe
        p.standardError = pipe
        progressBuffer = ""
        if showOutput { ui.appendLog(UpdateLogFormatter.operation(args.first ?? "")) }
        process = p
        if args.first != "devices" { render() }
        do { try p.run() }
        catch {
            process = nil
            done(-1, "Could not start the updater engine: \(error.localizedDescription)")
            if args.first != "devices" { render() }
            return
        }
        DispatchQueue.global(qos: .userInitiated).async {
            var data = Data()
            var displayLog = UpdateLogFormatter()
            while true {
                let bytes = pipe.fileHandleForReading.availableData
                if bytes.isEmpty { break }
                data.append(bytes)
                let part = String(decoding: bytes, as: UTF8.self)
                let events = displayLog.ingest(part)
                DispatchQueue.main.async {
                    if showOutput { events.forEach(self.ui.appendLog) }
                    if maintenance { self.consumeProgress(part) }
                }
            }
            p.waitUntilExit()
            let output = String(decoding: data, as: UTF8.self)
            let finalEvents = displayLog.ingest("", finish: true)
            DispatchQueue.main.async {
                if showOutput {
                    finalEvents.forEach(self.ui.appendLog)
                    self.ui.appendLog(.init(text: p.terminationStatus == 0 ? "Operation completed successfully" : "Operation stopped with an error", tone: p.terminationStatus == 0 ? .success : .error))
                }
                self.process = nil
                done(p.terminationStatus, output)
                if args.first != "devices" { self.render() }
            }
        }
    }

    private func consumeProgress(_ text: String) {
        progressBuffer += text
        while let end = progressBuffer.firstIndex(of: "\n") {
            let line = String(progressBuffer[..<end])
            progressBuffer.removeSubrange(...end)
            let phases = [("Reading", "Reading and checking monitor firmware"), ("Erasing", "Preparing the active firmware area"), ("Checking erase", "Checking the prepared area"), ("Programming", "Installing the selected firmware"), ("Verifying", "Checking the written firmware")]
            if let phase = phases.first(where: { line.hasPrefix($0.0 + " ") }),
               let count = line.split(separator: " ").last {
                let values = count.split(separator: "/").compactMap { Double($0) }
                if values.count == 2, values[1] > 0 {
                    progressValue = 100 * values[0] / values[1]
                    activity = "\(phase.1) · \(Int(progressValue!))% of this step"
                }
            } else if line.hasPrefix("Backup SHA256:") {
                activity = "The complete backup is verified and saved. Preparing installation…"
                progressValue = nil
            } else if line.hasPrefix("Current FW2:") {
                activity = "Installed firmware matches the selected IPS profile. Saving its complete backup…"
            }
        }
        render()
    }

    private func failureExplanation(_ output: String) -> String {
        if output.contains("lacks a supported geometry profile") || output.contains("No validated flash geometry profile") || output.contains("has no supported 4-KiB profile") {
            return "This flash-memory chip is unsupported. No firmware was erased or installed. Unplug the monitor’s DC power for 10 seconds, reconnect it, and check Details for the detected chip."
        }
        if output.contains("Automatic model identification could not recognize") || output.contains("Connected scaler lacks required") || output.contains("selected IPS model catalog") || output.contains("Current FW2 validation failed") || output.contains("Installed scaler is not a known compatible") {
            return "The updater could not establish the selected IPS model profile from its installed firmware. It stopped at the model check. See the log for the precise result."
        }
        if output.contains("Scaler setup register did not read back correctly") || output.contains("Flash-access enable mismatch") || output.contains("Flash-access enable did not read back correctly") {
            return "The monitor did not complete programming-mode setup. No firmware was erased or installed; any completed backup is retained. Unplug DC power for 10 seconds, then reconnect. See the log for the setup readings."
        }
        if output.contains("no directly paired VIA") || output.contains("Cannot open VIA bridge") {
            return "The selected USB connection could not be opened for this update. Check the upstream data cable and monitor USB hub source. See the log for the connection result."
        }
        let lines = output.components(separatedBy: "\n").filter { !$0.isEmpty && !$0.hasPrefix("Reading ") && !$0.hasPrefix("Erasing ") && !$0.hasPrefix("Programming ") && !$0.hasPrefix("Verifying ") && !$0.hasPrefix("Checking erase ") }
        let explanation = lines.last(where: { !$0.hasPrefix("ISP session ended.") }) ?? "The updater engine did not complete this operation."
        return "\(explanation)\n\nKeep any completed saved backup. Check the log before another operation."
    }

    private func showProblem(_ message: String, output: String = "") {
        stage = .issue
        problem = UpdateLogFormatter.safe(message)
        activity = ""
        ui.append("\n\(message)\n")
        if !output.isEmpty { ui.append("Operation stopped. See the log above for its result.\n") }
        render()
    }
    @objc private func copyDetails() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(ui.log.string, forType: .string)
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if process != nil || confirmationOpen {
            ui.append("An operation is running. Let it finish before quitting.\n")
            return .terminateCancel
        }
        return .terminateNow
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { process == nil && !confirmationOpen }
    func windowShouldClose(_ sender: NSWindow) -> Bool {
        if process != nil || confirmationOpen {
            ui.append("An operation is running. Let it finish before closing this window.\n")
            return false
        }
        return true
    }
}

@main
enum SpectrumApplication {
    static func main() {
        let delegate = AppDelegate()
        NSApplication.shared.delegate = delegate
        withExtendedLifetime(delegate) { NSApplication.shared.run() }
    }
}

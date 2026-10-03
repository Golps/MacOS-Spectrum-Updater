import AppKit

struct MonitorModel: Equatable {
    let id: String
    let displayName: String

    static let supported = [
        MonitorModel(id: "ES07D03", displayName: "ES07D03 · 4K 144 Hz · Matte"),
        MonitorModel(id: "ES07DC9", displayName: "ES07DC9 · 4K 144 Hz · Glossy"),
        MonitorModel(id: "ES07E30", displayName: "ES07E30 · 4K 144 Hz · Gorilla Glass"),
        MonitorModel(id: "ES07D02", displayName: "ES07D02 · QHD 280 Hz · Matte")
    ]
}

// Pure presentation: this view has no bundle, process, filesystem or USB access.
// It can be constructed and rendered offscreen for appearance/layout checks.
struct GuidedPresentation: Equatable {
    var step: Int
    var title: String
    var message: String
    var activity: String = ""
    var primaryTitle: String
    var primaryEnabled = false
    var running = false
    var progress: Double? = nil
}

final class GuidedUpdaterView: NSView {
    override var isFlipped: Bool { true }
    let firmwarePopup = NSPopUpButton()
    let importButton = NSButton(title: "Choose Firmware File…", target: nil, action: nil)
    let modelPopup = NSPopUpButton()
    let componentsLabel = GuidedUpdaterView.label("Scaler + HDMI/bridge: choose a vendor package\nUSB hub: choose the vendor hub binary\nUSB-C / PD: choose the vendor PD binary", size: 13)
    let firmwareSummary = GuidedUpdaterView.label("", size: 13)
    let firmwareName = GuidedUpdaterView.label("", size: 13, bold: true)
    let connectionLabel = GuidedUpdaterView.label("Checking for the monitor’s USB connection…", size: 13, bold: true)
    let devicePopup = NSPopUpButton()
    let refreshButton = NSButton(title: "Refresh Connection", target: nil, action: nil)
    let phaseTitle = GuidedUpdaterView.label("Checking the included updates", size: 21, bold: true)
    let phaseMessage = GuidedUpdaterView.label("The firmware files are being checked locally. No monitor maintenance commands run at startup.", size: 14)
    let activityLabel = GuidedUpdaterView.label("", size: 12)
    let primaryButton = NSButton(title: "Checking…", target: nil, action: nil)
    let recoveryButton = NSButton(title: "Use Beta 3 Fallback", target: nil, action: nil)
    let restoreButton = NSButton(title: "Restore Saved Backup", target: nil, action: nil)
    let compareButton = NSButton(title: "Compare Installed Firmware", target: nil, action: nil)
    let backupPopup = NSPopUpButton()
    let backupLabel = GuidedUpdaterView.label("Backups are saved automatically and kept privately on this Mac.", size: 12)
    let copyButton = NSButton(title: "Copy Log", target: nil, action: nil)
    let progress = NSProgressIndicator()
    let log = NSTextView()
    let detailsScroll = NSScrollView()
    let steps: [StepBadgeView]
    private var lastPresentation: GuidedPresentation?
    private var lastConnections: [String] = []
    private var lastSelection: Int?
    private var hasConnections = false
    private var detailsHeight: NSLayoutConstraint!
    private let rootStack = NSStackView()
    private let firmwareBox = NSBox()
    private let actionBox = NSBox()
    private let deviceRow = NSStackView()
    private let backupRow = NSStackView()
    private let contentStack = NSStackView()

    override init(frame: NSRect) {
        steps = ["Select monitor", "Connect", "Install", "Finish"].enumerated().map { StepBadgeView(number: $0.offset + 1, title: $0.element) }
        super.init(frame: frame)
        translatesAutoresizingMaskIntoConstraints = false
        wantsLayer = true
        rootStack.orientation = .vertical
        rootStack.alignment = .leading
        rootStack.spacing = 10
        rootStack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(rootStack)
        NSLayoutConstraint.activate([
            rootStack.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 18),
            rootStack.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -18),
            rootStack.topAnchor.constraint(equalTo: topAnchor, constant: 16),
            rootStack.bottomAnchor.constraint(lessThanOrEqualTo: bottomAnchor, constant: -16)
        ])
        let header = NSStackView()
        header.orientation = .horizontal
        header.alignment = .centerY
        header.spacing = 14
        let mark = SpectrumMarkView()
        mark.widthAnchor.constraint(equalToConstant: 48).isActive = true
        mark.heightAnchor.constraint(equalToConstant: 48).isActive = true
        header.addArrangedSubview(mark)
        let heading = NSStackView()
        heading.orientation = .vertical
        heading.alignment = .leading
        heading.spacing = 4
        heading.addArrangedSubview(Self.label("Spectrum Updater", size: 23, bold: true))
        heading.addArrangedSubview(Self.label("Spectrum IPS  •  Vendor firmware updater", size: 13))
        header.addArrangedSubview(heading)
        heading.setContentHuggingPriority(.defaultLow, for: .horizontal)
        addFullWidth(header)
        let strip = NSStackView(views: steps)
        strip.orientation = .horizontal
        strip.distribution = .fillEqually
        strip.spacing = 8
        addFullWidth(strip)

        let firmwareContent = NSStackView()
        firmwareContent.orientation = .vertical
        firmwareContent.alignment = .leading
        firmwareContent.spacing = 8
        addTo(firmwareContent, Self.label("Monitor & firmware", size: 12, bold: true))
        firmwarePopup.bezelStyle = .rounded
        modelPopup.addItems(withTitles: MonitorModel.supported.map(\.displayName))
        addTo(firmwareContent, modelPopup)
        addTo(firmwareContent, importButton)
        firmwarePopup.isHidden = true
        addTo(firmwareContent, firmwarePopup)
        addTo(firmwareContent, firmwareName)
        addTo(firmwareContent, firmwareSummary)
        addTo(firmwareContent, componentsLabel)
        configureBox(firmwareBox, content: firmwareContent)
        let panels = NSStackView(views: [firmwareBox, actionBox])
        panels.orientation = .horizontal
        panels.alignment = .top
        panels.distribution = .fillEqually
        panels.spacing = 12
        addFullWidth(panels)

        contentStack.orientation = .vertical
        contentStack.alignment = .leading
        contentStack.spacing = 8
        addTo(contentStack, phaseTitle)
        addTo(contentStack, phaseMessage)
        addTo(contentStack, connectionLabel)
        deviceRow.orientation = .horizontal
        deviceRow.spacing = 10
        devicePopup.bezelStyle = .rounded
        devicePopup.setContentHuggingPriority(.defaultLow, for: .horizontal)
        devicePopup.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        deviceRow.addArrangedSubview(devicePopup)
        deviceRow.addArrangedSubview(refreshButton)
        addTo(contentStack, deviceRow)
        devicePopup.isHidden = true
        progress.style = .bar
        progress.minValue = 0
        progress.maxValue = 100
        progress.isIndeterminate = true
        addTo(contentStack, progress)
        addTo(contentStack, activityLabel)
        primaryButton.bezelStyle = .rounded
        primaryButton.controlSize = .large
        primaryButton.font = .systemFont(ofSize: 14, weight: .semibold)
        primaryButton.keyEquivalent = "\r"
        primaryButton.widthAnchor.constraint(greaterThanOrEqualToConstant: 180).isActive = true
        addTo(contentStack, primaryButton)
        configureBox(actionBox, content: contentStack)

        let recover = NSStackView()
        recover.orientation = .horizontal
        recover.spacing = 12
        recover.distribution = .fillEqually
        recover.addArrangedSubview(recoveryButton)
        recover.addArrangedSubview(restoreButton)
        recover.addArrangedSubview(compareButton)
        recoveryButton.isHidden = true
        addFullWidth(recover)
        compareButton.isHidden = true
        backupRow.orientation = .horizontal
        backupRow.spacing = 10
        backupRow.addArrangedSubview(backupPopup)
        backupPopup.setContentHuggingPriority(.defaultLow, for: .horizontal)
        addFullWidth(backupRow)
        backupRow.isHidden = true
        addFullWidth(backupLabel)

        let detailRow = NSStackView()
        detailRow.orientation = .horizontal
        detailRow.spacing = 12
        let logHeading = Self.label("Diagnostics", size: 12, bold: true)
        let spacer = NSView()
        spacer.setContentHuggingPriority(.defaultLow, for: .horizontal)
        detailRow.addArrangedSubview(logHeading)
        detailRow.addArrangedSubview(spacer)
        copyButton.widthAnchor.constraint(equalToConstant: 100).isActive = true
        detailRow.addArrangedSubview(copyButton)
        addFullWidth(detailRow)
        detailsScroll.borderType = .bezelBorder
        detailsScroll.hasVerticalScroller = true
        detailsScroll.drawsBackground = true
        log.isEditable = false
        log.isSelectable = true
        log.isRichText = true
        log.isVerticallyResizable = true
        log.isHorizontallyResizable = false
        log.autoresizingMask = [.width]
        log.textContainer?.widthTracksTextView = true
        log.textContainerInset = NSSize(width: 10, height: 10)
        log.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        log.frame = NSRect(x: 0, y: 0, width: max(100, frame.width-48), height: 130)
        detailsScroll.documentView = log
        addFullWidth(detailsScroll)
        detailsHeight = detailsScroll.heightAnchor.constraint(equalToConstant: 130)
        detailsHeight.isActive = true
        for button in [refreshButton, recoveryButton, restoreButton, compareButton, copyButton] {
            button.bezelStyle = .rounded
        }
        applyColors()
        present(GuidedPresentation(step: 1, title: "Checking the updater", message: "Looking for your monitor’s USB connection.", primaryTitle: "Checking…", running: true))
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is not supported") }

    static func label(_ text: String, size: CGFloat, bold: Bool = false) -> NSTextField {
        let label = NSTextField(wrappingLabelWithString: text)
        label.font = bold ? .systemFont(ofSize: size, weight: .semibold) : .systemFont(ofSize: size)
        label.textColor = .labelColor
        label.drawsBackground = false
        label.isSelectable = true
        label.maximumNumberOfLines = 0
        label.setContentCompressionResistancePriority(.required, for: .vertical)
        return label
    }
    private func addFullWidth(_ view: NSView) {
        rootStack.addArrangedSubview(view)
        view.widthAnchor.constraint(equalTo: rootStack.widthAnchor).isActive = true
    }
    private func addTo(_ stack: NSStackView, _ view: NSView) {
        stack.addArrangedSubview(view)
        view.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
    }
    private func configureBox(_ box: NSBox, content: NSStackView) {
        box.boxType = .custom
        box.titlePosition = .noTitle
        box.cornerRadius = 10
        box.borderWidth = 1
        box.contentViewMargins = NSSize(width: 14, height: 12)
        guard let holder = box.contentView else { return }
        content.translatesAutoresizingMaskIntoConstraints = false
        holder.addSubview(content)
        NSLayoutConstraint.activate([
            content.leadingAnchor.constraint(equalTo: holder.leadingAnchor),
            content.trailingAnchor.constraint(equalTo: holder.trailingAnchor),
            content.topAnchor.constraint(equalTo: holder.topAnchor),
            content.bottomAnchor.constraint(equalTo: holder.bottomAnchor)
        ])
    }
    override func viewDidChangeEffectiveAppearance() {
        super.viewDidChangeEffectiveAppearance()
        applyColors()
    }
    func applyColors() {
        effectiveAppearance.performAsCurrentDrawingAppearance {
            self.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        }
        for box in [firmwareBox, actionBox] {
            box.fillColor = .controlBackgroundColor
            box.borderColor = .separatorColor
        }
        log.backgroundColor = .textBackgroundColor
        log.textColor = .textColor
        log.insertionPointColor = .textColor
        log.typingAttributes = [.font: NSFont.monospacedSystemFont(ofSize: 11, weight: .regular), .foregroundColor: NSColor.textColor]
        detailsScroll.backgroundColor = .textBackgroundColor
        if let storage = log.textStorage, storage.length > 0 {
            storage.enumerateAttribute(Self.logToneKey, in: NSRange(location: 0, length: storage.length)) { value, range, _ in
                let tone = UpdateLogTone(rawValue: value as? String ?? "") ?? .information
                storage.addAttribute(.foregroundColor, value: Self.logColor(tone), range: range)
            }
        }
        needsDisplay = true
    }
    private static let logToneKey = NSAttributedString.Key("SpectrumLogTone")
    private static func logColor(_ tone: UpdateLogTone) -> NSColor {
        switch tone {
        case .information: return .textColor
        case .success: return NSColor(name: nil) { appearance in
            appearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua
                ? NSColor(srgbRed: 0.40, green: 0.88, blue: 0.59, alpha: 1)
                : NSColor(srgbRed: 0.06, green: 0.39, blue: 0.18, alpha: 1)
        }
        case .error: return NSColor(name: nil) { appearance in
            appearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua
                ? NSColor(srgbRed: 1.00, green: 0.49, blue: 0.49, alpha: 1)
                : NSColor(srgbRed: 0.70, green: 0.08, blue: 0.08, alpha: 1)
        }
        }
    }
    func append(_ text: String) {
        for line in text.components(separatedBy: "\n") {
            if let event = UpdateLogFormatter.event(line) { appendLog(event) }
        }
    }
    func appendLog(_ event: UpdateLogEvent) {
        guard let storage = log.textStorage else { return }
        if let container = log.textContainer { log.layoutManager?.ensureLayout(for: container) }
        let clip = detailsScroll.contentView, previousOrigin = clip.bounds.origin
        let follow = log.bounds.height <= clip.bounds.height + 24
            || clip.bounds.maxY >= log.bounds.height - 24
        let marker = event.tone == .success ? "✓ " : event.tone == .error ? "✕ " : "• "
        storage.append(NSAttributedString(string: marker + UpdateLogFormatter.safe(event.text) + "\n", attributes: [
            .font: NSFont.monospacedSystemFont(ofSize: 11, weight: .regular),
            .foregroundColor: Self.logColor(event.tone), Self.logToneKey: event.tone.rawValue
        ]))
        if let container = log.textContainer { log.layoutManager?.ensureLayout(for: container) }
        if follow { log.scrollRangeToVisible(NSRange(location: storage.length, length: 0)) }
        else { clip.scroll(to: previousOrigin); detailsScroll.reflectScrolledClipView(clip) }
    }
    func setConnections(_ descriptions: [String], selectedIndex: Int?) {
        guard !hasConnections || descriptions != lastConnections || selectedIndex != lastSelection else { return }
        hasConnections = true
        lastConnections = descriptions
        lastSelection = selectedIndex
        devicePopup.removeAllItems()
        if descriptions.isEmpty {
            connectionLabel.stringValue = "No compatible USB connection found"
            devicePopup.isHidden = true
        } else if descriptions.count == 1 {
            connectionLabel.stringValue = "\(descriptions[0]) detected over USB"
            devicePopup.addItems(withTitles: descriptions)
            devicePopup.selectItem(at: 0)
            devicePopup.isHidden = true
        } else {
            connectionLabel.stringValue = "Multiple monitor connections detected. Choose one, or connect only the monitor you want to update."
            devicePopup.addItem(withTitle: "Choose the monitor’s USB connection")
            devicePopup.addItems(withTitles: descriptions)
            devicePopup.selectItem(at: selectedIndex.map { $0+1 } ?? 0)
            devicePopup.isHidden = false
        }
        needsLayout = true
    }
    func setBackupTitles(_ titles: [String], canRestore: Bool) {
        backupPopup.removeAllItems()
        backupPopup.addItems(withTitles: titles)
        backupRow.isHidden = titles.isEmpty || !canRestore
        restoreButton.isEnabled = canRestore && !titles.isEmpty
    }
    func present(_ state: GuidedPresentation) {
        guard state != lastPresentation else { return }
        lastPresentation = state
        phaseTitle.stringValue = state.title
        phaseMessage.stringValue = state.message
        activityLabel.stringValue = state.activity
        activityLabel.isHidden = state.activity.isEmpty
        primaryButton.title = state.primaryTitle
        primaryButton.isEnabled = state.primaryEnabled && !state.running
        firmwarePopup.isEnabled = !state.running
        importButton.isEnabled = !state.running
        modelPopup.isEnabled = !state.running
        devicePopup.isEnabled = !state.running
        refreshButton.isEnabled = !state.running
        recoveryButton.isEnabled = !state.running
        restoreButton.isEnabled = restoreButton.isEnabled && !state.running
        backupPopup.isEnabled = !state.running
        compareButton.isEnabled = !state.running
        progress.isHidden = !state.running
        if state.running {
            if let value = state.progress {
                progress.stopAnimation(nil)
                progress.isIndeterminate = false
                progress.doubleValue = max(0, min(100, value))
            } else {
                progress.isIndeterminate = true
                progress.startAnimation(nil)
            }
        } else { progress.stopAnimation(nil) }
        for (index, badge) in steps.enumerated() {
            badge.setState(index + 1 == state.step ? .current : index + 1 < state.step ? .complete : .upcoming)
        }
        needsLayout = true
    }
}

// A compact progress indicator with a distinct current step and completed checks.
final class StepBadgeView: NSView {
    enum State { case upcoming, current, complete }
    private let number: Int
    private let marker: NSTextField
    private let titleLabel: NSTextField
    private let track = NSView()
    private(set) var state: State = .upcoming
    var isHighlighted: Bool { state == .current }
    init(number: Int, title: String) {
        self.number = number
        marker = GuidedUpdaterView.label(String(number), size: 12, bold: true)
        titleLabel = GuidedUpdaterView.label(title, size: 12, bold: true)
        super.init(frame: .zero)
        track.wantsLayer = true
        track.translatesAutoresizingMaskIntoConstraints = false
        addSubview(track)
        marker.alignment = .center
        marker.isSelectable = false
        titleLabel.isSelectable = false
        let row = NSStackView(views: [marker, titleLabel])
        row.alignment = .centerY
        row.spacing = 7
        row.translatesAutoresizingMaskIntoConstraints = false
        addSubview(row)
        NSLayoutConstraint.activate([
            heightAnchor.constraint(equalToConstant: 38),
            track.leadingAnchor.constraint(equalTo: leadingAnchor),
            track.trailingAnchor.constraint(equalTo: trailingAnchor),
            track.bottomAnchor.constraint(equalTo: bottomAnchor),
            track.heightAnchor.constraint(equalToConstant: 2),
            marker.widthAnchor.constraint(equalToConstant: 22),
            titleLabel.widthAnchor.constraint(equalToConstant: 110),
            row.centerXAnchor.constraint(equalTo: centerXAnchor),
            row.centerYAnchor.constraint(equalTo: centerYAnchor, constant: -4),
            row.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor, constant: 8),
            row.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -8)
        ])
        setState(.upcoming)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is not supported") }
    func setState(_ state: State) {
        self.state = state
        marker.stringValue = state == .complete ? "✓" : String(number)
        marker.textColor = state == .current ? .controlAccentColor : .secondaryLabelColor
        titleLabel.textColor = state == .upcoming ? .secondaryLabelColor : .labelColor
        effectiveAppearance.performAsCurrentDrawingAppearance {
            track.layer?.backgroundColor = (state == .current ? NSColor.controlAccentColor : NSColor.separatorColor).cgColor
        }
        setAccessibilityLabel("Step \(number): \(titleLabel.stringValue), \(state)")
    }
    override func viewDidChangeEffectiveAppearance() {
        super.viewDidChangeEffectiveAppearance()
        setState(state)
    }
}

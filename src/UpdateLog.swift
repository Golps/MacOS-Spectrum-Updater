import Foundation

enum UpdateLogTone: String { case information, success, error }
struct UpdateLogEvent { let text: String; let tone: UpdateLogTone }

// Engine output is retained internally for receipts. Visible/copyable logs
// contain operations/results, not commands, source paths or registry dumps.
struct UpdateLogFormatter {
    private var pending = ""
    mutating func ingest(_ text: String, finish: Bool = false) -> [UpdateLogEvent] {
        pending += text
        var lines: [String] = []
        while let end = pending.firstIndex(of: "\n") {
            lines.append(String(pending[..<end])); pending.removeSubrange(...end)
        }
        if finish, !pending.isEmpty { lines.append(pending); pending = "" }
        return lines.compactMap(Self.event)
    }
    static func operation(_ command: String) -> UpdateLogEvent {
        let title: String
        switch command {
        case "inspect", "inspect-usb": title = "Checking the selected firmware"
        case "devices": title = "Checking the USB connection"
        case "flash", "flash-usb": title = "Starting backup and installation"
        case "verify", "verify-usb": title = "Comparing the installed firmware"
        case "restore": title = "Starting backup and restoration"
        default: title = "Starting monitor maintenance"
        }
        return .init(text: title, tone: .information)
    }
    static func safe(_ text: String) -> String {
        var result = text
        // Path-bearing metadata has dedicated mappings below. This fallback
        // removes quoted paths and the rest of an accidental absolute path.
        for pattern in [#"\"(?:/|file:)[^\"]*\""#, #"file://[^\s]+"#, #"(?:^|\s)/(?:Users|private|var|tmp|Volumes|Applications|Library|System)/[^\n]*"#] {
            if let regex = try? NSRegularExpression(pattern: pattern) {
                result = regex.stringByReplacingMatches(in: result, range: NSRange(result.startIndex..., in: result), withTemplate: " [file]")
            }
        }
        return result
    }
    static func event(_ source: String) -> UpdateLogEvent? {
        let line = source.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !line.isEmpty else { return nil }
        if line.hasPrefix("Image: ") || line.hasPrefix("Backup: ") || line.hasPrefix("Backup location:") { return nil }
        if let data = line.data(using: .utf8), let devices = try? JSONSerialization.jsonObject(with: data) as? [[String: String]] {
            return .init(text: devices.isEmpty ? "No compatible USB connection found" : "USB connection found (\(devices.count))", tone: .information)
        }
        if line.hasPrefix("SHA256:") || line.hasPrefix("Backup SHA256:") {
            return line.hasPrefix("Backup") ? .init(text: "Backup saved and integrity verified", tone: .success) : nil
        }
        if line.hasPrefix("Current FW2:") { return .init(text: "Installed scaler firmware validated", tone: .success) }
        if line.hasPrefix("Validated:") { return .init(text: "Firmware integrity checks passed", tone: .success) }
        if line.hasPrefix("Model profile verified:") { return .init(text: "Installed firmware matches the selected model profile", tone: .success) }
        if line.hasPrefix("ISP session ended.") { return .init(text: "Maintenance ended — follow the power-cycle instructions", tone: .information) }
        for (prefix, title) in [("Reading", "Reading firmware"), ("Erasing", "Preparing flash"), ("Checking erase", "Checking erased flash"), ("Programming", "Writing firmware"), ("Verifying", "Verifying written firmware")] {
            if line.hasPrefix(prefix + " "), let count = line.split(separator: " ").last {
                let values = count.split(separator: "/").compactMap { Double($0) }
                if values.count == 2, values[1] > 0 {
                    return .init(text: "\(title) · \(Int(100 * values[0] / values[1]))%", tone: values[0] == values[1] ? .success : .information)
                }
            }
        }
        let text = safe(line), lower = text.lowercased()
        if ["failed", "failure", "error", "mismatch", "differs", "invalid", "unsupported", "not supported", "no supported", "could not", "cannot ", "stopped", "timeout", "short transfer", "not a known", "not the supported", "no directly paired", "not in the selected", "not recognized", "unrecognized", "nothing erased", "nothing sent", "already exists", "out of memory", "rejected"].contains(where: lower.contains) {
            return .init(text: text, tone: .error)
        }
        if ["succeeded", "passed", "verified", "already installed"].contains(where: lower.contains) { return .init(text: text, tone: .success) }
        return .init(text: text, tone: .information)
    }
}

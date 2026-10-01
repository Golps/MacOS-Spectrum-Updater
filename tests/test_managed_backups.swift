import Foundation
import CryptoKit
import Darwin

@main
enum ManagedBackupTests {
    static func main() throws {
        let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("build/managed-backup-test-" + UUID().uuidString)
        defer { try? FileManager.default.removeItem(at: root) }
        var passed: [String] = []
        func check(_ name: String, _ body: () throws -> Void) throws { try body(); passed.append(name) }
        func require(_ condition: Bool, line: UInt = #line) { if !condition { fatalError("Managed backup assertion failed at test line \(line)") } }
        func rejects(_ body: () throws -> Void) throws {
            do { try body() } catch { return }
            fatalError("Expected managed backup rejection")
        }
        let monitor = MonitorConnection(registryID: "1", locationID: "port-a", product: "Fixture monitor", serial: "UNIT-123")
        let reconnected = MonitorConnection(registryID: "42", locationID: "port-b", product: monitor.product, serial: monitor.serial)
        let other = MonitorConnection(registryID: "2", locationID: "port-a", product: monitor.product, serial: "UNIT-456")
        let noSerial = MonitorConnection(registryID: "3", locationID: "port-a", product: monitor.product, serial: "")
        let pending = try ManagedBackups.prepare(for: monitor, firmware: FirmwareCatalog.beta03, root: root)
        try check("private automatic destination, no existing file") {
            require(pending.url.deletingLastPathComponent().standardizedFileURL.path == root.standardizedFileURL.path)
            require(!FileManager.default.fileExists(atPath: pending.url.path))
            let permissions = try FileManager.default.attributesOfItem(atPath: root.path)[.posixPermissions] as! NSNumber
            require(permissions.intValue & 0o777 == 0o700)
        }
        var bytes = try Data(contentsOf: URL(fileURLWithPath: "resources/Firmware/STOCK_ES07D03_Beta03.bin"))
        bytes.append(Data(repeating: 0xa5, count: ManagedBackups.rawByteCount - bytes.count))
        let rawHash = SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined()
        func receipt(_ pending: PendingBackup, rawHash: String, sourceHash: String = FirmwareCatalog.beta03.sha256) throws {
            let data = try JSONSerialization.data(withJSONObject: ["raw_sha256": rawHash, "source_sha256": sourceHash, "bytes": ManagedBackups.rawByteCount])
            let url = URL(fileURLWithPath: pending.url.path + ".receipt.json")
            try data.write(to: url); chmod(url.path, 0o600)
        }
        try bytes.write(to: pending.url); chmod(pending.url.path, 0o600)
        try receipt(pending, rawHash: rawHash)
        let output = "Current FW2: Beta03, SHA256 \(FirmwareCatalog.beta03.sha256)\nBackup SHA256: \(rawHash)\n"
        var saved: SavedBackup!
        try check("completed durable-engine backup is indexed") {
            saved = try ManagedBackups.complete(pending, for: monitor, firmware: FirmwareCatalog.beta03, processOutput: output)
            require(saved != nil && saved.sha256 == rawHash && saved.sourceFirmwareSHA256 == FirmwareCatalog.beta03.sha256)
            require(try ManagedBackups.list(for: monitor, root: root).map(\.id) == [pending.id])
        }
        try check("serial matches across reconnection and port change") {
            require(try ManagedBackups.validateForRestore(saved, monitor: reconnected, root: root) == pending.url)
        }
        try check("different monitor is not offered or restored") {
            require(try ManagedBackups.list(for: other, root: root).isEmpty)
            try rejects { _ = try ManagedBackups.validateForRestore(saved, monitor: other, root: root) }
        }
        try check("missing serial does not silently match a port") {
            require(try ManagedBackups.list(for: noSerial, root: root).isEmpty)
            try rejects { _ = try ManagedBackups.validateForRestore(saved, monitor: noSerial, root: root) }
            require(!ManagedBackups.usableSerial("0000000000000000"))
            require(!ManagedBackups.usableSerial("00-00-00-00"))
            require(!ManagedBackups.usableSerial("FFFFFFFF"))
            require(!ManagedBackups.usableSerial("0000000000000001"))
        }
        try check("unfinished operation cannot create ready backup") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: nil, root: root)
            require(try ManagedBackups.complete(p, for: monitor, firmware: nil, processOutput: "Failed before backup") == nil)
        }
        try check("truncated backup rejected") {
            try bytes.dropLast().write(to: pending.url)
            try rejects { _ = try ManagedBackups.validateForRestore(saved, monitor: monitor, root: root) }
        }
        try check("same-length corruption rejected") {
            bytes[123] ^= 1; try bytes.write(to: pending.url)
            try rejects { _ = try ManagedBackups.validateForRestore(saved, monitor: monitor, root: root) }
            bytes[123] ^= 1; try bytes.write(to: pending.url)
        }
        try check("symlink backup rejected") {
            let target = root.appendingPathComponent("target.bin"); try bytes.write(to: target)
            try FileManager.default.removeItem(at: pending.url)
            try FileManager.default.createSymbolicLink(at: pending.url, withDestinationURL: target)
            try rejects { _ = try ManagedBackups.validateForRestore(saved, monitor: monitor, root: root) }
            try FileManager.default.removeItem(at: pending.url); try bytes.write(to: pending.url)
        }
        try check("duplicate engine hash lines rejected") {
            try rejects { _ = try ManagedBackups.complete(pending, for: monitor, firmware: nil, processOutput: output + "Backup SHA256: \(rawHash)\n") }
        }
        try check("source image hash must match actual backup bytes") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: nil, root: root)
            try bytes.write(to: p.url); try receipt(p, rawHash: rawHash)
            let wrong = output.replacingOccurrences(of: FirmwareCatalog.beta03.sha256, with: FirmwareCatalog.entries[1].sha256)
            try rejects { _ = try ManagedBackups.complete(p, for: monitor, firmware: nil, processOutput: wrong) }
        }
        try check("interrupted install recovers through pending identity and durable receipt") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: FirmwareCatalog.beta03, root: root)
            try bytes.write(to: p.url); try receipt(p, rawHash: rawHash)
            let recovered = try ManagedBackups.list(for: reconnected, root: root).first { $0.id == p.id }
            require(recovered != nil && recovered?.createdAt == p.createdAt)
            require(try ManagedBackups.validateForRestore(recovered!, monitor: reconnected, root: root) == p.url)
            require(!FileManager.default.fileExists(atPath: root.appendingPathComponent(p.id + ".pending.json").path))
        }
        try check("interrupted restore retains a backup with no requested release ID") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: nil, root: root)
            try bytes.write(to: p.url); try receipt(p, rawHash: rawHash)
            let recorded = try ManagedBackups.complete(p, for: monitor, firmware: nil, processOutput: output)
            require(recorded != nil && recorded?.requestedFirmwareID == nil)
        }
        try check("orphan raw file without receipt cannot become a restore option") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: nil, root: root)
            try bytes.write(to: p.url)
            require(try !ManagedBackups.list(for: monitor, root: root).contains { $0.id == p.id })
        }
        try check("mismatched or opaque rescue receipt cannot become a restore option") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: nil, root: root)
            try bytes.write(to: p.url); try receipt(p, rawHash: String(repeating: "0", count: 64))
            require(try !ManagedBackups.list(for: monitor, root: root).contains { $0.id == p.id })
            try receipt(p, rawHash: rawHash, sourceHash: "")
            require(try !ManagedBackups.list(for: monitor, root: root).contains { $0.id == p.id })
        }
        try check("pending backup is recovered only for the recorded serial") {
            let p = try ManagedBackups.prepare(for: other, firmware: nil, root: root)
            try bytes.write(to: p.url); try receipt(p, rawHash: rawHash)
            require(try !ManagedBackups.list(for: monitor, root: root).contains { $0.id == p.id })
            require(try ManagedBackups.list(for: other, root: root).contains { $0.id == p.id })
        }
        try check("logs and metadata are private") {
            for ext in ["log", "json"] {
                let permissions = try FileManager.default.attributesOfItem(atPath: root.appendingPathComponent(pending.id + "." + ext).path)[.posixPermissions] as! NSNumber
                require(permissions.intValue & 0o777 == 0o600)
            }
        }
        let usbRelease = FirmwareCatalog.entries.first { $0.id == "pd-1902" }!
        let usb = try ManagedBackups.prepare(for: monitor, firmware: usbRelease, root: root)
        let usbData = Data(repeating: 0xa5, count: 0x40000)
        let usbHash = SHA256.hash(data: usbData).map { String(format: "%02x", $0) }.joined()
        func usbReceipt(_ bytes: Int = 0x40000, _ sha: String? = nil) throws {
            let data = try JSONSerialization.data(withJSONObject: ["raw_sha256": sha ?? usbHash, "bytes": bytes, "controller": "VL822Q7+VL103", "jedec": "EF3012"])
            try data.write(to: URL(fileURLWithPath: usb.url.path + ".receipt.json"))
        }
        try check("shared SPI backup uses a separate private folder") {
            require(usb.url.deletingLastPathComponent().lastPathComponent == "USB")
            try usbData.write(to: usb.url); try usbReceipt()
            let item = try ManagedBackups.complete(usb, for: monitor, firmware: usbRelease, processOutput: "Backup SHA256: " + usbHash + "\n")
            require(item?.bytes == 0x40000 && item?.sourceFirmwareSHA256 == "")
            require(try !ManagedBackups.list(for: monitor, root: root).contains { $0.id == usb.id })
        }
        try check("shared SPI receipt size or hash mismatch is rejected") {
            let p = try ManagedBackups.prepare(for: monitor, firmware: usbRelease, root: root)
            try usbData.write(to: p.url)
            let receiptData = try JSONSerialization.data(withJSONObject: ["raw_sha256": usbHash, "bytes": 0x80000, "controller": "VL822Q7+VL103", "jedec": "EF3012"])
            try receiptData.write(to: URL(fileURLWithPath: p.url.path + ".receipt.json"))
            try rejects { _ = try ManagedBackups.complete(p, for: monitor, firmware: usbRelease, processOutput: "Backup SHA256: " + usbHash + "\n") }
            try receiptData.write(to: URL(fileURLWithPath: p.url.path + ".receipt.json"))
            try FileManager.default.removeItem(at: p.url)
            try FileManager.default.createSymbolicLink(at: p.url, withDestinationURL: usb.url)
            try rejects { _ = try ManagedBackups.complete(p, for: monitor, firmware: usbRelease, processOutput: "Backup SHA256: " + usbHash + "\n") }
        }
        let result: [String: Any] = ["tests_passed": passed.count, "checks": passed, "device_operations": false]
        print(String(decoding: try JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted, .sortedKeys]), as: UTF8.self))
    }
}

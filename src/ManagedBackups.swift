import Foundation
import CryptoKit
import Darwin

struct MonitorConnection: Codable, Equatable {
    let registryID: String
    let locationID: String
    let product: String
    let serial: String
}

struct PendingBackup {
    let id: String
    let url: URL
    let createdAt: Date
}

struct SavedBackup: Codable, Identifiable, Equatable {
    let id: String
    let sha256: String
    let createdAt: Date
    let firmwareTitle: String
    let sourceFirmwareSHA256: String
    let monitorSerial: String
    let monitorProduct: String
    let locationID: String
    let requestedFirmwareID: String?
    let bytes: Int
    var displayTitle: String {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return "\(formatter.string(from: createdAt)) · \(firmwareTitle)"
    }
}

enum ManagedBackupError: Error, LocalizedError {
    case storageUnavailable, incompleteBackup, integrityFailed, identityUnavailable, wrongMonitor, invalidRecord
    var errorDescription: String? {
        switch self {
        case .storageUnavailable: return "The app could not prepare its private backup storage. Installation has not started."
        case .incompleteBackup: return "The operation did not finish a complete verified backup. Its log has been retained."
        case .integrityFailed: return "The saved backup failed its integrity check. Choose an original vendor firmware file instead."
        case .identityUnavailable: return "This USB connection has no unique serial number for matching a saved backup. You can reinstall an original vendor firmware file."
        case .wrongMonitor: return "This backup belongs to a different USB serial number. Choose an original vendor firmware file instead."
        case .invalidRecord: return "The saved backup record is not valid."
        }
    }
}

enum ManagedBackups {
    static let rawByteCount = 4 * 1024 * 1024

    private struct PendingIntent: Codable {
        let id: String
        let createdAt: Date
        let monitor: MonitorConnection
        let requestedFirmwareID: String?
    }

    private struct BackupReceipt: Codable {
        let rawSHA256: String
        let sourceSHA256: String
        let bytes: Int
        enum CodingKeys: String, CodingKey {
            case rawSHA256 = "raw_sha256", sourceSHA256 = "source_sha256", bytes
        }
    }

    // Production uses private app support; tests inject a workspace directory.
    static func directory(root: URL? = nil) throws -> URL {
        let location: URL
        if let root { location = root }
        else {
            let support = try FileManager.default.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
            location = support.appendingPathComponent("Spectrum Updater", isDirectory: true).appendingPathComponent("Backups", isDirectory: true)
        }
        guard location.isFileURL else { throw ManagedBackupError.storageUnavailable }
        try FileManager.default.createDirectory(at: location, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        var st = stat()
        guard lstat(location.path, &st) == 0, (st.st_mode & S_IFMT) == S_IFDIR, st.st_uid == getuid() else {
            throw ManagedBackupError.storageUnavailable
        }
        guard chmod(location.path, 0o700) == 0 else { throw ManagedBackupError.storageUnavailable }
        return location
    }

    static func prepare(for monitor: MonitorConnection, firmware: FirmwareRelease?, root: URL? = nil) throws -> PendingBackup {
        let base = try directory(root: root)
        let directory = firmware?.component != nil && firmware?.component != .scaler ? try self.directory(root: base.appendingPathComponent("USB", isDirectory: true)) : base
        let id = UUID().uuidString.lowercased()
        let url = directory.appendingPathComponent(id + ".bin")
        guard !FileManager.default.fileExists(atPath: url.path) else { throw ManagedBackupError.storageUnavailable }
        // Save the connection identity before the CLI can erase anything. A
        // crash must not leave a durable raw backup with no way to find it.
        let createdAt = Date(timeIntervalSince1970: Date().timeIntervalSince1970.rounded(.down))
        let intent = PendingIntent(id: id, createdAt: createdAt, monitor: monitor, requestedFirmwareID: firmware?.id)
        try writePrivate(try encode(intent), to: directory.appendingPathComponent(id + ".pending.json"))
        // The CLI creates this file exclusively, checks it through the same open
        // inode, and requests full durable storage before the first erase.
        return PendingBackup(id: id, url: url, createdAt: createdAt)
    }

    static func complete(_ pending: PendingBackup, for monitor: MonitorConnection, firmware: FirmwareRelease?, processOutput: String) throws -> SavedBackup? {
        guard UUID(uuidString: pending.id) != nil,
              pending.url.lastPathComponent == pending.id + ".bin" else { throw ManagedBackupError.invalidRecord }
        let directory = pending.url.deletingLastPathComponent()
        let logURL = directory.appendingPathComponent(pending.id + ".log")
        try writePrivate(Data(processOutput.utf8), to: logURL)
        let hashes = processOutput.components(separatedBy: "\n").filter { $0.hasPrefix("Backup SHA256: ") }.map { String($0.dropFirst(15)) }
        if hashes.isEmpty { return nil }
        guard hashes.count == 1, validHash(hashes[0]) else { throw ManagedBackupError.incompleteBackup }
        if let firmware, firmware.component != .scaler {
            return try completeUSB(pending, monitor: monitor, firmware: firmware, outputHash: hashes[0])
        }
        let raw = try readRaw(pending.url)
        guard hash(raw) == hashes[0] else { throw ManagedBackupError.integrityFailed }
        let intent = try readIntent(directory.appendingPathComponent(pending.id + ".pending.json"))
        guard intent.id == pending.id, intent.createdAt == pending.createdAt, intent.monitor == monitor,
              intent.requestedFirmwareID == firmware?.id else { throw ManagedBackupError.invalidRecord }
        let receipt = try verifiedReceipt(for: pending.url, raw: raw)
        let source = processOutput.components(separatedBy: "\n").first { $0.hasPrefix("Current FW2: ") }
        let sourceHash = source?.components(separatedBy: "SHA256 ").last ?? ""
        guard sourceHash == receipt.sourceSHA256, hashes[0] == receipt.rawSHA256 else { throw ManagedBackupError.incompleteBackup }
        let saved = record(intent: intent, receipt: receipt)
        try commit(saved, in: directory)
        return saved
    }

    private struct USBReceipt: Codable {
        let raw_sha256: String
        let bytes: Int
        let controller: String
        let jedec: String
    }
    private static func completeUSB(_ pending: PendingBackup, monitor: MonitorConnection, firmware: FirmwareRelease, outputHash: String) throws -> SavedBackup {
        let raw = try readPrivate(pending.url, maximumBytes: 1024 * 1024)
        let receipt = try JSONDecoder().decode(USBReceipt.self, from: readPrivate(URL(fileURLWithPath: pending.url.path + ".receipt.json"), maximumBytes: 4096))
        guard [0x40000, 0x80000, 0x100000].contains(raw.count), receipt.bytes == raw.count,
              receipt.controller == "VL822Q7+VL103", receipt.jedec.count == 6,
              receipt.raw_sha256 == outputHash, hash(raw) == outputHash else { throw ManagedBackupError.integrityFailed }
        let directory = pending.url.deletingLastPathComponent()
        let intent = try readIntent(directory.appendingPathComponent(pending.id + ".pending.json"))
        guard intent.id == pending.id, intent.createdAt == pending.createdAt, intent.monitor == monitor,
              intent.requestedFirmwareID == firmware.id else { throw ManagedBackupError.invalidRecord }
        let saved = SavedBackup(id: pending.id, sha256: outputHash, createdAt: pending.createdAt,
            firmwareTitle: "Shared USB hub / PD SPI (" + receipt.jedec + ")", sourceFirmwareSHA256: "",
            monitorSerial: monitor.serial, monitorProduct: monitor.product, locationID: monitor.locationID,
            requestedFirmwareID: firmware.id, bytes: raw.count)
        try commit(saved, in: directory)
        return saved
    }

    static func list(for monitor: MonitorConnection, root: URL? = nil) throws -> [SavedBackup] {
        // Location IDs identify ports, not monitors. Never use them as a silent
        // substitute for a unit serial when offering a device-specific restore.
        guard usableSerial(monitor.serial) else { return [] }
        let location = try directory(root: root)
        try recoverPending(for: monitor, in: location)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return try FileManager.default.contentsOfDirectory(at: location, includingPropertiesForKeys: nil)
            .filter { $0.pathExtension == "json" }
            .compactMap { path -> SavedBackup? in
                guard let data = try? readPrivate(path, maximumBytes: 16 * 1024), let item = try? decoder.decode(SavedBackup.self, from: data),
                      UUID(uuidString: item.id) != nil, path.lastPathComponent == item.id + ".json",
                      item.monitorSerial == monitor.serial, item.monitorProduct == monitor.product,
                      item.bytes == rawByteCount, validHash(item.sha256), validHash(item.sourceFirmwareSHA256),
                      FileManager.default.fileExists(atPath: location.appendingPathComponent(item.id + ".bin").path) else { return nil }
                return item
            }.sorted { $0.createdAt > $1.createdAt }
    }

    static func validateForRestore(_ saved: SavedBackup, monitor: MonitorConnection, root: URL? = nil) throws -> URL {
        guard usableSerial(monitor.serial) else { throw ManagedBackupError.identityUnavailable }
        guard saved.monitorSerial == monitor.serial, saved.monitorProduct == monitor.product else { throw ManagedBackupError.wrongMonitor }
        guard UUID(uuidString: saved.id) != nil, saved.bytes == rawByteCount,
              validHash(saved.sha256), validHash(saved.sourceFirmwareSHA256) else { throw ManagedBackupError.invalidRecord }
        let url = try directory(root: root).appendingPathComponent(saved.id + ".bin")
        let raw = try readRaw(url)
        guard hash(raw) == saved.sha256, try embeddedImageHash(raw) == saved.sourceFirmwareSHA256 else { throw ManagedBackupError.integrityFailed }
        return url
    }

    static func usableSerial(_ serial: String) -> Bool {
        let normalized = serial.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        let digits = normalized.filter { $0.isLetter || $0.isNumber }
        return !normalized.isEmpty && !digits.isEmpty && !digits.allSatisfy({ $0 == "0" })
            && digits != "0000000000000001" && !digits.allSatisfy({ $0 == "f" }) && !["unknown", "none", "null", "n/a"].contains(normalized)
    }
    private static func validHash(_ value: String) -> Bool {
        value.count == 64 && value.utf8.allSatisfy { (48...57).contains($0) || (97...102).contains($0) }
    }
    private static func hash(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
    private static func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        return try encoder.encode(value)
    }
    private static func readIntent(_ url: URL) throws -> PendingIntent {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let intent = try decoder.decode(PendingIntent.self, from: readPrivate(url, maximumBytes: 16 * 1024))
        guard UUID(uuidString: intent.id) != nil,
              url.lastPathComponent == intent.id + ".pending.json" else { throw ManagedBackupError.invalidRecord }
        return intent
    }
    private static func verifiedReceipt(for url: URL, raw: Data) throws -> BackupReceipt {
        let receiptURL = URL(fileURLWithPath: url.path + ".receipt.json")
        let receipt = try JSONDecoder().decode(BackupReceipt.self, from: readPrivate(receiptURL, maximumBytes: 4096))
        guard receipt.bytes == rawByteCount, validHash(receipt.rawSHA256), validHash(receipt.sourceSHA256),
              receipt.rawSHA256 == hash(raw), try embeddedImageHash(raw) == receipt.sourceSHA256 else {
            throw ManagedBackupError.integrityFailed
        }
        return receipt
    }
    private static func record(intent: PendingIntent, receipt: BackupReceipt) -> SavedBackup {
        let sourceName = FirmwareCatalog.entries.first(where: { $0.sha256 == receipt.sourceSHA256 })?.title
            ?? (receipt.sourceSHA256 == "d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3" ? "Stock Beta03" : "Previous ES07D03 firmware")
        return SavedBackup(id: intent.id, sha256: receipt.rawSHA256, createdAt: intent.createdAt,
            firmwareTitle: sourceName, sourceFirmwareSHA256: receipt.sourceSHA256,
            monitorSerial: intent.monitor.serial, monitorProduct: intent.monitor.product,
            locationID: intent.monitor.locationID, requestedFirmwareID: intent.requestedFirmwareID, bytes: rawByteCount)
    }
    private static func commit(_ saved: SavedBackup, in directory: URL) throws {
        try writePrivate(try encode(saved), to: directory.appendingPathComponent(saved.id + ".json"))
        // Keep the pending intent until the completed index has been durably
        // written. A leftover intent is harmless and can be recovered again.
        try? FileManager.default.removeItem(at: directory.appendingPathComponent(saved.id + ".pending.json"))
    }
    private static func recoverPending(for monitor: MonitorConnection, in directory: URL) throws {
        for path in try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil)
            where path.lastPathComponent.hasSuffix(".pending.json") {
            do {
                let intent = try readIntent(path)
                guard intent.monitor.serial == monitor.serial, intent.monitor.product == monitor.product else { continue }
                let rawURL = directory.appendingPathComponent(intent.id + ".bin")
                let raw = try readRaw(rawURL)
                let receipt = try verifiedReceipt(for: rawURL, raw: raw)
                try commit(record(intent: intent, receipt: receipt), in: directory)
            } catch {
                // Retain incomplete or invalid files for diagnosis. Never turn
                // a missing receipt, damaged file or opaque rescue into a
                // selectable restore backup merely because a .bin exists.
                continue
            }
        }
    }
    private static func embeddedImageHash(_ raw: Data) throws -> String {
        guard raw.count == rawByteCount else { throw ManagedBackupError.incompleteBackup }
        let offset = 0x10020
        let footer = (0..<4).reduce(0) { $0 | (Int(raw[offset + $1]) << (8 * $1)) }
        guard footer >= 0x33880, footer <= raw.count - 4 else { throw ManagedBackupError.incompleteBackup }
        return hash(Data(raw.prefix(footer + 4)))
    }
    private static func readRaw(_ url: URL) throws -> Data {
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)
        guard fd >= 0 else { throw ManagedBackupError.incompleteBackup }
        var st = stat()
        guard fstat(fd, &st) == 0, (st.st_mode & S_IFMT) == S_IFREG, st.st_uid == getuid(), st.st_nlink == 1, st.st_size == rawByteCount else {
            close(fd); throw ManagedBackupError.incompleteBackup
        }
        let file = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        let data = try file.readToEnd() ?? Data()
        guard data.count == rawByteCount else { throw ManagedBackupError.incompleteBackup }
        return data
    }
    private static func readPrivate(_ url: URL, maximumBytes: Int) throws -> Data {
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)
        guard fd >= 0 else { throw ManagedBackupError.incompleteBackup }
        var st = stat()
        guard fstat(fd, &st) == 0, (st.st_mode & S_IFMT) == S_IFREG,
              st.st_uid == getuid(), st.st_nlink == 1, st.st_size > 0, st.st_size <= maximumBytes else {
            close(fd); throw ManagedBackupError.incompleteBackup
        }
        let file = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        let data = try file.readToEnd() ?? Data()
        guard data.count == st.st_size else { throw ManagedBackupError.incompleteBackup }
        return data
    }
    private static func writePrivate(_ data: Data, to url: URL) throws {
        try data.write(to: url, options: [.atomic])
        guard chmod(url.path, 0o600) == 0 else { throw ManagedBackupError.storageUnavailable }
        let fd = open(url.path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)
        guard fd >= 0 else { throw ManagedBackupError.storageUnavailable }
        var st = stat()
        let valid = fstat(fd, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG && st.st_uid == getuid()
            && st.st_nlink == 1 && st.st_size == data.count
        let synced = valid && fsync(fd) == 0 && fcntl(fd, F_FULLFSYNC) == 0
        close(fd)
        guard synced else { throw ManagedBackupError.storageUnavailable }
        let parent = open(url.deletingLastPathComponent().path, O_RDONLY | O_DIRECTORY | O_CLOEXEC)
        guard parent >= 0 else { throw ManagedBackupError.storageUnavailable }
        let directorySynced = fsync(parent) == 0
        close(parent)
        guard directorySynced else { throw ManagedBackupError.storageUnavailable }
    }
}

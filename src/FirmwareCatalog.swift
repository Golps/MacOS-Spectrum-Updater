import Foundation
import CryptoKit

enum UpdateComponent: String, CaseIterable {
    case scaler = "Scaler + included HDMI/bridge"
    case hub = "USB hub"
    case pd = "USB-C / Power Delivery"
}

struct FirmwareRelease: Identifiable, Equatable {
    let id: String
    let title: String
    let version: String
    let summary: String
    let isExperimental: Bool
    let sha256: String
    let filename: String
    let byteCount: Int
    let models: [String]
    var displayVersion: String { version }
    var component: UpdateComponent { id.hasPrefix("hub-") ? .hub : id.hasPrefix("pd-") ? .pd : .scaler }
    var canFlash: Bool { !models.isEmpty }
}

enum FirmwareCatalogError: Error, LocalizedError {
    case resourcesMissing, fileUnreadable, sizeMismatch, hashMismatch, inspectionFailed, unknownFirmware
    var errorDescription: String? {
        switch self {
        case .resourcesMissing: return "Choose a vendor firmware binary to begin."
        case .fileUnreadable: return "The selected firmware cannot be read."
        case .sizeMismatch: return "The firmware length does not match its release profile."
        case .hashMismatch: return "The firmware changed after selection. Choose the original file again."
        case .inspectionFailed: return "The firmware did not pass its independent integrity checks."
        case .unknownFirmware: return "This file is not in the supported vendor-release catalog. Choose an original vendor ZIP, binary or USB installer for a listed IPS model. OLED and custom firmware are not supported."
        }
    }
}

enum FirmwareCatalog {
    static let placeholder = FirmwareRelease(id: "none", title: "No firmware selected", version: "", summary: "Choose an original vendor ZIP, .bin file or USB installer. Firmware is not bundled with this app.", isExperimental: false, sha256: "", filename: "", byteCount: 0, models: [])
    static let entries: [FirmwareRelease] = [
        .init(id: "beta03", title: "V108 Beta 3", version: "D03.V108T", summary: "Vendor Beta03, revision 1169-E771. Includes scaler and secondary components; no trailing Processing update payload.", isExperimental: false, sha256: "d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3", filename: "STOCK_ES07D03_Beta03.bin", byteCount: 1344628, models: ["ES07D03", "ES07DC9", "ES07E30"]),
        .init(id: "v108", title: "V108", version: "V108", summary: "Vendor revision 1148. Includes the trailing secondary-update payload.", isExperimental: false, sha256: "136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757", filename: "STOCK_ES07D03_V108.bin", byteCount: 2069060, models: ["ES07D03", "ES07E30"]),
        .init(id: "v106", title: "V106", version: "V106", summary: "Original ES07D03 vendor release.", isExperimental: false, sha256: "199bb51c3d31023ffb37542acc250af4bd66cc7059f9a221697ce9c6c5b27ab1", filename: "v106.bin", byteCount: 2056388, models: ["ES07D03"]),
        .init(id: "v105", title: "V105", version: "V105", summary: "Original ES07D03 vendor release.", isExperimental: false, sha256: "536d94f761d84fa7d8b616dbf5442b81a2d470f2cb7112cba213bc4fbd10add9", filename: "v105.bin", byteCount: 2056020, models: ["ES07D03"]),
        .init(id: "dc9-v101", title: "Glossy V101", version: "V101", summary: "Original ES07DC9 vendor revision 1148; includes secondary-update payload.", isExperimental: false, sha256: "0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9", filename: "dc9-v101.bin", byteCount: 2069060, models: ["ES07DC9"]),
        .init(id: "d02-v101", title: "QHD V101", version: "D02.V101", summary: "Original ES07D02 vendor revision 1148; includes secondary-update payload.", isExperimental: false, sha256: "5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162", filename: "d02-v101.bin", byteCount: 2014868, models: ["ES07D02"]),
        .init(id: "d02-beta01", title: "QHD Beta 1", version: "D02 Beta 1", summary: "Original ES07D02 vendor revision 1167; includes secondary-update payload.", isExperimental: false, sha256: "e319da9490df8cec4ff94677385e57a560899a9ef335b6dc7e9b052bf3bcc9f0", filename: "d02-beta01.bin", byteCount: 2014868, models: ["ES07D02"]),
        .init(id: "hub-06a4", title: "USB hub 06A4", version: "06A4", summary: "Vendor VL822Q7 USB hub image. Updates the hub through its shared SPI flash.", isExperimental: false, sha256: "ec6699214c621671449ec941ab4ccd8c413cb79b6e369d99a654c12beb3a3356", filename: "hub.bin", byteCount: 42556, models: ["ES07D03", "ES07DC9", "ES07E30", "ES07D02"]),
        .init(id: "pd-1902", title: "USB-C / PD 19.02", version: "0A.89.19.02", summary: "Vendor VL103 USB-C / Power Delivery image. Preserves the USB hub firmware.", isExperimental: false, sha256: "62879652a96b8098cb240c4a4290828304afc6709b3948066ccb0d1de49e0fe1", filename: "pd.bin", byteCount: 32768, models: ["ES07D03", "ES07DC9", "ES07E30", "ES07D02"]),
        .init(id: "pd-1702", title: "USB-C / PD 17.02", version: "0A.89.17.02", summary: "Earlier Vendor VL103 USB-C / Power Delivery image. Preserves the USB hub firmware.", isExperimental: false, sha256: "3e3c4c2224c676b1e4971280f5678f65b37ddb8609c3afdfa1df6f72e14b44b1", filename: "pd1702.bin", byteCount: 32768, models: ["ES07D03", "ES07DC9", "ES07E30", "ES07D02"])
    ]
    static var beta03: FirmwareRelease { entries[0] }
    static func identify(_ url: URL) throws -> FirmwareRelease {
        guard url.isFileURL, let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]), values.isRegularFile == true, let size = values.fileSize, size > 0, size <= 4194304 else { throw FirmwareCatalogError.fileUnreadable }
        guard let data = try? Data(contentsOf: url), data.count == size else { throw FirmwareCatalogError.fileUnreadable }
        let hash = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
        guard let release = entries.first(where: { $0.sha256 == hash && $0.byteCount == size }) else { throw FirmwareCatalogError.unknownFirmware }
        return release
    }
    static func validate(_ entry: FirmwareRelease, url: URL) throws -> URL {
        guard try identify(url) == entry else { throw FirmwareCatalogError.hashMismatch }
        return url
    }
    static func load(_ entry: FirmwareRelease, resources: URL?) throws -> URL {
        guard let resources else { throw FirmwareCatalogError.resourcesMissing }
        return try validate(entry, url: resources.appendingPathComponent("Firmware").appendingPathComponent(entry.filename))
    }
    static func confirmInspection(_ entry: FirmwareRelease, exitStatus: Int32, output: String) throws -> String {
        let hashes = output.components(separatedBy: "\n").filter { $0.hasPrefix("SHA256: ") }
        guard entries.contains(entry), entry.canFlash, exitStatus == 0, hashes == ["SHA256: " + entry.sha256] else { throw FirmwareCatalogError.inspectionFailed }
        return entry.sha256
    }
}

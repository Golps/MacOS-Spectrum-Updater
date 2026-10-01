import Foundation
import CryptoKit
import Compression

// Only pinned original vendor packages are unpacked. Windows installers are
// treated as data; they are never executed. Payloads retain their original SHA.
enum VendorImport {
    struct Item { let release: FirmwareRelease; let data: Data }
    private struct Archive { let sha256: String; let bytes: Int; let member: String }
    private static let archives: [Archive] = [
        .init(sha256: "d95de1784ceb799e92ea5cffaf7acf1aae0a64011f4915bab61e47b464d07cca", bytes: 3561078, member: "BD_EVE_4K_Release_20211126_V105_rev957_USB update.bin"),
        .init(sha256: "f0040fd38cb1785afbf02ec742f8c424b4aaebdeff9a7bffb97f797df7176f91", bytes: 4085730, member: "BD_EVE_4K_Release_20220216_V106_rev988_USB update.bin"),
        .init(sha256: "06eafe9eb83a117a22002832efce255902ccdccefae07a20e5ed44b2d11a19f8", bytes: 4090085, member: "ES07D03_V108_rev1148_USB update.bin"),
        .init(sha256: "b227596f273214f00de3176f40fdbbd97d0d6e5216cb1fd02f6bf63b14772db0", bytes: 3894825, member: "spectrum_es07d03_scaler_fw_1169-E771_Beta03/ES07D03_Beta03_rev1169-E771_USB_update.bin"),
        .init(sha256: "c734affb14d5ca50756beba91d7c511d76f52e91dbc986d9a1d0cbb0dcbca7fa", bytes: 1166758, member: "VL822_06A4_VL103R_89_17_02_Lehui_20210804.exe"),
        .init(sha256: "13605e7587f2f4269b9d67c0bc253e159566b20395be5979f8a8b8883d14b19b", bytes: 1203109, member: "VL822_06A4_VL103R_89_19_02_Lehui_20220615.exe"),
        .init(sha256: "8a09f42099596e3d5f90451db562c0a9fd7c79ba7fa77a7875eb9462db4a7cd9", bytes: 4053346, member: "ES07DC9_V101_rev1148_USB update.bin"),
        .init(sha256: "fcb298863617ec1194a63cf54bbd74d77562ebbbabc343e0a6566442c48b7f63", bytes: 3995303, member: "ES07D02_V101_rev1148_USB update.bin"),
        .init(sha256: "eb6d1dda4791554fdfbcb93f106e88dd95737f33f215780f4f132520efc8a555", bytes: 3996150, member: "ES07D02_Beta01_rev1167_USB_update.bin"),
    ]
    private struct Payload { let offset: Int; let compressed: Int; let bytes: Int; let id: String }
    private struct Installer { let sha256: String; let bytes: Int; let payloads: [Payload] }
    private static let installers: [Installer] = [
        .init(sha256: "123035bc7afe8c8a9755b0e53448b926380c1fad3955c3d52e422a62bdc3bd55", bytes: 922630, payloads: [
            .init(offset: 0x5044b, compressed: 18113, bytes: 42556, id: "hub-06a4"),
            .init(offset: 0x54b13, compressed: 15988, bytes: 32768, id: "pd-1902")]),
        .init(sha256: "4f2386dd49350101a47dcab208fc8ce4118bd8c4afd62e1e29a9663d9be21e79", bytes: 921120, payloads: [
            .init(offset: 0x4fe05, compressed: 18113, bytes: 42556, id: "hub-06a4"),
            .init(offset: 0x544cd, compressed: 16079, bytes: 32768, id: "pd-1702")])
    ]
    private static func hash(_ data: Data) -> String { SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined() }
    static func load(_ url: URL) throws -> [Item] {
        guard let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]), values.isRegularFile == true,
              let count = values.fileSize, count > 0, count <= 16 * 1024 * 1024 else { throw FirmwareCatalogError.fileUnreadable }
        let data = try Data(contentsOf: url)
        guard data.count == count else { throw FirmwareCatalogError.fileUnreadable }
        return try unpack(data)
    }
    static func unpack(_ data: Data) throws -> [Item] {
        let sha = hash(data)
        if let release = FirmwareCatalog.entries.first(where: { $0.sha256 == sha && $0.byteCount == data.count }) {
            return [Item(release: release, data: data)]
        }
        if let installer = installers.first(where: { $0.sha256 == sha && $0.bytes == data.count }) {
            return try installer.payloads.map { payload in
                guard payload.offset > 0, payload.offset <= data.count, payload.compressed <= data.count-payload.offset,
                      data[payload.offset-1] == 0x16,
                      let release = FirmwareCatalog.entries.first(where: { $0.id == payload.id }) else { throw FirmwareCatalogError.inspectionFailed }
                let stream = xz(Data(data[payload.offset..<payload.offset+payload.compressed]), bytes: payload.bytes)
                var decoded = [UInt8](repeating: 0, count: payload.bytes + 1)
                let count = stream.withUnsafeBytes { source in decoded.withUnsafeMutableBytes { dest in
                    compression_decode_buffer(dest.bindMemory(to: UInt8.self).baseAddress!, dest.count,
                        source.bindMemory(to: UInt8.self).baseAddress!, source.count, nil, COMPRESSION_LZMA)
                } }
                let binary = Data(decoded.prefix(count))
                guard count == release.byteCount, hash(binary) == release.sha256 else { throw FirmwareCatalogError.inspectionFailed }
                return Item(release: release, data: binary)
            }
        }
        if let archive = archives.first(where: { $0.sha256 == sha && $0.bytes == data.count }) {
            let folder = FileManager.default.temporaryDirectory.appendingPathComponent("SpectrumPackage-" + UUID().uuidString)
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
            defer { try? FileManager.default.removeItem(at: folder) }
            let snapshot = folder.appendingPathComponent("package.zip")
            try data.write(to: snapshot, options: .withoutOverwriting)
            let process = Process(), output = Pipe(), errors = Pipe()
            process.executableURL = URL(fileURLWithPath: "/usr/bin/unzip")
            process.arguments = ["-p", snapshot.path, archive.member]
            process.standardInput = FileHandle.nullDevice
            process.standardOutput = output; process.standardError = errors
            try process.run()
            var binary = Data()
            while let chunk = try output.fileHandleForReading.read(upToCount: 65536), !chunk.isEmpty {
                guard binary.count + chunk.count <= 4 * 1024 * 1024 else { process.terminate(); process.waitUntilExit(); throw FirmwareCatalogError.inspectionFailed }
                binary.append(chunk)
            }
            process.waitUntilExit()
            guard process.terminationStatus == 0 else { throw FirmwareCatalogError.inspectionFailed }
            return try unpack(binary)
        }
        throw FirmwareCatalogError.unknownFirmware
    }
    // Original XZ framing for the cataloged raw LZMA2 streams (8 MiB dictionary).
    // Apple's built-in Compression framework supplies the decoder. Check type 0
    // is valid XZ; exact vendor output SHA256 is mandatory after decoding.
    private static func crc(_ data: Data) -> Data {
        var c: UInt32 = 0xffffffff
        for byte in data { c ^= UInt32(byte); for _ in 0..<8 { c = (c >> 1) ^ ((c & 1) == 0 ? 0 : 0xedb88320) } }
        return little(c ^ 0xffffffff)
    }
    private static func little(_ n: UInt32) -> Data { Data((0..<4).map { UInt8(truncatingIfNeeded: n >> (8 * $0)) }) }
    private static func variable(_ number: Int) -> Data {
        var n = number, data = Data()
        repeat { var byte = UInt8(n & 127); n >>= 7; if n != 0 { byte |= 128 }; data.append(byte) } while n != 0
        return data
    }
    private static func padded(_ data: Data) -> Data { var d = data; while d.count % 4 != 0 { d.append(0) }; return d }
    private static func xz(_ raw: Data, bytes: Int) -> Data {
        let flags = Data([0, 0])
        var stream = Data([0xfd, 0x37, 0x7a, 0x58, 0x5a, 0]); stream.append(flags); stream.append(crc(flags))
        var header = Data([2, 0, 0x21, 1, 0x16, 0, 0, 0]); header.append(crc(header))
        var block = header; block.append(raw); stream.append(padded(block))
        var index = Data([0, 1]); index.append(variable(header.count + raw.count)); index.append(variable(bytes)); index = padded(index); index.append(crc(index)); stream.append(index)
        var footer = little(UInt32(index.count / 4 - 1)); footer.append(flags)
        stream.append(crc(footer)); stream.append(footer); stream.append(contentsOf: [0x59, 0x5a])
        return stream
    }
}

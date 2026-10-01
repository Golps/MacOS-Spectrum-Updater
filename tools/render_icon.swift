import AppKit

@main enum RenderIcon {
    static func main() throws {
        let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        var representations = Data()
        for size in [16, 32, 128, 256, 512] {
            for factor in [1, 2] {
                let pixels = size * factor
                let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
                NSGraphicsContext.saveGraphicsState()
                NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
                SpectrumBrand.draw(in: NSRect(x: 0, y: 0, width: pixels, height: pixels))
                NSGraphicsContext.restoreGraphicsState()
                let suffix = factor == 2 ? "@2x" : ""
                let png = bitmap.representation(using: .png, properties: [:])!
                try png.write(to: directory.appendingPathComponent("icon_\(size)x\(size)\(suffix).png"))
                let type: String?
                switch (size, factor) {
                case (16, 1): type = "icp4"
                case (32, 1): type = "icp5"
                case (32, 2): type = "icp6"
                case (128, 1): type = "ic07"
                case (256, 1): type = "ic08"
                case (512, 1): type = "ic09"
                case (512, 2): type = "ic10"
                default: type = nil
                }
                if let type {
                    representations.append(contentsOf: type.utf8)
                    var length = UInt32(png.count + 8).bigEndian
                    withUnsafeBytes(of: &length) { representations.append(contentsOf: $0) }
                    representations.append(png)
                }
            }
        }
        var icon = Data("icns".utf8)
        var length = UInt32(representations.count + 8).bigEndian
        withUnsafeBytes(of: &length) { icon.append(contentsOf: $0) }
        icon.append(representations)
        try icon.write(to: URL(fileURLWithPath: CommandLine.arguments[2]))
    }
}

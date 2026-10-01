import AppKit

// Original vector artwork shared by the application and its exported app icon.
enum SpectrumBrand {
    static func draw(in rect: NSRect) {
        NSGraphicsContext.saveGraphicsState()
        let transform = NSAffineTransform()
        transform.translateX(by: rect.minX, yBy: rect.minY)
        transform.scale(by: rect.width / 1024)
        transform.concat()
        let tile = NSBezierPath(roundedRect: NSRect(x: 32, y: 32, width: 960, height: 960), xRadius: 214, yRadius: 214)
        NSGradient(colors: [NSColor(srgbRed: 0.12, green: 0.23, blue: 0.44, alpha: 1), NSColor(srgbRed: 0.035, green: 0.07, blue: 0.16, alpha: 1)])!.draw(in: tile, angle: -70)
        let screen = NSBezierPath(roundedRect: NSRect(x: 196, y: 324, width: 632, height: 432), xRadius: 48, yRadius: 48)
        NSColor(srgbRed: 0.8, green: 0.9, blue: 1, alpha: 1).setStroke()
        screen.lineWidth = 22
        screen.stroke()
        let colors: [NSColor] = [.systemPurple, .systemBlue, .systemTeal, .systemGreen, .systemYellow, .systemOrange]
        for (index, color) in colors.enumerated() {
            color.setFill()
            NSBezierPath(roundedRect: NSRect(x: 244 + index * 91, y: 358, width: 82, height: 34), xRadius: 12, yRadius: 12).fill()
        }
        let arrow = NSBezierPath()
        arrow.move(to: NSPoint(x: 512, y: 665))
        arrow.line(to: NSPoint(x: 512, y: 466))
        arrow.move(to: NSPoint(x: 427, y: 551))
        arrow.line(to: NSPoint(x: 512, y: 466))
        arrow.line(to: NSPoint(x: 597, y: 551))
        arrow.lineWidth = 30
        arrow.lineCapStyle = .round
        arrow.lineJoinStyle = .round
        NSColor.white.setStroke()
        arrow.stroke()
        let stand = NSBezierPath()
        stand.move(to: NSPoint(x: 512, y: 314))
        stand.line(to: NSPoint(x: 512, y: 234))
        stand.move(to: NSPoint(x: 410, y: 234))
        stand.line(to: NSPoint(x: 614, y: 234))
        stand.lineWidth = 22
        stand.lineCapStyle = .round
        NSColor(srgbRed: 0.8, green: 0.9, blue: 1, alpha: 1).setStroke()
        stand.stroke()
        NSGraphicsContext.restoreGraphicsState()
    }
}

final class SpectrumMarkView: NSView {
    override func draw(_ dirtyRect: NSRect) { SpectrumBrand.draw(in: bounds) }
    override var intrinsicContentSize: NSSize { NSSize(width: 64, height: 64) }
}

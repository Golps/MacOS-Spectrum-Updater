import Foundation
@main enum CatalogTests {
 static func main() throws {
  let catalog = FirmwareCatalog.entries
  precondition(catalog.count == 10)
  precondition(!catalog.contains { $0.isExperimental || $0.sha256 == "461993270a3a4458e38f5d10d157ce5b091bd5db2beb6baec78b708aaa61dd7a" })
  for entry in catalog {
   precondition(entry.sha256.count == 64 && entry.byteCount > 0)
   precondition(entry.canFlash)
  }
  precondition(FirmwareCatalog.beta03.models == ["ES07D03", "ES07DC9", "ES07E30"])
  let original = URL(fileURLWithPath: "resources/Firmware/STOCK_ES07D03_Beta03.bin")
  if FileManager.default.fileExists(atPath: original.path) {
    let identified = try FirmwareCatalog.identify(original)
    precondition(identified == FirmwareCatalog.beta03)
    let scratch = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
    defer { try? FileManager.default.removeItem(at: scratch) }
    var bytes = try Data(contentsOf: original)
    bytes[30] ^= 1
    try bytes.write(to: scratch)
    do { _ = try FirmwareCatalog.identify(scratch); fatalError("Modified firmware accepted") } catch FirmwareCatalogError.unknownFirmware { }
    print("Stock import and corrupt-file rejection passed.")
  } else { print("Stock import fixture checks skipped; run tools/fetch_test_fixtures.py to supply vendor fixtures.") }
  print("Catalog routes, model compatibility and stock-only policy passed. No hardware access.")
 }
}

import Foundation
@main enum ImportTests {
 static func main() throws {
  var assertions = 0
  for name in ["v105", "v106", "v108", "beta03", "dc9-v101", "d02-v101", "d02-beta01", "usb1702", "usb1902"] {
   let url=URL(fileURLWithPath:"tests/fixtures/"+name+".zip")
   let data=try Data(contentsOf:url)
   let items=try VendorImport.load(url)
   precondition(items.count == (name.hasPrefix("usb") ? 2 : 1)); assertions += 1
   for item in items { precondition(item.data.count == item.release.byteCount); assertions += 1 }
   var corrupt=data;corrupt[corrupt.count/2] ^= 1
   do { _ = try VendorImport.unpack(corrupt); fatalError("Corrupt archive accepted") } catch FirmwareCatalogError.unknownFirmware { assertions += 1 }
   do { _ = try VendorImport.unpack(Data(data.dropLast())); fatalError("Truncated archive accepted") } catch FirmwareCatalogError.unknownFirmware { assertions += 1 }
  }
  print("{\"assertions\":\(assertions),\"hardware_access\":false,\"vendor_packages\":9}")
 }
}

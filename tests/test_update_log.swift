import Foundation
@main enum LogTests {
 static func main() throws {
  var checks = 0
  func require(_ value: Bool) { precondition(value); checks += 1 }
  var formatter = UpdateLogFormatter()
  require(formatter.ingest("Image: /Volumes/Fixture/Folder With Spaces/update.bin\nReading 3").isEmpty)
  let events = formatter.ingest("2/64\nValidated: CRC32\nUSB failure 0xe000\nBackup: /private/tmp/backup.bin\nBackup SHA256: abc\n")
  require(events.count == 4)
  require(events[0].text == "Reading firmware · 50%" && events[0].tone == .information)
  require(events[1].tone == .success && events[2].tone == .error && events[3].tone == .success)
  require(!events.contains { $0.text.contains("/Volumes/") || $0.text.contains("/private/") })
  require(UpdateLogFormatter.event("[{\"registry_id\":\"1\",\"product\":\"VIA\"}]")?.text == "USB connection found (1)")
  require(UpdateLogFormatter.event("[]")?.text == "No compatible USB connection found")
  require(UpdateLogFormatter.safe("Cannot read \"/Volumes/Fixture/Some Folder/file.bin\" safely.") == "Cannot read  [file] safely.")
  require(!UpdateLogFormatter.safe("Failure at /Volumes/Fixture/Some Folder/file.bin").contains("/Volumes/"))
  require(UpdateLogFormatter.operation("flash-usb").text == "Starting backup and installation")
  require(formatter.ingest("Programming completed").isEmpty)
  require(formatter.ingest("", finish: true).count == 1)
  require(formatter.ingest("", finish: true).isEmpty)
  print("{\"assertions\":\(checks),\"hardware_access\":false,\"result\":\"passed\"}")
 }
}

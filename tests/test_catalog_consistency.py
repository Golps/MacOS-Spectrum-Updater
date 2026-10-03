"""Fail if the Swift catalog, C scaler authorization table, and manifest drift."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / "config/firmware-catalog.json").read_text())["firmware"]

swift = (ROOT / "src/FirmwareCatalog.swift").read_text()
swift_rows = {}
pattern = re.compile(
    r'\.init\(id: "([^"]+)".*?sha256: "([0-9a-f]{64})".*?byteCount: (\d+), models: \[([^\]]*)\]\)'
)
for match in pattern.finditer(swift):
    ident, sha, size, raw_models = match.groups()
    models = re.findall(r'"([^"]+)"', raw_models)
    component = "hub" if ident.startswith("hub-") else "pd" if ident.startswith("pd-") else "scaler"
    swift_rows[ident] = {
        "id": ident, "component": component, "sha256": sha,
        "byteCount": int(size), "models": models, "installable": True,
    }

expected_swift = {row["id"]: row for row in manifest if row["installable"]}
assert swift_rows == expected_swift, (
    "FirmwareCatalog.swift differs from config/firmware-catalog.json\n"
    f"Swift: {swift_rows}\nManifest: {expected_swift}"
)

header = (ROOT / "src/known-firmware.h").read_text()
bits = {
    "SP_MODEL_D03": "ES07D03",
    "SP_MODEL_DC9": "ES07DC9",
    "SP_MODEL_E30": "ES07E30",
    "SP_MODEL_D02": "ES07D02",
}
c_rows = {}
for sha, mask, installable in re.findall(r'\{"([0-9a-f]{64})", ([^,]+), (true|false)\}', header):
    models = [model for token, model in bits.items() if token in mask]
    c_rows[sha] = {"models": models, "installable": installable == "true"}

expected_c = {
    row["sha256"]: {"models": row["models"], "installable": row["installable"]}
    for row in manifest if row["component"] == "scaler"
}
assert c_rows == expected_c, (
    "known-firmware.h differs from config/firmware-catalog.json\n"
    f"C: {c_rows}\nManifest: {expected_c}"
)

print(f"Catalog consistency passed: {len(swift_rows)} installable releases and {len(c_rows)} scaler authorization profiles.")

"""Verify pinned UI copies and documented local port adaptations, read-only."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1] / "Middleware"
count = 0
for name in ("u8g2-subset-manifest.json", "showcase-source-manifest.json"):
    manifest = json.loads((root / name).read_text())
    for entry in manifest["files"]:
        expected = entry.get("local_sha256", entry["sha256"])
        path = root / entry["path"]
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit("Hash mismatch: " + str(path))
        count += 1
print(f"PASS: {count} UI source/license/port files match manifests")

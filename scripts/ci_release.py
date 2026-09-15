"""CI release packaging: copy firmware binaries, compute SHA-256, emit qymera-manifest.json.

Expected layout:
  .pio/build/esp8266_generic/firmware.bin  -> Qymera-<V>-esp8266.bin
  .pio/build/esp32_devkit/firmware.bin     -> Qymera-<V>-esp32.bin
  .pio/build/esp32c3_devkit/firmware.bin   -> Qymera-<V>-esp32c3.bin

Manifest (consumed by the device Web OTA):
  {
    "product": "Qymera",
    "version": "<V>",
    "channel": "stable",
    "esp8266":  { "url": "...", "sha256": "<64hex>", "size": <N> },
    "esp32":    { ... },
    "esp32c3":  { ... }
  }
Run: python scripts/ci_release.py v1.0.0
"""
import hashlib
import json
import os
import shutil
import sys

VERSION = (sys.argv[1] if len(sys.argv) > 1 else "0.0.0").lstrip("v")
PRODUCT = "Qymera"
CHANNEL = "stable"
REPO = os.environ.get("GITHUB_REPOSITORY", "gonreyna85code/Qymera")
BASE_URL = f"https://github.com/{REPO}/releases/latest/download"

ENVS = {
    "esp8266": "esp8266_generic",
    "esp32": "esp32_devkit",
    "esp32c3": "esp32c3_devkit",
}

DIST = os.path.join("dist")
os.makedirs(DIST, exist_ok=True)

manifest = {
    "product": PRODUCT,
    "version": VERSION,
    "channel": CHANNEL,
}

for tag, env in ENVS.items():
    src = os.path.join(".pio", "build", env, "firmware.bin")
    if not os.path.exists(src):
        print(f"ERROR: missing {src}")
        sys.exit(1)
    dst = os.path.join(DIST, f"{PRODUCT}-{VERSION}-{tag}.bin")
    shutil.copyfile(src, dst)
    digest = hashlib.sha256(open(src, "rb").read()).hexdigest()
    size = os.path.getsize(src)
    manifest[tag] = {
        "url": f"{BASE_URL}/{os.path.basename(dst)}",
        "sha256": digest,
        "size": size,
    }
    print(f"{dst}: {size} bytes, sha256 {digest[:16]}...")

out = os.path.join(DIST, "qymera-manifest.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2)
print(f"manifest -> {out}")
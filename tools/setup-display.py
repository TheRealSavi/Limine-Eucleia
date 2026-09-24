#!/usr/bin/env python3
"""Install matching QEMU GTK modules in this checkout without root."""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
PREFIX = ROOT / "work/tools"
CACHE = ROOT / "work/packages"


def output(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def main() -> None:
    PREFIX.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    keyrings: list[str] = []
    for name in ("archlinux", "cachyos"):
        source = pathlib.Path(f"/usr/share/pacman/keyrings/{name}.gpg")
        keyring = CACHE / f"{name}.gpg"
        if source.read_bytes().startswith(b"-----BEGIN"):
            subprocess.run(["gpg", "--batch", "--yes", "--dearmor", "--output",
                            str(keyring), str(source)], check=True)
        else:
            keyring.write_bytes(source.read_bytes())
        keyrings.extend(["--keyring", str(keyring)])
    installed = output("pacman", "-Q", "qemu-common").split()[1]
    packages = output("pacman", "-Sp", "--print-format", "%n|%v|%h|%l",
                      "qemu-ui-gtk", "qemu-ui-opengl").splitlines()
    manifest: list[dict[str, str]] = []
    for line in packages:
        name, version, checksum, url = line.split("|")
        if name.startswith("qemu-") and version != installed:
            raise SystemExit("Repository QEMU modules differ from installed QEMU. "
                             "Perform a normal system upgrade before retrying.")
        archive = CACHE / url.rsplit("/", 1)[1]
        if not archive.exists():
            print(f"Downloading {name} {version}", flush=True)
            urllib.request.urlretrieve(url, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
            raise SystemExit(f"Package checksum mismatch: {archive}")
        signature = archive.with_name(archive.name + ".sig")
        urllib.request.urlretrieve(url + ".sig", signature)
        subprocess.run([
            "gpgv", *keyrings,
            str(signature), str(archive),
        ], check=True)
        subprocess.run(["bsdtar", "-xf", str(archive), "-C", str(PREFIX), "usr/"],
                       check=True)
        manifest.append({"name": name, "version": version, "sha256": checksum, "url": url})
    (PREFIX / "packages.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"QEMU display modules installed in {PREFIX}")


if __name__ == "__main__":
    main()

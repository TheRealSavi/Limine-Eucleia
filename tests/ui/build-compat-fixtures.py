#!/usr/bin/env python3
"""Build isolated Linux initramfs and EFI payloads for compatibility checks."""

from __future__ import annotations

import json
import stat
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "work/compatibility/fixtures"


def build() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests/ui/fixtures"
    subprocess.run(["cc", "-std=gnu11", "-Os", "-nostdlib", "-static", "-fno-pie", "-no-pie",
                    "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
                    str(source / "linux-init.c"), "-o", str(OUT / "init")], check=True)
    archive = bytearray()

    def entry(name: str, mode: int, data: bytes = b"", major: int = 0, minor: int = 0) -> None:
        encoded = name.encode() + b"\0"
        fields = [1, mode, 0, 0, 1, 0, len(data), 0, 0, major, minor, len(encoded), 0]
        archive.extend(b"070701" + "".join(f"{value:08x}" for value in fields).encode())
        archive.extend(encoded)
        archive.extend(bytes((-len(archive)) % 4))
        archive.extend(data)
        archive.extend(bytes((-len(archive)) % 4))

    entry("dev", stat.S_IFDIR | 0o755)
    entry("dev/console", stat.S_IFCHR | 0o600, major=5, minor=1)
    entry("init", stat.S_IFREG | 0o755, (OUT / "init").read_bytes())
    entry("TRAILER!!!", 0)
    (OUT / "initramfs.cpio").write_bytes(archive)
    subprocess.run(["clang", "--target=x86_64-unknown-windows", "-std=gnu11", "-ffreestanding",
                    "-fshort-wchar", "-mno-red-zone", "-fno-stack-protector", "-Ipicoefi/inc",
                    "-c", str(source / "chainload.c"), "-o", str(OUT / "chainload.obj")],
                   cwd=ROOT, check=True)
    subprocess.run(["lld-link", "/subsystem:efi_application", "/entry:efi_main", "/nodefaultlib",
                    "/out:" + str(OUT / "chainload.efi"), str(OUT / "chainload.obj")], check=True)
    (OUT / "manifest.json").write_text(json.dumps({"initramfs_bytes": len(archive),
        "efi_bytes": (OUT / "chainload.efi").stat().st_size}, indent=2) + "\n")
    print(OUT)


if __name__ == "__main__":
    build()

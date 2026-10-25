#!/usr/bin/env python3
"""Bundle a Linux server binary and its non-glibc shared libraries."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SYSTEM_LIBRARIES = (
    "libc.so.", "libm.so.", "libpthread.so.", "libdl.so.",
    "librt.so.", "libresolv.so.", "libnss_", "ld-linux",
)


def dependencies(binary: Path) -> dict[str, Path]:
    output = subprocess.check_output(["ldd", str(binary)], text=True)
    missing = re.findall(r"^\s*(\S+)\s+=>\s+not found", output, re.MULTILINE)
    if missing:
        raise RuntimeError(f"Missing shared libraries for {binary}: {', '.join(missing)}")
    found: dict[str, Path] = {}
    for name, path in re.findall(r"^\s*(\S+)\s+=>\s+(/\S+)", output, re.MULTILINE):
        if not name.startswith("/") and not name.startswith(SYSTEM_LIBRARIES):
            found[name] = Path(path)
    return found


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()

    version = (ROOT / "VERSION.txt").read_text().strip()
    executable = args.build_dir / "server" / "nx-3D-server"
    nexilis = args.build_dir / "nexilis" / "libnexilis.so"
    if not executable.is_file() or not nexilis.is_file():
        raise FileNotFoundError("Build the server and Nexilis before packaging")

    with tempfile.TemporaryDirectory(prefix="nx3d-linux-release-") as temp:
        package = Path(temp) / "nx-3D-server"
        package.mkdir()
        shutil.copy2(executable, package / executable.name)
        shutil.copy2(nexilis, package / nexilis.name)
        map_asset = ROOT / "assets/maps/dust2_cs2/map.brush"
        packaged_map = package / "assets/maps/dust2_cs2"
        packaged_map.mkdir(parents=True)
        shutil.copy2(map_asset, packaged_map / map_asset.name)
        for name, source in {**dependencies(executable), **dependencies(nexilis)}.items():
            if name != nexilis.name:
                shutil.copy2(source, package / name)

        # DT_RUNPATH is not inherited by grandchildren; give each copied ELF
        # its own lookup path for libraries in this directory.
        for binary in (path for path in package.iterdir() if path.is_file()):
            subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(binary)], check=True)

        (package / "README.txt").write_text(
            "nx-3D dedicated server for Linux x86-64.\n"
            "Built on Ubuntu 24.04; run ./nx-3D-server in a terminal.\n"
            "The server stops when Enter is pressed.\n"
        )
        archive = ROOT / "dist" / f"nx-3D-server-v{version}-linux-x64.tar.gz"
        archive.parent.mkdir(exist_ok=True)
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(package, arcname=package.name)
        print(f"Created {archive}")


if __name__ == "__main__":
    main()

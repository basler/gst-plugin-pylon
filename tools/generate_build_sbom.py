#!/usr/bin/env python3
"""Generate a per-binary-package SBOM for direct build dependencies."""

import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import quote


# Keep this package-level map aligned with the dependencies used by the Meson
# targets and the public headers installed by packaging/debian/*.install.
BUILD_DEPENDENCIES = {
    "gst-plugin-pylon": {
        "libc6": "linker",
        "libgcc-s1": "linker",
        "libglib2.0-dev": "headers",
        "libgstreamer1.0-dev": "headers-and-linker",
        "libgstreamer-plugins-base1.0-dev": "headers-and-linker",
        "libstdc++6": "linker",
        "pylon-core": "headers-and-linker",
    },
    "gst-plugin-pylon-dev": {
        "libgstreamer1.0-dev": "headers",
        "libgstreamer-plugins-base1.0-dev": "headers",
        "gst-plugin-pylon": "package",
    },
    "python3-pygstpylon": {
        "libc6": "linker",
        "libgcc-s1": "linker",
        "libgstreamer1.0-dev": "headers-and-linker",
        "libgstreamer-plugins-base1.0-dev": "headers-and-linker",
        "libstdc++6": "linker",
        "python3-dev": "headers",
        "gst-plugin-pylon": "package",
    },
}

BUILDINFO_PACKAGE = re.compile(
    r"^\s*([a-z0-9][a-z0-9+.-]*)(?::([a-z0-9][a-z0-9-]*))?"
    r"\s+\(\s*=\s*([^)]+?)\s*\)\s*$"
)


def deb_field(deb_path: Path, field: str) -> str:
    result = subprocess.run(
        ["dpkg-deb", "--field", str(deb_path), field],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def deb_purl(name: str, version: str, architecture: str) -> str:
    return (
        f"pkg:deb/{quote(name, safe='+.-')}@{quote(version, safe='.-_~')}"
        f"?arch={quote(architecture, safe='.-_~')}&distro=ubuntu-22.04"
    )


def split_field_list(value: str) -> list[str]:
    # Buildinfo continuation lines are already folded; package names and Debian
    # versions do not contain commas in the Installed-Build-Depends field.
    return [entry.strip() for entry in value.split(",") if entry.strip()]


def installed_build_dependencies(buildinfo_path: Path) -> dict[str, tuple[str, str | None]]:
    fields = {}
    current = None
    for line in buildinfo_path.read_text(encoding="utf-8").splitlines():
        if line.startswith((" ", "\t")) and current:
            fields[current] += " " + line.strip()
        elif ":" in line:
            current, value = line.split(":", 1)
            fields[current] = value.strip()
        else:
            current = None

    dependencies = {}
    for entry in split_field_list(fields.get("Installed-Build-Depends", "")):
        match = BUILDINFO_PACKAGE.fullmatch(entry)
        if match:
            name, architecture, version = match.groups()
            dependencies[name] = (version.strip(), architecture)
    if not dependencies:
        raise ValueError(
            f"No exact Installed-Build-Depends entries found in {buildinfo_path}"
        )
    return dependencies


def package_component(
    name: str,
    version: str,
    architecture: str,
    dependency_kind: str,
) -> dict:
    purl = deb_purl(name, version, architecture)
    return {
        "type": "library",
        "name": name,
        "version": version,
        "purl": purl,
        "bom-ref": purl,
        "scope": "required",
        "supplier": {
            "name": "Ubuntu",
            "url": ["https://ubuntu.com"],
        },
        "properties": [
            {"name": "basler:sbom:dependency-kind", "value": dependency_kind},
        ],
    }


def pylon_component(version: str) -> dict:
    purl = (
        f"pkg:conan/pylon-core@{quote(version, safe='.-_~')}"
        "?channel=potentially-public&user=release"
    )
    return {
        "type": "library",
        "name": "pylon-core",
        "version": version,
        "purl": purl,
        "bom-ref": purl,
        "scope": "required",
        "supplier": {
            "name": "Basler AG",
            "url": ["https://www.baslerweb.com"],
        },
        "properties": [
            {"name": "basler:sbom:dependency-kind", "value": "headers-and-linker"},
            {
                "name": "basler:conan:reference",
                "value": f"pylon-core/{version}@release/potentially-public",
            },
        ],
    }


def main() -> None:
    if len(sys.argv) != 6:
        raise SystemExit(
            f"usage: {Path(sys.argv[0]).name} <target.deb> <buildinfo> "
            "<pylon-core-version.txt> <output-sbom> <arch>"
        )
    target_path, buildinfo_path, pylon_version_path, output_path = map(
        Path, sys.argv[1:5]
    )
    architecture = sys.argv[5]

    target_name = deb_field(target_path, "Package")
    target_version = deb_field(target_path, "Version")
    target_architecture = deb_field(target_path, "Architecture")
    if target_name not in BUILD_DEPENDENCIES:
        raise ValueError(f"No direct build dependency mapping for {target_name}")
    if target_architecture not in (architecture, "all"):
        raise ValueError(
            f"Package architecture {target_architecture} does not match {architecture}"
        )

    dependencies = installed_build_dependencies(buildinfo_path)
    components = []
    for name, dependency_kind in BUILD_DEPENDENCIES[target_name].items():
        if name == "pylon-core":
            version = pylon_version_path.read_text(encoding="utf-8").strip()
            if not version:
                raise ValueError(f"Resolved pylon-core version is empty: {pylon_version_path}")
            components.append(pylon_component(version))
            continue

        if name == "gst-plugin-pylon":
            # The package is built alongside the other binary packages from
            # the same source build; look it up from the downloaded artifacts.
            package_path = next(
                target_path.parent.glob(f"gst-plugin-pylon_*_{architecture}.deb"),
                None,
            )
            if package_path is None:
                raise ValueError("Could not find the gst-plugin-pylon build item")
            version = deb_field(package_path, "Version")
            purl = deb_purl("gst-plugin-pylon", version, architecture)
            components.append({
                "type": "application",
                "name": "gst-plugin-pylon",
                "version": version,
                "purl": purl,
                "bom-ref": purl,
                "scope": "required",
                "supplier": {
                    "name": "Basler AG",
                    "url": ["https://www.baslerweb.com"],
                },
                "properties": [
                    {"name": "basler:sbom:dependency-kind", "value": dependency_kind},
                ],
            })
            continue

        resolved = dependencies.get(name)
        if resolved is None:
            raise ValueError(
                f"Direct build dependency {name} is absent from "
                f"{buildinfo_path.name} Installed-Build-Depends"
            )
        version, dependency_architecture = resolved
        components.append(package_component(
            name,
            version,
            dependency_architecture or architecture,
            dependency_kind,
        ))

    root_purl = deb_purl(target_name, target_version, target_architecture)
    root = {
        "type": "application",
        "name": target_name,
        "version": target_version,
        "purl": root_purl,
        "bom-ref": root_purl,
        "scope": "required",
        "hashes": [{
            "alg": "SHA-512",
            "content": hashlib.sha512(target_path.read_bytes()).hexdigest(),
        }],
        "supplier": {
            "name": "Basler AG",
            "url": ["https://www.baslerweb.com"],
        },
        "properties": [
            {"name": "basler:sbom:classification", "value": "build"},
            {"name": "bsi:component:filename", "value": target_path.name},
            {
                "name": "basler:sbom:dependency-completeness",
                "value": "complete for the selected direct build dependencies",
            },
        ],
    }
    bom = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.6",
        "version": 1,
        "metadata": {
            "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds").replace(
                "+00:00", "Z"
            ),
            "authors": [{
                "name": "Basler AG",
                "email": "support.europe@baslerweb.com",
            }],
            "tools": {
                "components": [{
                    "type": "application",
                    "name": "generate_build_sbom.py",
                    "version": "1",
                }]
            },
            "component": root,
        },
        "components": components,
        "dependencies": [{
            "ref": root_purl,
            "dependsOn": sorted(component["bom-ref"] for component in components),
        }],
    }
    output_path.write_text(
        json.dumps(bom, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()

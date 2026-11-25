#!/usr/bin/env python3
"""Generate GitHub Release markdown for universal Debian packages and SBOMs."""

import os
import re
import sys
from collections import defaultdict

import requests


PACKAGES = [
    ("gst-plugin-pylon", "GStreamer Plugin"),
    ("gst-plugin-pylon-dev", "Development Package"),
    ("python3-pygstpylon", "Python Bindings"),
]


def fetch_release_files(repo_url, version):
    api_url = f"https://api.github.com/repos/{repo_url}/releases/tags/{version}"
    response = requests.get(api_url, timeout=30)
    if response.status_code != 200:
        print(f"Error fetching release info: {response.status_code}", file=sys.stderr)
        return []
    release_info = response.json()
    return [
        {"name": asset["name"], "url": asset["browser_download_url"]}
        for asset in release_info.get("assets", [])
    ]


def extract_release_notes(version):
    changelog_path = "CHANGELOG.md"
    if not os.path.exists(changelog_path):
        print("CHANGELOG.md file not found.", file=sys.stderr)
        return ""

    with open(changelog_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    match = re.match(r"v?(\d+\.\d+\.\d+).*", version)
    if not match:
        print(f"Could not parse semantic version from tag {version!r}", file=sys.stderr)
        return ""
    base_version = match.group(1)

    release_notes = []
    in_section = False
    for line in lines:
        if line.startswith("## [") and in_section:
            break
        if line.startswith(f"## [{base_version}]"):
            in_section = True
        if in_section:
            release_notes.append(line)

    if not release_notes:
        print(
            f"No release notes found for version {base_version} in CHANGELOG.md.",
            file=sys.stderr,
        )
        return ""

    return "".join(release_notes).strip()


def categorize_deb_file(filename):
    arch_map = {
        "x86_64": r"_amd64\.deb$",
        "arm64": r"_arm64\.deb$",
    }
    for package, _ in PACKAGES:
        if filename.startswith(f"{package}_") and filename.endswith(".deb"):
            for arch_label, arch_pattern in arch_map.items():
                if re.search(arch_pattern, filename):
                    return package, arch_label
    return None, None


def generate_package_table(files):
    categorized = defaultdict(dict)
    for file_info in files:
        package, arch = categorize_deb_file(file_info["name"])
        if package and arch:
            categorized[package][arch] = f"[{file_info['name']}]({file_info['url']})"

    table = "## Debian / Ubuntu Packages (Ubuntu 22.04+, Debian 12+)\n\n"
    table += "| Package | x86_64 (`amd64`) | ARM64 (`arm64`) |\n"
    table += "|---------|------------------|-----------------|\n"
    for package, label in PACKAGES:
        x64_link = categorized[package].get("x86_64", "N/A")
        arm64_link = categorized[package].get("arm64", "N/A")
        table += f"| **{label}** (`{package}`) | {x64_link} | {arm64_link} |\n"
    return table


def generate_sbom_section(files):
    sboms = [
        f"- [{item['name']}]({item['url']})"
        for item in sorted(files, key=lambda x: x["name"])
        if item["name"].endswith(".cdx.json")
    ]
    if not sboms:
        return ""
    return "## CycloneDX Build SBOMs\n\n" + "\n".join(sboms) + "\n"


def generate_release_template(files, version, release_notes):
    sections = [f"# Release {version}\n"]
    if release_notes:
        sections.append("## Release Notes\n\n" + release_notes + "\n")
    sections.append(generate_package_table(files))

    sbom_section = generate_sbom_section(files)
    if sbom_section:
        sections.append(sbom_section)

    source_files = [
        item for item in files if item["name"].endswith((".tar.gz", ".zip"))
    ]
    if source_files:
        src_lines = ["## Source Code\n"]
        for item in source_files:
            src_lines.append(f"- [{item['name']}]({item['url']})")
        sections.append("\n".join(src_lines) + "\n")

    return "\n".join(sections)


def main():
    if len(sys.argv) != 2:
        print("Usage: python tools/prepare_release_page.py <release-tag>", file=sys.stderr)
        sys.exit(1)

    version = sys.argv[1]
    repo_url = "basler/gst-plugin-pylon"
    files = fetch_release_files(repo_url, version)
    release_notes = extract_release_notes(version)
    template = generate_release_template(files, version, release_notes)
    print(template)

    with open("release_template.md", "w", encoding="utf-8") as f:
        f.write(template)
    print("Template has been saved to release_template.md")


if __name__ == "__main__":
    main()

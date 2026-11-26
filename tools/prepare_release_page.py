#!/usr/bin/env python3
"""Generate GitHub Release markdown for universal Debian packages and SBOMs."""

import argparse
from collections import defaultdict
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.request


PACKAGES = [
    ("gst-plugin-pylon", "GStreamer Plugin"),
    ("gst-plugin-pylon-dev", "Development Package"),
    ("python3-pygstpylon", "Python Bindings"),
]


def fetch_release_files(repo_url, version):
    headers = {"Accept": "application/vnd.github+json"}
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"

    release_info = None
    tag_url = f"https://api.github.com/repos/{repo_url}/releases/tags/{version}"
    try:
        with urllib.request.urlopen(urllib.request.Request(tag_url, headers=headers), timeout=30) as resp:
            release_info = json.load(resp)
    except urllib.error.HTTPError as err:
        if err.code == 404 and token:
            list_url = f"https://api.github.com/repos/{repo_url}/releases?per_page=20"
            with urllib.request.urlopen(urllib.request.Request(list_url, headers=headers), timeout=30) as resp:
                for rel in json.load(resp):
                    if rel.get("tag_name") == version:
                        release_info = rel
                        break
        if release_info is None:
            print(f"Error fetching release info: {err.code}", file=sys.stderr)
            return []

    return [
        {"name": asset["name"], "url": asset["browser_download_url"]}
        for asset in release_info.get("assets", [])
    ]


def collect_local_assets(assets_dir, repo_url, version):
    base_url = f"https://github.com/{repo_url}/releases/download/{version}"
    files = []
    for path in sorted(Path(assets_dir).iterdir()):
        if not path.is_file():
            continue
        if path.name.endswith((".deb", ".cdx.json", ".grype.txt")):
            files.append({"name": path.name, "url": f"{base_url}/{path.name}"})
    return files


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
            continue
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

    table = (
        "## Debian / Ubuntu Packages "
        "(Ubuntu 22.04+, Debian 12 bookworm / 13 trixie)\n\n"
    )
    table += "| Package | x86_64 (`amd64`) | ARM64 (`arm64`) |\n"
    table += "|---------|------------------|-----------------|\n"
    for package, label in PACKAGES:
        x64_link = categorized[package].get("x86_64", "N/A")
        arm64_link = categorized[package].get("arm64", "N/A")
        table += f"| **{label}** (`{package}`) | {x64_link} | {arm64_link} |\n"
    return table


def generate_sbom_section(files):
    by_name = {item["name"]: item["url"] for item in files}
    sbom_lines = []
    for name in sorted(by_name):
        if not name.endswith(".cdx.json"):
            continue
        line = f"- [{name}]({by_name[name]})"
        grype_name = name[:-len(".cdx.json")] + ".grype.txt"
        if grype_name in by_name:
            line += f" ([vulnerability scan summary]({by_name[grype_name]}))"
        sbom_lines.append(line)
    if not sbom_lines:
        return ""
    return "## CycloneDX Build SBOMs & Vulnerability Summaries\n\n" + "\n".join(sbom_lines) + "\n"


def generate_release_template(files, version, release_notes):
    sections = [f"# Release {version}\n"]
    sections.append(generate_package_table(files))

    sbom_section = generate_sbom_section(files)
    if sbom_section:
        sections.append(sbom_section)

    if release_notes:
        sections.append("## Release Notes\n\n" + release_notes + "\n")

    return "\n".join(sections)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="Release git tag (e.g. v1.1.0 or v1.1.0rc1)")
    parser.add_argument(
        "--assets-dir",
        help="Local directory of release assets (builds deterministic GitHub download URLs without querying the API)",
    )
    parser.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_REPOSITORY", "basler/gst-plugin-pylon"),
        help="GitHub owner/repo (default: basler/gst-plugin-pylon)",
    )
    parser.add_argument(
        "--output",
        default="release_template.md",
        help="Output Markdown file path (default: release_template.md)",
    )
    args = parser.parse_args()

    if args.assets_dir:
        files = collect_local_assets(args.assets_dir, args.repo, args.version)
    else:
        files = fetch_release_files(args.repo, args.version)

    release_notes = extract_release_notes(args.version)
    template = generate_release_template(files, args.version, release_notes)
    print(template)

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(template)
    print(f"Template has been saved to {args.output}")


if __name__ == "__main__":
    main()

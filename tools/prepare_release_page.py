#! /bin/env python3
import re
import sys
import os
import requests
from collections import defaultdict


def fetch_release_files(repo_url, version):
    api_url = f"https://api.github.com/repos/{repo_url}/releases/tags/{version}"
    response = requests.get(api_url)
    if response.status_code != 200:
        print(f"Error fetching release info: {response.status_code}")
        return []
    release_info = response.json()
    files = [
        {"name": asset["name"], "url": asset["browser_download_url"]}
        for asset in release_info.get("assets", [])
    ]
    return files


def extract_release_notes(version):
    changelog_path = 'CHANGELOG.md'
    if not os.path.exists(changelog_path):
        print("CHANGELOG.md file not found.")
        return ""

    with open(changelog_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Extract the base version number without any suffixes
    base_version = re.match(r"v(\d+\.\d+\.\d+).*", version).group(1)

    release_notes = []
    in_section = False

    for line in lines:
        # Check for the start of the next section, which indicates the end of the current section
        if line.startswith("## [") and in_section:
            break
        # Identify the start of the section for the desired version
        if line.startswith(f"## [{base_version}]"):
            in_section = True
        # If we are in the desired section, append the line to release_notes
        if in_section:
            release_notes.append(line)

    # If no notes were found, return a message
    if not release_notes:
        print(f"No release notes found for version {base_version} in CHANGELOG.md.")
        return ""

    return ''.join(release_notes).strip()


def categorize_file(filename):
    os_patterns = {
        "Ubuntu 24.04": r"ubuntu-24\.04",
        "Ubuntu 22.04": r"ubuntu-22\.04",
        "Ubuntu 20.04": r"ubuntu-20\.04",
        "Debian Bookworm": r"debian-12|bookworm",
        "Debian Bullseye": r"debian-11|bullseye",
    }

    arch_patterns = {
        "x86_64": r"amd64",
        "arm64": r"arm64",
    }

    package_patterns = {
        "gst-plugin-pylon": r"^gst-plugin-pylon_.*\.deb$",
        "gst-plugin-pylon-dev": r"^gst-plugin-pylon-dev_.*\.deb$",
        "python3-pygstpylon": r"^python3-pygstpylon_.*\.deb$",
    }

    for package, pattern in package_patterns.items():
        if re.match(pattern, filename):
            for os, os_pattern in os_patterns.items():
                if re.search(os_pattern, filename, re.IGNORECASE):
                    for arch, arch_pattern in arch_patterns.items():
                        if re.search(arch_pattern, filename, re.IGNORECASE):
                            return package, os, arch

    return None, None, None


def generate_package_table(files, package_name, header, version, repo_url):
    categorized_files = defaultdict(lambda: defaultdict(str))

    for file_info in files:
        filename = file_info["name"]
        file_url = file_info["url"]
        package, os, arch = categorize_file(filename)
        if package == package_name:
            categorized_files[os][arch] = f"[{filename}]({file_url})"

    table = f"### {header}\n\n"
    table += "| OS Version      | x86_64 | arm64 |\n"
    table += "|-----------------|--------|-------|\n"

    for os in [
        "Ubuntu 24.04",
        "Ubuntu 22.04",
        "Ubuntu 20.04",
        "Debian Bookworm",
        "Debian Bullseye",
    ]:
        x86_64_link = categorized_files[os]["x86_64"] or "N/A"
        arm64_link = categorized_files[os]["arm64"] or "N/A"
        table += f"| {os:<16} | {x86_64_link} | {arm64_link} |\n"

    return table


def generate_release_template(files, version, repo_url, release_notes):
    template = f"# Release {version}\n\n"

    template += "## Release Notes\n\n"
    template += release_notes + "\n\n"

    packages = [
        ("gst-plugin-pylon", "GStreamer Plugin"),
        ("gst-plugin-pylon-dev", "Development Package"),
        ("python3-pygstpylon", "Python Bindings"),
    ]

    for package, header in packages:
        template += generate_package_table(files, package, header, version, repo_url)
        template += "\n"

    source_files = [
        file_info
        for file_info in files
        if file_info["name"].endswith((".tar.gz", ".zip"))
    ]
    if source_files:
        template += "## Source Code\n\n"
        for file_info in source_files:
            filename = file_info["name"]
            file_url = file_info["url"]
            template += f"- [{filename}]({file_url})\n"

    return template


# Get the release tag from the command line arguments
if len(sys.argv) != 2:
    print("Usage: python script.py <release-tag>")
    sys.exit(1)


version = sys.argv[1]  # e.g., "v0.7.3rc1"
repo_url = "basler/gst-plugin-pylon"

# Fetch files from the release page using the GitHub API
files = fetch_release_files(repo_url, version)

# Extract release notes from CHANGELOG.md
release_notes = extract_release_notes(version)

# Generate the template
template = generate_release_template(files, version, repo_url, release_notes)
print(template)

# Optionally, save the template to a file
with open("release_template.md", "w", encoding="utf-8") as f:
    f.write(template)
print("Template has been saved to release_template.md")

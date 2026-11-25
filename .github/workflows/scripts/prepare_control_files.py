"""Select matching SDK descriptors and Linux Debian packages, then download them."""

import json
import os
from pathlib import Path
import re
import subprocess
import sys


DEB_ARCH_PATTERNS = {
    "x86_64": re.compile(
        r"^pylon(?:[-_](?!supplementary|plus).*)?[-_](?:linux[-_])?(?:x86_64|amd64)(?:[-_][0-9]+)?(?:\.deb|[-_]debs?\.tar\.gz)$",
        re.IGNORECASE,
    ),
    "aarch64": re.compile(
        r"^pylon(?:[-_](?!supplementary|plus).*)?[-_](?:linux[-_])?(?:aarch64|arm64)(?:[-_][0-9]+)?(?:\.deb|[-_]debs?\.tar\.gz)$",
        re.IGNORECASE,
    ),
}


def parse_version(value, allow_two_segment=False):
    """Parse pylon suite version 'YY.MM[.patch]' into decimal ints."""
    match = re.fullmatch(r"([0-9]+)\.([0-9]+)(?:\.([0-9]+))?", value)
    if match is None:
        raise ValueError(f"Invalid pylon SDK version: {value!r}")
    year = int(match.group(1), 10)
    month = int(match.group(2), 10)
    patch = (
        int(match.group(3), 10)
        if match.group(3) is not None
        else (None if allow_two_segment else 0)
    )
    if not 1 <= month <= 12:
        raise ValueError(f"Invalid month in pylon SDK version: {value!r}")
    return year, month, patch


def _find_deb_package(directory, deb_arch, files_by_dir):
    pattern = DEB_ARCH_PATTERNS.get(deb_arch)
    if pattern is None:
        raise ValueError(f"Unsupported deb_arch: {deb_arch!r}")
    dir_files = sorted(files_by_dir.get(directory, ()))
    matches = [f"{directory}/{name}" for name in dir_files if pattern.fullmatch(name)]
    if not matches:
        raise ValueError(f"Missing Debian package for {deb_arch}")
    if len(matches) != 1:
        raise ValueError(f"Ambiguous Debian packages for {deb_arch} ({len(matches)} matches)")
    return matches[0]


def select_artifacts(paths, platforms, search_prefix, sdk_version=None):
    """Select latest SDK patch/build per platform under search_prefix."""
    prefix = search_prefix.strip().strip("/")
    if not prefix:
        raise ValueError("SDK_SEARCH_PREFIX must not be empty")
    prefix_slash = prefix + "/"

    sdk_filter = None
    if sdk_version:
        sdk_filter = parse_version(sdk_version, allow_two_segment=True)

    files_by_dir = {}
    reports = []
    for path in sorted(set(paths)):
        if not path.startswith(prefix_slash):
            continue
        rel = path[len(prefix_slash):]
        parts = rel.split("/")
        if len(parts) != 3:
            continue
        version_text, build_text, filename = parts
        directory = f"{prefix}/{version_text}/{build_text}"
        files_by_dir.setdefault(directory, []).append(filename)

        if not filename.startswith("report_") or not filename.endswith(".json"):
            continue
        version = parse_version(version_text)
        if sdk_filter is not None:
            if version[:2] != sdk_filter[:2]:
                continue
            if sdk_filter[2] is not None and version[2] != sdk_filter[2]:
                continue
        if re.fullmatch(r"[0-9]{8}\.[0-9]+", build_text) is None:
            raise ValueError(f"Invalid SDK build identifier: {build_text!r}")
        build = tuple(int(part, 10) for part in build_text.split("."))
        reports.append((version, build, path))

    target_desc = sdk_version or "latest SDK"
    if not reports:
        raise ValueError(f"No SDK metadata found for {target_desc}")
    if sdk_filter is None:
        latest_version = max(report[0] for report in reports)
        reports = [report for report in reports if report[0] == latest_version]

    selections = {}
    for platform, platform_cfg in platforms.items():
        candidates = []
        for version, build, path in reports:
            report_name = path.rsplit("/", 1)[1]
            stem = report_name[len("report_"):-len(".json")]
            if stem == platform or stem.startswith(platform + "_"):
                candidates.append((version, build, path))
        if not candidates:
            raise ValueError(f"No SDK metadata for {platform} ({target_desc})")
        latest = max((version, build) for version, build, _ in candidates)
        matches = [path for version, build, path in candidates if (version, build) == latest]
        if len(matches) != 1:
            raise ValueError(f"Ambiguous SDK metadata for {platform} ({len(matches)} matches)")
        report = matches[0]
        directory = report.rsplit("/", 1)[0]
        version_str = report[len(prefix_slash):].split("/", 1)[0]

        deb_arch = (
            (platform_cfg or {}).get("deb_arch")
            if isinstance(platform_cfg, dict)
            else None
        )
        deb_archive = (
            _find_deb_package(directory, deb_arch, files_by_dir) if deb_arch else None
        )
        selections[platform] = {
            "version": version_str,
            "report": report,
            "deb_archive": deb_archive,
        }
    return selections


def download_file(source, destination):
    """Fail on CLI errors, missing files and empty files, without logging internal paths."""
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        destination.unlink()
    subprocess.run(
        [
            "jf",
            "rt",
            "dl",
            "--quiet=true",
            "--flat=true",
            "--fail-no-op=true",
            source,
            str(destination),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if not destination.is_file() or destination.stat().st_size == 0:
        raise ValueError(f"Download did not produce a nonempty file: {destination}")


def main():
    with open(".github/workflows/build_config.json", encoding="utf-8") as config_file:
        platforms = json.load(config_file)

    search_prefix = os.environ.get("SDK_SEARCH_PREFIX", "").strip().strip("/")
    if not search_prefix:
        raise ValueError("SDK_SEARCH_PREFIX environment variable is not set")

    sdk_version = os.environ.get("PYLON_SDK_VERSION", "").strip() or None
    if sdk_version is not None:
        parse_version(sdk_version, allow_two_segment=True)

    result = subprocess.run(
        ["jf", "rt", "s", f"{search_prefix}/*"],
        check=True,
        capture_output=True,
        text=True,
    )
    artifacts = json.loads(result.stdout)
    selections = select_artifacts(
        [artifact["path"] for artifact in artifacts],
        platforms,
        search_prefix=search_prefix,
        sdk_version=sdk_version,
    )

    resolved_versions = set()
    for platform, item in selections.items():
        resolved_versions.add(item["version"])
        print(
            f"Prepared SDK inputs for {platform} (pylon {item['version']})",
            flush=True,
        )
        if item["deb_archive"] is not None:
            dest_name = (
                "pylon.deb"
                if item["deb_archive"].lower().endswith(".deb")
                else "pylon_debs.tar.gz"
            )
            download_file(item["deb_archive"], Path(platform) / dest_name)
        else:
            download_file(item["report"], Path(platform) / f"control_{platform}.json")

    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output and resolved_versions:
        summary_version = sorted(resolved_versions)[-1]
        with open(github_output, "a", encoding="utf-8") as out:
            out.write(f"pylon_version={summary_version}\n")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        cmd_name = error.cmd[0] if isinstance(error.cmd, (list, tuple)) and error.cmd else "command"
        print(
            f"SDK input preparation failed: {cmd_name} exited with status {error.returncode}",
            file=sys.stderr,
        )
        sys.exit(1)
    except (ValueError, KeyError, OSError) as error:
        print(f"SDK input preparation failed: {error}", file=sys.stderr)
        sys.exit(1)

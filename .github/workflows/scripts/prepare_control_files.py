"""Select matching SDK descriptors and Linux Debian packages, then download them."""

import json
import os
from pathlib import Path
import re
import socket
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


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

BUILD_DIR_RE = re.compile(r"^[0-9]{8}\.[0-9]+$")


class SanitizedSDKError(RuntimeError):
    """Error safe to print to CI logs without leaking endpoints or paths."""


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


def _version_matches_filter(version, sdk_filter):
    if sdk_filter is None:
        return True
    if version[:2] != sdk_filter[:2]:
        return False
    if sdk_filter[2] is not None and version[2] != sdk_filter[2]:
        return False
    return True


def _find_deb_package(directory, deb_arch, files_by_dir):
    pattern = DEB_ARCH_PATTERNS.get(deb_arch)
    if pattern is None:
        raise ValueError(f"Unsupported deb_arch: {deb_arch!r}")
    dir_files = sorted(files_by_dir.get(directory, ()))
    matches = [f"{directory}/{name}" for name in dir_files if pattern.fullmatch(name)]
    if not matches:
        raise ValueError(f"Missing Debian package for {deb_arch}")
    if len(matches) != 1:
        raise ValueError(
            f"Ambiguous Debian packages for {deb_arch} ({len(matches)} matches)"
        )
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
        rel = path[len(prefix_slash) :]
        parts = rel.split("/")
        if len(parts) != 3:
            continue
        version_text, build_text, filename = parts
        directory = f"{prefix}/{version_text}/{build_text}"
        files_by_dir.setdefault(directory, []).append(filename)

        if not filename.startswith("report_") or not filename.endswith(".json"):
            continue
        version = parse_version(version_text)
        if not _version_matches_filter(version, sdk_filter):
            continue
        if BUILD_DIR_RE.fullmatch(build_text) is None:
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
            stem = report_name[len("report_") : -len(".json")]
            if stem == platform or stem.startswith(platform + "_"):
                candidates.append((version, build, path))
        if not candidates:
            raise ValueError(f"No SDK metadata for {platform} ({target_desc})")
        latest = max((version, build) for version, build, _ in candidates)
        matches = [
            path for version, build, path in candidates if (version, build) == latest
        ]
        if len(matches) != 1:
            raise ValueError(
                f"Ambiguous SDK metadata for {platform} ({len(matches)} matches)"
            )
        report = matches[0]
        directory = report.rsplit("/", 1)[0]
        version_str = report[len(prefix_slash) :].split("/", 1)[0]

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


def normalize_api_base(sdk_url):
    base = sdk_url.strip().rstrip("/")
    if not base:
        raise ValueError("SDK_URL environment variable is not set")
    if not base.endswith("/artifactory"):
        base = f"{base}/artifactory"
    return base


def _describe_transport_error(err):
    if isinstance(err, urllib.error.HTTPError):
        return f"HTTP {err.code}"
    if isinstance(err, urllib.error.URLError):
        reason = err.reason
        if isinstance(reason, (TimeoutError, socket.timeout)):
            return "connection timed out"
        if isinstance(reason, socket.gaierror):
            return "DNS resolution failed"
        if isinstance(reason, OSError):
            code = f" errno={reason.errno}" if reason.errno is not None else ""
            return f"{type(reason).__name__}{code}"
        return type(reason).__name__
    if isinstance(err, (TimeoutError, socket.timeout)):
        return "connection timed out"
    return type(err).__name__


def _open_with_retries(url, token, timeout=30, max_attempts=3):
    headers = {
        "Authorization": f"Bearer {token}",
        "User-Agent": "gst-plugin-pylon-ci/1.0",
    }
    last_err = None
    for attempt in range(1, max_attempts + 1):
        req = urllib.request.Request(url, headers=headers)
        try:
            return urllib.request.urlopen(req, timeout=timeout)
        except urllib.error.HTTPError as err:
            last_err = err
            if err.code not in (429, 500, 502, 503, 504) or attempt == max_attempts:
                break
        except (urllib.error.URLError, TimeoutError, socket.timeout, OSError) as err:
            last_err = err
            if attempt == max_attempts:
                break
        time.sleep(min(2 * attempt, 6))
    raise SanitizedSDKError(
        f"SDK server request failed after {max_attempts} attempt(s): "
        f"{_describe_transport_error(last_err)}"
    )


def list_storage_children(api_base, token, repo_path):
    """List immediate children of a repository folder via the Storage API."""
    encoded = urllib.parse.quote(repo_path.strip("/"), safe="/")
    url = f"{api_base}/api/storage/{encoded}"
    with _open_with_retries(url, token, timeout=30, max_attempts=3) as resp:
        payload = json.loads(resp.read().decode("utf-8"))
    children = []
    for entry in payload.get("children", ()):
        name = entry.get("uri", "").lstrip("/")
        if name:
            children.append((name, bool(entry.get("folder", False))))
    return children


def discover_candidate_paths(
    list_children_fn, platforms, search_prefix, sdk_version=None
):
    """Walk only the latest matching version/build folders instead of scanning all history."""
    prefix = search_prefix.strip().strip("/")
    if not prefix:
        raise ValueError("SDK_SEARCH_PREFIX must not be empty")

    sdk_filter = None
    if sdk_version:
        sdk_filter = parse_version(sdk_version, allow_two_segment=True)

    version_dirs = []
    for name, is_folder in list_children_fn(prefix):
        if not is_folder:
            continue
        try:
            parsed = parse_version(name)
        except ValueError:
            continue
        if _version_matches_filter(parsed, sdk_filter):
            version_dirs.append((parsed, name))

    version_dirs.sort(reverse=True)
    if not version_dirs:
        return []

    required_platforms = set(platforms.keys())
    collected_paths = []
    matched_version = None

    for parsed_ver, version_name in version_dirs:
        if matched_version is not None and parsed_ver != matched_version:
            break
        build_dirs = []
        for bname, is_folder in list_children_fn(f"{prefix}/{version_name}"):
            if not is_folder or BUILD_DIR_RE.fullmatch(bname) is None:
                continue
            build_tuple = tuple(int(part, 10) for part in bname.split("."))
            build_dirs.append((build_tuple, bname))
        build_dirs.sort(reverse=True)

        found_platforms = set()
        version_paths = []
        for _, build_name in build_dirs:
            folder = f"{prefix}/{version_name}/{build_name}"
            for fname, is_folder in list_children_fn(folder):
                if is_folder:
                    continue
                version_paths.append(f"{folder}/{fname}")
                if fname.startswith("report_") and fname.endswith(".json"):
                    stem = fname[len("report_") : -len(".json")]
                    for plat in required_platforms:
                        if stem == plat or stem.startswith(plat + "_"):
                            found_platforms.add(plat)
            if required_platforms.issubset(found_platforms):
                break

        if found_platforms:
            collected_paths.extend(version_paths)
            if sdk_filter is None:
                matched_version = parsed_ver
            if required_platforms.issubset(found_platforms):
                break

    return collected_paths


def download_file(api_base, token, source, destination):
    """Stream-download a repository file without logging internal paths or URLs."""
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        destination.unlink()

    encoded = urllib.parse.quote(source.strip("/"), safe="/")
    url = f"{api_base}/{encoded}"
    with _open_with_retries(url, token, timeout=120, max_attempts=3) as resp:
        with open(destination, "wb") as out:
            while True:
                chunk = resp.read(1024 * 1024)
                if not chunk:
                    break
                out.write(chunk)

    if not destination.is_file() or destination.stat().st_size == 0:
        raise ValueError(f"Download did not produce a nonempty file: {destination}")


def main():
    with open(".github/workflows/build_config.json", encoding="utf-8") as config_file:
        platforms = json.load(config_file)

    api_base = normalize_api_base(os.environ.get("SDK_URL", ""))
    token = os.environ.get("SDK_TOKEN", "").strip()
    if not token:
        raise ValueError("SDK_TOKEN environment variable is not set")

    search_prefix = os.environ.get("SDK_SEARCH_PREFIX", "").strip().strip("/")
    if not search_prefix:
        raise ValueError("SDK_SEARCH_PREFIX environment variable is not set")

    sdk_version = os.environ.get("PYLON_SDK_VERSION", "").strip() or None
    if sdk_version is not None:
        parse_version(sdk_version, allow_two_segment=True)

    paths = discover_candidate_paths(
        lambda repo_path: list_storage_children(api_base, token, repo_path),
        platforms,
        search_prefix=search_prefix,
        sdk_version=sdk_version,
    )
    selections = select_artifacts(
        paths,
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
            download_file(api_base, token, item["deb_archive"], Path(platform) / dest_name)
        else:
            download_file(
                api_base,
                token,
                item["report"],
                Path(platform) / f"control_{platform}.json",
            )

    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output and resolved_versions:
        summary_version = sorted(resolved_versions)[-1]
        with open(github_output, "a", encoding="utf-8") as out:
            out.write(f"pylon_version={summary_version}\n")


if __name__ == "__main__":
    try:
        main()
    except (SanitizedSDKError, ValueError, KeyError, OSError) as error:
        print(f"SDK input preparation failed: {error}", file=sys.stderr)
        sys.exit(1)

#!/bin/bash
set -euo pipefail

# Function to parse the L4T version string and format it
parse_nvidia_version() {
    local version_string="$1"
    local major minor revision date formatted_date

    # Extract major version (e.g., 35)
    major=$(echo "$version_string" | grep -oP '(?<=# R)[0-9]+')

    # Extract minor version and revision (e.g., 4.1)
    minor_revision=$(echo "$version_string" | grep -oP '(?<=REVISION: )[0-9]+\.[0-9]+')
    minor=$(echo "$minor_revision" | cut -d. -f1)
    revision=$(echo "$minor_revision" | cut -d. -f2)

    # Extract date (e.g., Tue Aug  1 19:57:35 UTC 2023)
    date=$(echo "$version_string" | grep -oP '(?<=DATE: ).*')

    # Format date (e.g., 20230801195735)
    formatted_date=$(date -d "$date" +'%Y%m%d%H%M%S')

    # Combine into desired format (e.g., 35.4.1-20230801195735)
    echo "${major}.${minor}.${revision}-${formatted_date}"
}

# Function to detect the platform version from /etc/os-release
detect_os_version() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        echo "${ID}-${VERSION_ID}"
    else
        echo "unknown"
    fi
}

# Resolve the pylon dpkg Version used for this build. Appears in the Debian
# package Version as +pylon<ver> (visible in the .deb filename) and is what
# debian/rules later substitutes into Depends / Pylon-Built-Against.
resolve_pylon_version() {
    if dpkg -s pylon &>/dev/null; then
        dpkg-query -W -f='${Version}' pylon
        return
    fi
    if [[ -n "${PYLON_PKG_VERSION:-}" ]]; then
        echo "${PYLON_PKG_VERSION}"
        return
    fi
    echo "Error: pylon dpkg is not installed and PYLON_PKG_VERSION is unset." >&2
    echo "Install a pylon package, run tools/register_pylon_from_tree.sh, or set PYLON_PKG_VERSION." >&2
    if [[ ! -d "${PYLON_ROOT:-/opt/pylon}/include/pylon" ]]; then
        echo "Error: PYLON_ROOT=${PYLON_ROOT:-/opt/pylon} also has no include/pylon." >&2
    fi
    exit 1
}

# Debian Version may only use [A-Za-z0-9.+~:-]. Map '-' (as in
# 26.08.1-deb0) to '.' so the +pylon suffix never collides with an NVIDIA
# '-1~L4T' platform suffix when stripping on re-runs.
sanitize_pylon_version_for_deb() {
    echo "$1" | tr '-' '.' | sed 's/[^A-Za-z0-9.~+]/\./g'
}

# Extract L4T version information from /etc/nv_tegra_release
if [[ -f /etc/nv_tegra_release ]]; then
    nvidia_version_string=$(head -n 1 /etc/nv_tegra_release)
    PLATFORM_VERSION=$(parse_nvidia_version "$nvidia_version_string")
else
    PLATFORM_VERSION=$(detect_os_version)
fi

echo "Platform version is: $PLATFORM_VERSION"

changelog_file="packaging/debian/changelog"

# Check if the changelog file exists
if [ ! -f "$changelog_file" ]; then
    echo "Error: Changelog file '$changelog_file' not found."
    exit 1
fi

PYLON_VERSION="$(resolve_pylon_version)"
PYLON_VERSION_SAFE="$(sanitize_pylon_version_for_deb "$PYLON_VERSION")"
echo "Build-time pylon version: ${PYLON_VERSION}"

# Create a temporary file
temp_file=$(mktemp)

# Extract the current version from the changelog
current_version=$(head -n 1 "$changelog_file" | sed -n 's/.*(\(.*\)).*/\1/p')

# Strip prior +pylon / -1~ platform suffixes so the script is idempotent.
# Normal packages:  <base>+pylon<ver>
# NVIDIA packages:  <base>+pylon<ver>-1~<L4T>
# +pylon ver is sanitized (no '-'), so '-1~' always marks the L4T suffix.
base_version=$(echo "$current_version" | sed -E 's/\+pylon[A-Za-z0-9.~+]*//; s/-1~.*//')
if [[ -f /etc/nv_tegra_release ]]; then
    new_version="${base_version}+pylon${PYLON_VERSION_SAFE}-1~${PLATFORM_VERSION}"
else
    new_version="${base_version}+pylon${PYLON_VERSION_SAFE}"
fi

sed "1s/(${current_version})/(${new_version})/" "$changelog_file" > "$temp_file"
mv "$temp_file" "$changelog_file"

echo "Changelog updated successfully to version ${new_version}"
echo "Runtime Depends will be pylon (= ${PYLON_VERSION}); .deb name carries +pylon${PYLON_VERSION_SAFE}"

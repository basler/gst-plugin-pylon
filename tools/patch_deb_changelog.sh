#!/bin/bash

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

# Create a temporary file
temp_file=$(mktemp)

# Extract the current version from the changelog
current_version=$(head -n 1 "$changelog_file" | sed -n 's/.*(\(.*\)).*/\1/p')

# Remove any old distro suffix. Normal packages are built once on the oldest
# supported distro and keep one version across all target distributions.
# NVIDIA packages retain a platform suffix because their DeepStream/L4T ABI is
# a separate build target.
base_version=$(echo "$current_version" | sed 's/-1~.*//')
if [[ -f /etc/nv_tegra_release ]]; then
    new_version="${base_version}-1~${PLATFORM_VERSION}"
else
    new_version="${base_version}"
fi

# Replace the version in the first line of the changelog
sed "1s/(${current_version})/(${new_version})/" "$changelog_file" > "$temp_file"

# Check if sed command was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to modify the changelog."
    rm "$temp_file"
    exit 1
fi

# Replace the original file with the modified version
mv "$temp_file" "$changelog_file"

echo "Changelog updated successfully to version ${new_version}"

# Verify the build SDK. Runtime compatibility is declared statically in
# debian/control as pylon >= 26.06 and << 27; never pin binaries to the exact
# version of the SDK or CI stub used for this build.
if ! dpkg -s pylon &> /dev/null; then
    echo "Warning: pylon dpkg is not installed; using PYLON_ROOT=${PYLON_ROOT:-/opt/pylon}" >&2
    if [[ ! -d "${PYLON_ROOT:-/opt/pylon}/include/pylon" ]]; then
        echo "Error: pylon package is not installed and PYLON_ROOT has no include/pylon" >&2
        exit 1
    fi
fi

echo "Pylon compatibility remains the range declared in debian/control"


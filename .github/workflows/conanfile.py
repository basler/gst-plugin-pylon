import os
import json
import shutil
from conan import ConanFile


class GstPluginPylonConanConsumer(ConanFile):
    name = "gst-plugin-pylon"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "control_file": ["ANY"],
        "third_party_license_file": ["ANY"]
    }
    default_options = {
        "control_file": "Placeholder will be overwritten by the CI",
        "third_party_license_file": "Placeholder will be overwritten by the CI"
    }

    @property
    def _platform_name(self):
        arch = str(self.settings.arch).lower()
        os_name = str(self.settings.os).lower()
        if os_name == "linux":
            if arch == "x86_64":
                platform_key = "linux_x86_64"
            elif arch in ("aarch64", "armv8"):
                platform_key = "linux_aarch64"
            else:
                platform_key = "linux_x86_64"
        elif os_name == "windows":
            platform_key = "windows"
        elif os_name == "macos":
            platform_key = "macos"
        else:
            platform_key = "unknown"
        return platform_key

    def requirements(self):
        # Read the control file
        control_path = str(self.options.control_file)
        with open(control_path) as f:
            control = json.load(f)

        # Create a mapping of package names to versions from the control file
        version_map = {pkg["name"]: pkg["version"] for pkg in control}

        # License files
        self.requires("pylon-licenses/20251125@release/potentially-public")

        # Core pylon packages needed for gst-plugin-pylon (SDK and runtime only, no dataprocessing)
        core_packages = [
            "pylon-core"
        ]

        for req in core_packages:
            version = version_map.get(req)
            if version:
                self.requires(f"{req}/{version}@release/potentially-public")
            else:
                # Fallback version if not found in control file
                self.requires(f"{req}/26.06@release/potentially-public")

    def imports(self):
        # Copy legal files based on the platform
        os_name = str(self.settings.os).lower()
        if os_name == "linux":
            license_path = os.path.join(self.install_folder, "pylon", "share", "pylon", "licenses")
            os.makedirs(license_path, exist_ok=True)
            if os.path.exists(str(self.options.third_party_license_file)):
                shutil.copy2(str(self.options.third_party_license_file), license_path)
            self.copy("**/License.txt", root_package="pylon-licenses", dst="pylon/share/pylon/licenses", ignore_case=True, keep_path=False)
        elif os_name == "windows":
            license_path = os.path.join(self.install_folder, "pylon", "Licenses")
            os.makedirs(license_path, exist_ok=True)
            if os.path.exists(str(self.options.third_party_license_file)):
                shutil.copy2(str(self.options.third_party_license_file), license_path)
            self.copy("**/License.txt", root_package="pylon-licenses", dst="pylon/Licenses", ignore_case=True, keep_path=False)
        elif os_name == "macos":
            license_path = os.path.join(self.install_folder, "pylon", "Frameworks", "pylon.framework", "Versions", "A", "Resources")
            os.makedirs(license_path, exist_ok=True)
            if os.path.exists(str(self.options.third_party_license_file)):
                shutil.copy2(str(self.options.third_party_license_file), license_path)
            self.copy("**/License.txt", root_package="pylon-licenses", dst="pylon/Frameworks/pylon.framework/Versions/A/Resources", ignore_case=True, keep_path=False)

        # Copy the core pylon packages (SDK and runtime only)
        self.copy("*", root_package="pylon-core", dst="pylon", ignore_case=True)

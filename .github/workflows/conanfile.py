import fnmatch
import json
import os
from conan import ConanFile
from conan.tools.files import copy


class GstPluginPylonConanConsumer(ConanFile):
    name = "gst-plugin-pylon"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "build_config": ["ANY"],
        "control_file": ["ANY"],
    }
    default_options = {
        "build_config": "Placeholder overwritten by CI",
        "control_file": "Placeholder overwritten by CI",
    }

    def layout(self):
        self.folders.generators = "."

    @property
    def _platform_name(self):
        arch = str(self.settings.arch).lower()
        os_name = str(self.settings.os).lower()
        if os_name == "linux":
            if arch in ("aarch64", "armv8"):
                return "linux_aarch64"
            return "linux_x86_64"
        if os_name == "windows":
            return "windows"
        if os_name == "macos":
            return "macos"
        return "unknown"

    def requirements(self):
        config_path = str(self.options.build_config)
        with open(config_path, encoding="utf-8") as f:
            config = json.load(f)

        control_path = str(self.options.control_file)
        with open(control_path, encoding="utf-8") as f:
            control = json.load(f)

        resolved_refs = json.loads(os.environ.get("SDK_RESOLVED_REFS", "{}") or "{}")
        reqs = config.get(self._platform_name, {}).get("requirements", ["pylon-core"])
        for req in reqs:
            ref = resolved_refs.get(req)
            if not ref:
                version_map = {pkg["name"]: pkg["version"] for pkg in control}
                version = version_map.get(req)
                if not version:
                    raise ValueError(f"Required package {req!r} missing from control metadata")
                ref = f"{req}/{version}"
            self.requires(ref, force=True)

    def generate(self):
        config_path = str(self.options.build_config)
        with open(config_path, encoding="utf-8") as f:
            config = json.load(f)

        imports = config.get(self._platform_name, {}).get("imports", ["pylon-core"])
        deploy_dst = os.path.join(self.generators_folder, "pylon")
        for _, dep in self.dependencies.items():
            if any(fnmatch.fnmatchcase(dep.ref.name, pattern) for pattern in imports):
                copy(self, "*", src=dep.package_folder, dst=deploy_dst, keep_path=True)

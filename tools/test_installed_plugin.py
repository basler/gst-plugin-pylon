"""Verify that GStreamer and Python load the targets installed by Meson."""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def load_build_options(build_dir):
    options_path = Path(build_dir, "meson-info", "intro-buildoptions.json")
    if not options_path.is_file():
        return {}
    return {
        opt["name"]: opt.get("value")
        for opt in json.loads(options_path.read_text(encoding="utf-8"))
    }


def env_path_value(env):
    for key in ("Path", "PATH"):
        if key in env:
            return env[key]
    return ""


def resolve_gst_tool(name, env):
    """Find gst-inspect-1.0 / gst-launch-1.0 across Unix and Windows layouts."""
    candidates = [name]
    if os.name == "nt":
        candidates.append(f"{name}.exe")

    search_path = env_path_value(env)
    for candidate in candidates:
        found = shutil.which(candidate, path=search_path)
        if found:
            return found

    for key, value in env.items():
        if key.upper().startswith("GSTREAMER_1_0_ROOT") and value:
            for candidate in candidates:
                path = Path(value) / "bin" / candidate
                if path.is_file():
                    return str(path)

    raise FileNotFoundError(
        f"Could not find {name}; searched PATH={search_path!r} and GSTREAMER_1_0_ROOT*"
    )


def check_binding(module, dll_dirs):
    with ExitStack() as stack:
        for directory in dll_dirs:
            stack.enter_context(os.add_dll_directory(directory))
        sys.path.insert(0, str(module.parent))
        import pygstpylon

        if not module.samefile(pygstpylon.__file__):
            raise RuntimeError(f"Expected {module}, imported {pygstpylon.__file__}")


def smoke_test(build_dir):
    manifest = Path(build_dir, "meson-info", "intro-installed.json")
    paths = [Path(p) for p in json.loads(manifest.read_text(encoding="utf-8")).values()]
    options = load_build_options(build_dir)

    plugins = [
        p for p in paths
        if p.parent.name == "gstreamer-1.0" and p.suffix in {".so", ".dylib", ".dll"}
    ]
    if len(plugins) != 1:
        raise RuntimeError(f"Expected one installed GStreamer plugin, found {plugins}")
    plugin = plugins[0]

    modules = [
        p for p in paths
        if p.name.startswith("pygstpylon.") and p.suffix in {".so", ".pyd"}
    ]
    bindings_enabled = options.get("python-bindings") == "enabled"
    if len(modules) > 1:
        raise RuntimeError(f"Expected at most one pygstpylon module, found {modules}")
    if bindings_enabled and not modules:
        raise RuntimeError("python-bindings was enabled, but no installed pygstpylon module was found")
    module = modules[0] if modules else None

    env = os.environ.copy()
    for key in ("GST_PLUGIN_PATH", "GST_PLUGIN_SYSTEM_PATH", "GST_PLUGIN_SYSTEM_PATH_1_0",
                "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH", "PYTHONPATH"):
        env.pop(key, None)

    dll_dirs = []
    if os.name == "nt":
        libraries = [p for p in paths if "gstpylon-1.0" in p.name and p.suffix == ".dll"]
        if len(libraries) != 1:
            raise RuntimeError(f"Expected one gstpylon-1.0 DLL, found {libraries}")
        dll_dirs.append(libraries[0].parent)
        pylon_root = env.get("PYLON_ROOT") or options.get("pylon-root") or ""
        if pylon_root:
            sdk = Path(pylon_root, "Runtime", "x64")
            if sdk.is_dir():
                dll_dirs.append(sdk)
        # Windows has no RPATH; gst-inspect must find dependent DLLs via PATH.
        path_prefix = os.pathsep.join(str(d) for d in dll_dirs)
        env["Path"] = path_prefix + os.pathsep + env_path_value(env)
        env["PATH"] = env["Path"]

    with tempfile.TemporaryDirectory() as tmp:
        env.update(
            GST_PLUGIN_PATH_1_0=str(plugin.parent),
            GST_REGISTRY_1_0=str(Path(tmp, "registry.bin")),
            XDG_CACHE_HOME=tmp,
            LC_ALL="C",
            PYLON_CAMEMU="1",
            PYLONSRC_SKIP_TRANSPORT_LAYERS="usb,gige,cxp,cameralink",
        )

        gst_inspect = resolve_gst_tool("gst-inspect-1.0", env)
        gst_launch = resolve_gst_tool("gst-launch-1.0", env)

        def run(args, **kwargs):
            return subprocess.run(args, env=env, cwd=tmp, check=True, timeout=90, **kwargs)

        info = run([gst_inspect, "pylonsrc"], stdout=subprocess.PIPE).stdout
        filename = re.search(rb"^\s*Filename\s+(.+?)\s*$", info, re.MULTILINE)
        if not filename or not plugin.samefile(os.fsdecode(filename[1])):
            raise RuntimeError(f"Expected {plugin}, inspector reported {filename and filename[1]!r}")
        run([gst_launch, "-q", "pylonsrc", "num-buffers=5", "!", "fakesink"])

        if module is None:
            print(f"Loaded {plugin}; captured five emulated frames (python-bindings disabled).")
            return

        run([sys.executable, "-I", Path(__file__).resolve(), "--binding", module, *dll_dirs])
        print(f"Loaded {plugin} and {module}; captured five emulated frames.")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit(f"usage: {sys.argv[0]} <meson-build-dir>")
    if sys.argv[1] == "--binding":
        check_binding(Path(sys.argv[2]), sys.argv[3:])
    else:
        smoke_test(sys.argv[1])

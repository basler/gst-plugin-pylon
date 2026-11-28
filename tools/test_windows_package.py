"""Test the Windows release archive the way a user installs it.

Runs on a machine that has only the GStreamer runtime MSI and the pylon
runtime installed, with the archive extracted to an arbitrary directory. No
build tree, Visual Studio environment, or Meson devenv is involved; the only
setup is what the README documents.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

BINDING_CHECK = r"""
import os, sys
for d in sys.argv[2:]:
    os.add_dll_directory(d)
sys.path.insert(0, sys.argv[1])
import pygstpylon
assert hasattr(pygstpylon, "GstPylonMeta"), dir(pygstpylon)
assert hasattr(pygstpylon, "gst_buffer_get_pylon_meta"), dir(pygstpylon)
print(f"pygstpylon {pygstpylon.__file__} imported by Python {sys.version.split()[0]}")
"""


def user_env(package, gst_root, pylon_runtime, tmp):
    """A fresh user session: system dirs plus the documented PATH entries."""
    system_root = os.environ.get("SystemRoot", r"C:\Windows")
    path = [
        package / "bin",
        gst_root / "bin",
        pylon_runtime,
        Path(system_root, "System32"),
        Path(system_root),
    ]
    env = {
        key: os.environ[key]
        for key in ("SystemRoot", "SystemDrive", "windir", "COMSPEC", "PATHEXT",
                    "USERPROFILE", "LOCALAPPDATA", "APPDATA", "PROGRAMDATA",
                    "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE")
        if key in os.environ
    }
    env.update(
        PATH=os.pathsep.join(str(p) for p in path),
        TEMP=tmp,
        TMP=tmp,
        GST_PLUGIN_PATH_1_0=str(package / "lib" / "gstreamer-1.0"),
        GST_REGISTRY_1_0=str(Path(tmp, "registry.bin")),
        PYLON_CAMEMU="1",
        PYLONSRC_SKIP_TRANSPORT_LAYERS="usb,gige,cxp,cameralink",
    )
    return env


def check_stable_abi(module):
    """The module must import python3.dll only, never python3XY.dll."""
    data = module.read_bytes()
    versioned = sorted({m.decode().lower()
                        for m in re.findall(rb"python3\d+\.dll", data, re.IGNORECASE)})
    if versioned:
        raise RuntimeError(f"{module.name} is bound to {versioned}; expected python3.dll only")
    if not re.search(rb"python3\.dll", data, re.IGNORECASE):
        raise RuntimeError(f"{module.name} does not import python3.dll")
    print(f"{module.name} links the CPython stable ABI (python3.dll)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True,
                        help="extracted release archive")
    parser.add_argument("--gst-root", type=Path, required=True)
    parser.add_argument("--pylon-runtime", type=Path, required=True)
    parser.add_argument("--python", action="append", default=[],
                        help="interpreter that must import pygstpylon (repeatable)")
    args = parser.parse_args()

    package = args.package.resolve()
    plugin = package / "lib" / "gstreamer-1.0" / "gstpylon.dll"
    module = package / "python" / "pygstpylon.pyd"
    for required in (plugin, package / "bin" / "gstpylon-1.0.dll", module):
        if not required.is_file():
            raise RuntimeError(f"Archive is missing {required.relative_to(package)}")

    with tempfile.TemporaryDirectory() as tmp:
        env = user_env(package, args.gst_root, args.pylon_runtime, tmp)

        def run(cmd, **kwargs):
            print("+", " ".join(str(c) for c in cmd), flush=True)
            return subprocess.run([str(c) for c in cmd], env=env, cwd=tmp,
                                  check=True, timeout=120, **kwargs)

        info = run([args.gst_root / "bin" / "gst-inspect-1.0.exe", "pylonsrc"],
                   stdout=subprocess.PIPE).stdout
        filename = re.search(rb"^\s*Filename\s+(.+?)\s*$", info, re.MULTILINE)
        if not filename or not plugin.samefile(os.fsdecode(filename[1])):
            raise RuntimeError(f"Expected {plugin}, inspector reported {filename and filename[1]!r}")
        run([args.gst_root / "bin" / "gst-launch-1.0.exe", "-q",
             "pylonsrc", "num-buffers=5", "!", "fakesink"])
        print(f"Loaded {plugin}; captured five emulated frames.")

        check_stable_abi(module)
        dll_dirs = [package / "bin", args.gst_root / "bin", args.pylon_runtime]
        for python in args.python or [sys.executable]:
            run([python, "-I", "-c", BINDING_CHECK, package / "python", *dll_dirs])


if __name__ == "__main__":
    main()

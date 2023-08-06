#!/usr/bin/env python3
import os
import json
import sys
import shlex
import subprocess


def patch_pylon_path(pylon_base: str, target: str):
    # fetch the library deps from target

    command = ["otool", "-L", target]
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    output = result.stdout

    for line in output.splitlines():
        lib = line.strip().split(" ")[0]
        
        if "@rpath/pylon.framework" in lib:
            abs_lib = lib.replace("@rpath",pylon_base)
            command = [
                "install_name_tool",
                "-change",
                lib,
                abs_lib,
                target,
            ]
        print(" ".join(command))
        subprocess.run(command, check=True)


# execute meson_introspect to get access to all
# build info
if "MESONINTROSPECT" not in os.environ:
    raise RuntimeError("MESONINTROSPECT not found")

mesonintrospect_cmd = os.environ["MESONINTROSPECT"]

command = shlex.split(mesonintrospect_cmd)
command.extend(["-a"])  # dump all state as json

result = subprocess.run(command, capture_output=True, text=True, check=True)
output = result.stdout

# meson state
meson_introspect = json.loads(output)

# get install prefixes
prefix = ""
libdir = ""

for opt in meson_introspect["buildoptions"]:
    if opt["name"] == "prefix":
        prefix = opt["value"]
    if opt["name"] == "libdir":
        libdir = opt["value"]

# get target libraries
target_libs = []
install_targets = meson_introspect["install_plan"]["targets"]
for t_name in install_targets:
    destination_path = meson_introspect["install_plan"]["targets"][t_name][
        "destination"
    ].replace("libdir_shared", "libdir")
    target_libs.append(
        os.path.join(
            prefix, destination_path.format(libdir=libdir, shared_libdir=libdir)
        )
    )

pylon_path = ""
for dep in meson_introspect["dependencies"]:
    # extract pylon_path from pylon link_args
    if dep["name"] == "pylon":
        pylon_path = dep["link_args"][0].split("/pylon.framework")[0]
        break

for tgt in target_libs:
    pass
    #patch_pylon_path(pylon_path, tgt)

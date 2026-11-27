#!/usr/bin/env python3
# Copyright (C) 2026 Basler AG
#
# Prints the build identity of gst-plugin-pylon, used as the plugin version
# string and by bug reports. It is run on every build by meson's vcs_tag, so
# it must be cheap and must never fail.
#
#   <version>                 exact release tag v<version> on a clean tree
#   <version>+g<rev>          untagged build from commit <rev> (12 hex digits)
#   <version>+g<rev>.dirty    as above, with uncommitted tracked changes
#   <version>+unknown         no git metadata (plain source tarball)
#
# The output follows the PEP 440 local version format, so Python tooling
# accepts it, and it is a pure function of the source tree: building the same
# commit twice gives the same string. No timestamps or host data are used.
#
# Source archives made with `git archive` carry the commit through
# .git_archival.txt (expanded by export-subst in .gitattributes).

import os
import subprocess
import sys

ARCHIVAL_FILE = ".git_archival.txt"


def _git(source_root, *args):
    try:
        result = subprocess.run(
            ["git", "-C", source_root, *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip()


def _archival_rev(source_root):
    try:
        with open(os.path.join(source_root, ARCHIVAL_FILE), encoding="utf-8") as f:
            line = f.read().strip()
    except OSError:
        return None
    # Unexpanded placeholder means this is a plain checkout, not an archive.
    if not line.startswith("commit=") or "$Format" in line:
        return None
    rev = line.split("=", 1)[1].strip()
    return rev[:12] if rev else None


def build_id(source_root, version):
    if os.path.exists(os.path.join(source_root, ".git")):
        rev = _git(source_root, "rev-parse", "--short=12", "HEAD")
        if rev:
            status = _git(source_root, "status", "--porcelain", "--untracked-files=no")
            dirty = bool(status)
            tag = _git(source_root, "describe", "--exact-match", "--tags", "HEAD")
            if tag == "v" + version and not dirty:
                return version
            return version + "+g" + rev + (".dirty" if dirty else "")

    rev = _archival_rev(source_root)
    if rev:
        return version + "+g" + rev
    return version + "+unknown"


def main(argv):
    if len(argv) != 3:
        print("usage: build_id.py <source-root> <project-version>", file=sys.stderr)
        return 1  # non-zero makes vcs_tag use its fallback string
    print(build_id(argv[1], argv[2]), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

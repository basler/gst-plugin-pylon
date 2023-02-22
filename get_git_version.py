import subprocess
import sys, os

meson_project_path = sys.argv[1]
git_dir = meson_project_path + "/.git"

if not os.path.exists(git_dir):
    sys.exit(1)

cmd_result = subprocess.run(["git", "describe", "--tags"], capture_output=True)
vcs_tag = cmd_result.stdout.decode('utf-8')[:-1] 

if not len(vcs_tag):
    sys.exit(1)
else:
    vcs_tag = vcs_tag[1:]

print(vcs_tag)
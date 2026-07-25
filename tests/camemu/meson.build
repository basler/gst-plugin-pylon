# Functional camemu tests for pylonsrc.
# The harness is a Bash script (pipefail, [[ ]], etc.); do not run it with dash.
bash = find_program('bash')
camemu_script = files('run_camemu_tests.sh')
build_root = meson.project_build_root()
source_root = meson.project_source_root()
install_prefix = source_root / 'install'

test(
  'camemu_pylonsrc',
  bash,
  args: [camemu_script],
  env: {
    'BUILD_DIR': build_root,
    'BUILD_PREFIX': install_prefix,
    'PYLON_CAMEMU': '3',
    'PYLON_ROOT': '/opt/pylon',
  },
  timeout: 300,
  workdir: source_root,
)

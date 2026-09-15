# Functional camemu tests for pylonsrc (PYLON_CAMEMU on Linux, macOS, Windows).
# The harness is a Bash script (pipefail, [[ ]], etc.); do not run it with dash.
bash = find_program('bash', required: false)
if not bash.found()
  warning('Skipping camemu tests: bash not found')
else
  camemu_script = files('run_camemu_tests.sh')
  build_root = meson.project_build_root()
  source_root = meson.project_source_root()
  install_prefix = source_root / 'install'

  # Honor PYLON_ROOT from the environment (CI unpacks Conan to a workspace path).
  camemu_pylon_root = run_command(python3, '-c',
    'import os; print(os.environ.get("PYLON_ROOT", "/opt/pylon"))',
    check: false).stdout().strip()

  test(
    'camemu_pylonsrc',
    bash,
    args: [camemu_script],
    env: {
      'BUILD_DIR': build_root,
      'BUILD_PREFIX': install_prefix,
      'PYLON_CAMEMU': '3',
      'PYLON_ROOT': camemu_pylon_root,
      # Meson injects MALLOC_PERTURB_ which defeats malloc_trim RSS checks.
      'MALLOC_PERTURB_': '0',
    },
    timeout: 600,
    is_parallel: false,
    workdir: source_root,
  )
endif

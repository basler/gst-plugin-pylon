# Compatibility test: ensures no API/behavior breaks vs baseline
# Uses PYLON_CAMEMU so no physical camera needed.
# Works with build dir (no install) or install prefix.

compat_script = files('run_compat_tests.sh')
build_root = meson.project_build_root()
source_root = meson.project_source_root()
# Prefer install dir if it exists (e.g. CI runs install first)
install_prefix = source_root / 'install'

bash = find_program('bash', required: false)
if not bash.found()
  warning('Skipping compat tests: bash not found')
else
  compat_pylon_root = run_command(python3, '-c',
    'import os; print(os.environ.get("PYLON_ROOT", "/opt/pylon"))',
    check: false).stdout().strip()

  test(
    'compat_pylonsrc',
    bash,
    args: [compat_script],
    env: {
      'BUILD_DIR': build_root,
      'BUILD_PREFIX': install_prefix,
      'PYLON_CAMEMU': '2',
      'PYLON_ROOT': compat_pylon_root,
    },
    timeout: 90,
    workdir: source_root,
  )
endif

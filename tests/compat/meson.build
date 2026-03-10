# Compatibility test: ensures no API/behavior breaks vs baseline
# Uses PYLON_CAMEMU so no physical camera needed.
# Works with build dir (no install) or install prefix.

compat_script = join_paths(meson.current_source_dir(), 'run_compat_tests.sh')
build_root = meson.project_build_root()
source_root = meson.project_source_root()
# Prefer install dir if it exists (e.g. CI runs install first)
install_prefix = source_root / 'install'

test(
  'compat_pylonsrc',
  find_program('sh'),
  args: [
    '-c',
    'BUILD_DIR="@0@" BUILD_PREFIX="@1@" @2@'.format(
      build_root,
      install_prefix,
      compat_script,
    ),
  ],
  env: ['PYLON_CAMEMU=2'],
  timeout: 90,
  workdir: source_root,
)

# Functional camemu tests (shell + optional PyGObject appsink check)
camemu_script = join_paths(meson.current_source_dir(), 'run_camemu_tests.sh')
build_root = meson.project_build_root()
source_root = meson.project_source_root()
install_prefix = source_root / 'install'

test(
  'camemu_pylonsrc',
  find_program('sh'),
  args: [
    '-c',
    'BUILD_DIR="@0@" BUILD_PREFIX="@1@" @2@'.format(
      build_root,
      install_prefix,
      camemu_script,
    ),
  ],
  env: ['PYLON_CAMEMU=3'],
  timeout: 300,
  workdir: source_root,
)

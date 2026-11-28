# Functional camemu tests for pylonsrc (PYLON_CAMEMU on Linux, macOS, Windows).
# The harness is a Bash script (pipefail, [[ ]], etc.); do not run it with dash.
bash = find_program('bash', required: false)
if not bash.found()
  warning('Skipping camemu tests: bash not found')
else
  camemu_script = files('run_camemu_tests.sh')
  test_env = pylon_build_env
  test_env.set('PYLON_CAMEMU', '3')
  test_env.set('GST_REGISTRY_1_0', meson.current_build_dir() / 'registry.bin')
  test_env.set('MALLOC_PERTURB_', '0')
  test(
    'camemu_pylonsrc',
    bash,
    args: [camemu_script],
    env: test_env,
    timeout: 600,
    is_parallel: false,
    workdir: meson.project_source_root(),
    # Drives the system gst-launch/gst-inspect binaries; see the sanitizer CI job.
    suite: ['shell'],
  )
endif

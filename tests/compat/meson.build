# Compatibility test: ensures no API/behavior breaks vs baseline
# Uses PYLON_CAMEMU so no physical camera needed.
test('compat_snapshot_comparison', python3, args: files('test_run_ab_compat.py'))

compat_script = files('run_compat_tests.sh')
bash = find_program('bash', required: false)
if not bash.found()
  warning('Skipping compat tests: bash not found')
else
  test_env = pylon_build_env
  test_env.set('PYLON_CAMEMU', '2')
  test_env.set('GST_REGISTRY_1_0', meson.current_build_dir() / 'registry.bin')
  test(
    'compat_pylonsrc',
    bash,
    args: [compat_script],
    env: test_env,
    timeout: 90,
    workdir: meson.project_source_root(),
    # Drives the system gst-launch/gst-inspect binaries; see the sanitizer CI job.
    suite: ['shell'],
  )
endif

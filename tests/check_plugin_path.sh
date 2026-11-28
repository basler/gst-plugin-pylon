# Sourced by both Bash suites before accepting any inspector output.
check_plugin_path() {
  : "${PYLON_TEST_PLUGIN:?Run this script through meson devenv -C <builddir>}"
  local actual
  actual="$(LC_ALL=C gst-inspect-1.0 pylonsrc | sed -nE 's/^[[:space:]]*Filename[[:space:]]+//p' | tr -d '\r')"
  if ! [[ "$actual" -ef "$PYLON_TEST_PLUGIN" ]]; then
    echo "Expected $PYLON_TEST_PLUGIN, gst-inspect loaded: $actual" >&2
    return 1
  fi
}

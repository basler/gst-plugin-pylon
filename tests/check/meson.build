valgrind_dep = dependency('valgrind', required: false).partial_dependency(compile_args : true)
if valgrind_dep.found()
  cdata.set('HAVE_VALGRIND', 1)
endif

# name, condition when to skip the test and extra dependencies
pylon_tests = [
  [ 'generic/states' ],
]

test_defines = [
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
  '-DGST_CHECK_TEST_ENVIRONMENT_BEACON="GST_PLUGIN_LOADING_WHITELIST"',
  '-DGST_TEST_FILES_PATH="' + meson.current_source_dir() + '/../files"',
  '-DGST_USE_UNSTABLE_API',
]

pluginsdirs = []
if gst_dep.type_name() == 'pkgconfig'
  pbase = dependency('gstreamer-plugins-base-' + api_version, required: true)
  pluginsdirs = [gst_dep.get_variable(pkgconfig : 'pluginsdir'),
                 pbase.get_variable(pkgconfig : 'pluginsdir')]
  gst_plugin_scanner_dir = gst_dep.get_variable(pkgconfig : 'pluginscannerdir', default_value : '')
else
  gst_plugin_scanner_dir = subproject('gstreamer').get_variable('gst_scanner_dir')
endif


## workaround for gstreamer isssue on e.g. Ubuntu 22.04 x86_64 and aarch6
if gst_plugin_scanner_dir != '' and not fs.exists(gst_plugin_scanner_dir)
  if host_machine.cpu_family() == 'x86_64'
    gst_plugin_scanner_dir = '/usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/'
  elif host_machine.cpu_family() == 'aarch64'
    gst_plugin_scanner_dir = '/usr/lib/aarch64-linux-gnu/gstreamer1.0/gstreamer-1.0/'
  endif
endif
##

gst_plugin_scanner_path = join_paths(gst_plugin_scanner_dir, 'gst-plugin-scanner')

state_ignore_elements = '''
'''

test_deps = [gst_dep, gstbase_dep, gstcheck_dep, gstvideo_dep,
  gstpbutils_dep, valgrind_dep] + glib_deps

# FIXME: add valgrind suppression common/gst.supp gst-plugins-good.supp
foreach t : pylon_tests
  fname = '@0@.c'.format(t.get(0))
  test_name = t.get(0).underscorify()
  extra_sources = t.get(3, [ ])
  extra_deps = t.get(2, [ ])
  skip_test = t.get(1, false)
  if not skip_test
    env = environment()
    env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
    env.set('GST_STATE_IGNORE_ELEMENTS', state_ignore_elements)
    env.set('CK_DEFAULT_TIMEOUT', '20')
    env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer', 'gst-plugins-base',
        'gst-plugin-pylon@' + meson.global_build_root())
    env.set('GST_PLUGIN_PATH_1_0', [meson.global_build_root()] + pluginsdirs)
    env.set('GSETTINGS_BACKEND', 'memory')

    env.set('GST_REGISTRY', join_paths(meson.current_build_dir(), '@0@.registry'.format(test_name)))
    env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)
    exe = executable(test_name, fname, extra_sources,
      include_directories : [configinc],
      c_args : ['-DHAVE_CONFIG_H=1' ] + test_defines,
      dependencies : test_deps + extra_deps,
    )
    test(test_name, exe, env: env, timeout: 3 * 60)
  endif
endforeach

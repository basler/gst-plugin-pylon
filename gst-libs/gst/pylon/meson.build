# gst-plugin-pylon
# this plugin has to link against the basler pylon runtime libraries
#

# Resolve pylon SDK root from -Dpylon-root, falling back to PYLON_ROOT env var.
pylon_path = get_option('pylon-root')
if pylon_path == ''
  pylon_path = run_command(python3, '-c',
    'import os; print(os.environ.get("PYLON_ROOT", ""))',
    check: false).stdout().strip()
endif

fs = import('fs')

pylon_include_ok = pylon_path != '' and (
  fs.exists(pylon_path / 'include/pylon') or
  fs.exists(pylon_path / 'Development/include/pylon') or
  fs.exists(pylon_path / 'Library/Frameworks/pylon.framework/Headers') or
  fs.exists(pylon_path / 'Frameworks/pylon.framework/Headers') or
  fs.exists(pylon_path / 'Frameworks/pylon.framework/Versions/A/Headers'))

if (host_system == 'linux' or host_system == 'darwin') and not pylon_include_ok
  error('Set -Dpylon-root or PYLON_ROOT to a pylon install (Linux: include/pylon, macOS: Frameworks/pylon.framework)')
endif

# From-source builds still accept pylon 7.1+ (CMake package) and the 6.x
# fallback. Debian packages separately Depend on pylon (>= 26.06).
pylon_cmake_prefixes = []
if pylon_path != ''
  pylon_cmake_prefixes += [
    pylon_path,
    pylon_path / 'share/pylon/cmake',
    pylon_path / 'lib/cmake/pylon',
  ]
  if host_system == 'windows'
    pylon_cmake_prefixes += [pylon_path / 'Development/CMake/pylon']
  endif
  if host_system == 'darwin'
    pylon_cmake_prefixes += [
      pylon_path / 'Library/Frameworks/pylon.framework/Resources',
      pylon_path / 'Frameworks/pylon.framework/Resources',
      pylon_path / 'Frameworks/pylon.framework/Versions/A/Resources',
      pylon_path / 'Frameworks/pylon.framework',
    ]
  endif
endif

pylon_cmake_args = []
if pylon_path != ''
  pylon_cmake_args += ['-DPYLON_ROOT=' + pylon_path]
endif
if pylon_cmake_prefixes.length() > 0
  pylon_cmake_args += ['-DCMAKE_PREFIX_PATH=' + ';'.join(pylon_cmake_prefixes)]
endif

pylon_dep = dependency('pylon', method : 'cmake',
        modules : ['pylon::PylonBase', 'pylon::PylonUtility'],
        version : '>=7.1',
        cmake_args : pylon_cmake_args,
        required : false)

if not pylon_dep.found()
  message('fallback to detect pylon 6.x')
  pylon_dep = dependency('pylon', method : 'cmake',
          cmake_module_path: meson.project_source_root() + '/cmake',
          cmake_args : pylon_cmake_args,
          required : true)
endif

if host_system == 'linux' and pylon_path != ''
  pylon_rpath = pylon_path / 'lib'
else
  pylon_rpath = ''
endif

pylon_rpath_opt = get_option('pylon-rpath')
if host_system != 'linux' or pylon_rpath_opt == 'none'
  pylon_install_rpath = ''
elif pylon_rpath_opt != ''
  pylon_install_rpath = pylon_rpath_opt
elif pylon_path != ''
  if meson.is_cross_build()
    pylon_install_rpath = ''
    pylon_target_root = ''
    if cross_sysroot != ''
      pylon_target_root = run_command(python3, '-c',
        'import posixpath, sys\n' +
        'root = posixpath.normpath(sys.argv[1].replace(chr(92), "/"))\n' +
        'path = posixpath.normpath(sys.argv[2].replace(chr(92), "/"))\n' +
        'if root != "/" and path.startswith(root + "/"):\n' +
        '    print(path[len(root):])\n',
        cross_sysroot, pylon_path,
        check: true).stdout().strip()
      if pylon_target_root != '' and pylon_target_root != '/usr'
        pylon_install_rpath = pylon_target_root + '/lib'
      endif
    endif
    if pylon_install_rpath == '' and pylon_target_root != '/usr'
      message('Cross-building with pylon-root outside sys_root; omitting pylon install_rpath (set -Dpylon-rpath=/opt/pylon/lib if needed on target)')
    endif
  else
    pylon_install_rpath = pylon_rpath
  endif
else
  pylon_install_rpath = ''
endif

gstpylon_sources = [ 
  'gstpyloncache.cpp',
  'gstpylondebug.cpp',
  'gstpylonfeatureprior.cpp',
  'gstpylonfeaturewalker.cpp',
  'gstpyloninspectcache.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonlimitprobe.cpp',
  'gstpylonmeta.cpp',
  'gstpylonobject.cpp',
  'gstpylonparamspecs.cpp',
  'gstpylonparamfactory.cpp',
  'gstpylontlenumerate.cpp',
]

gstpylon_headers = [
  'gstpylon-prelude.h',
  'gstpylonmeta.h',
]

install_headers(gstpylon_headers, subdir : 'gstreamer-1.0/gst/pylon/')

gstpylon_cpp_args = gst_plugin_pylon_args + ['-DBUILDING_EXT_PYLONSRC']
if get_option('test-hooks')
  gstpylon_cpp_args += ['-DGST_PYLON_ENABLE_TEST_HOOKS']
endif

gstpylon = library('gstpylon-' + api_version,
  gstpylon_sources,
  c_args : gst_plugin_pylon_args + '-DBUILDING_EXT_PYLONSRC',
  cpp_args : gstpylon_cpp_args,
  link_args : [noseh_link_args],
  gnu_symbol_visibility: 'inlineshidden',
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  build_rpath : pylon_rpath,
  install_rpath : pylon_install_rpath
)

# generate pkgconfig file
pkgconfig.generate(gstpylon, description : 'Basler Pylon gstreamer source')

# set dependency for plugin code examples
gstpylon_dep = declare_dependency(link_with : gstpylon,
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep])

# For examples to mimic what they would to on an installed setud
meson.override_dependency('gstpylon', gstpylon_dep)


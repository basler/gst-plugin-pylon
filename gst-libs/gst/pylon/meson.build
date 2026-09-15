# gst-plugin-pylon
# this plugin has to link against the basler pylon runtime libraries
#

# import PYLON_ROOT environment into meson
pylon_path = run_command(python3, '-c', 'import os; print(os.environ.get("PYLON_ROOT","PYLON_ROOT_NOT_SET"))', check: false).stdout().strip()

fs = import('fs')

pylon_include_ok = pylon_path != 'PYLON_ROOT_NOT_SET' and (
  fs.exists(pylon_path / 'include/pylon') or
  fs.exists(pylon_path / 'Development/include/pylon') or
  fs.exists(pylon_path / 'Library/Frameworks/pylon.framework/Headers') or
  fs.exists(pylon_path / 'Frameworks/pylon.framework/Headers') or
  fs.exists(pylon_path / 'Frameworks/pylon.framework/Versions/A/Headers'))

if (target_machine.system() == 'linux' or target_machine.system() == 'darwin') and not pylon_include_ok
  error('PYLON_ROOT has to be set to a pylon install (Linux: include/pylon, macOS: Frameworks/pylon.framework)')
endif

# From-source builds still accept pylon 7.1+ (CMake package) and the 6.x
# fallback. Debian packages separately Depend on pylon (>= 26.06).
pylon_cmake_prefixes = []
if pylon_path != 'PYLON_ROOT_NOT_SET'
  pylon_cmake_prefixes += [
    pylon_path,
    pylon_path / 'share/pylon/cmake',
    pylon_path / 'lib/cmake/pylon',
  ]
  if target_machine.system() == 'windows'
    pylon_cmake_prefixes += [pylon_path / 'Development/CMake/pylon']
  endif
  if target_machine.system() == 'darwin'
    pylon_cmake_prefixes += [
      pylon_path / 'Library/Frameworks/pylon.framework/Resources',
      pylon_path / 'Frameworks/pylon.framework/Resources',
      pylon_path / 'Frameworks/pylon.framework/Versions/A/Resources',
      pylon_path / 'Frameworks/pylon.framework',
    ]
  endif
endif

pylon_cmake_args = ['-DPYLON_ROOT=' + pylon_path]
foreach prefix : pylon_cmake_prefixes
  pylon_cmake_args += ['-DCMAKE_PREFIX_PATH=' + prefix]
endforeach

pylon_dep = dependency('pylon', method : 'cmake',
        modules : ['pylon::PylonBase', 'pylon::PylonUtility'],
        version : '>=7.1',
        cmake_args : pylon_cmake_args,
        required : false)

if pylon_dep.found()
  if target_machine.system() == 'linux'
    pylon_rpath = pylon_path / 'lib'
  else
    pylon_rpath = ''
  endif
else
  message('fallback to detect pylon 6.x')
  pylon_dep = dependency('pylon', method : 'cmake',
          cmake_module_path: meson.project_source_root() + '/cmake',
          required : true)
  pylon_rpath = ''
endif

gstpylon_sources = [ 
  'gstpyloncache.cpp',
  'gstpylondebug.cpp',
  'gstpylonfeaturewalker.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonmeta.cpp',
  'gstpylonobject.cpp',
  'gstpylonparamspecs.cpp',
  'gstpylonparamfactory.cpp',
]

gstpylon_headers = [
  'gstpylon-prelude.h',
  'gstpylonmeta.h',
]

install_headers(gstpylon_headers, subdir : 'gstreamer-1.0/gst/pylon/')

gstpylon = library('gstpylon-' + api_version,
  gstpylon_sources,
  c_args : gst_plugin_pylon_args + '-DBUILDING_EXT_PYLONSRC',
  cpp_args : gst_plugin_pylon_args + '-DBUILDING_EXT_PYLONSRC',
  link_args : [noseh_link_args],
  gnu_symbol_visibility: 'inlineshidden',
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_rpath : pylon_rpath
)

# generate pkgconfig file
pkgconfig.generate(gstpylon, description : 'Basler Pylon gstreamer source')

# set dependency for plugin code examples
gstpylon_dep = declare_dependency(link_with : gstpylon,
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep])

# For examples to mimic what they would to on an installed setud
meson.override_dependency('gstpylon', gstpylon_dep)


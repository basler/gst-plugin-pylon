# gst-plugin-pylon
# this plugin has to link against the basler pylon runtime libraries
#

# import PYLON_ROOT environment into meson
pylon_path = run_command(python3, '-c', 'import os; print(os.environ.get("PYLON_ROOT","PYLON_ROOT_NOT_SET"))', check: false).stdout().strip()

if target_machine.system() == 'linux' and pylon_path == 'PYLON_ROOT_NOT_SET'
  # PYLON_ROOT has to be set
  error('PYLON_ROOT has to be set to location of pylon install e.g. /opt/pylon')
endif

pylon_config = find_program ('pylon-config', pylon_path / 'bin' / 'pylon-config', required : false, disabler : true)
if not pylon_config.found()
   error('Pylon SDK not found, is it installed? If you installed it in alternative location you may specify it using -Dpylon-path=')
endif


pylon_c_args = run_command(pylon_config, '--cflags', check : true).stdout().strip().split()
pylon_link_args = run_command(pylon_config, '--libs', check : true).stdout().strip().split()

pylon_dep = declare_dependency (
    compile_args : pylon_c_args,
    link_args : pylon_link_args
)

# runtime path is properly defined by cmake
pylon_link_args = []

pylon_sources = [
  'gstpylonsrc.c',
  'gstpylonplugin.c',
  'gstpylonmeta.cpp',
  'gstchildinspector.cpp',
  'gstpylonobject.cpp',
  'gstpylon.cpp',
  'gstpylonintrospection.cpp',
  'gstpylonparamspecs.cpp',
  'gstpylonfeaturewalker.cpp',
  'gstpylonimagehandler.cpp',
  'gstpylondisconnecthandler.cpp'
]

gstpylon = library('gstpylon',
  pylon_sources,
  c_args : gst_plugin_pylon_args,
  link_args : [noseh_link_args, pylon_link_args],
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_dir : plugins_install_dir,
  install_rpath: pylon_path / 'lib'
)

pkgconfig.generate(gstpylon, install_dir : plugins_pkgconfig_install_dir)
plugins += [gstpylon]


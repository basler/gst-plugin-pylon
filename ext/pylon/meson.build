pylon_path = get_option('pylon-path')
if pylon_path == ''
  pylon_path = '/opt/pylon'
endif

pylon_rpath = get_option('pylon-rpath')
if pylon_rpath == ''
  pylon_rpath = pylon_path + '/lib'
endif

pylon_config = find_program ('pylon-config', dirs: pylon_path + '/bin/', required : false, disabler : true)
if not pylon_config.found()
  error('Pylon SDK not found, is it installed? If you installed it in alternative location you may specify it using -Dpylon-path=')
endif

message('Pylon path: ' + pylon_path)
message('Pylon rpath: ' + pylon_rpath)

pylon_c_args = run_command(pylon_config, '--cflags', check : true).stdout().strip().split()
pylon_link_args = run_command(pylon_config, '--libs', check : true).stdout().strip().split()

pylon_dep = declare_dependency (
  compile_args : pylon_c_args,
  link_args : pylon_link_args
)

pylon_sources = [
  'gstpylonsrc.c',
  'gstpylonplugin.c',
  'gstpylon.cpp'
]

gstpylon = library('gstpylon',
  pylon_sources,
  c_args : gst_plugin_pylon_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_dir : plugins_install_dir,
  install_rpath : pylon_rpath,
)

pkgconfig.generate(gstpylon, install_dir : plugins_pkgconfig_install_dir)
plugins += [gstpylon]

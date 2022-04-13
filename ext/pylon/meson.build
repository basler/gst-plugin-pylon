pylon_dep = dependency('pylon')

gstpylon = library('gstpylon',
  'gstpylonsrc.c',
  c_args : gst_plugin_pylon_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, pylon_dep],
  install : true,
  install_dir : plugins_install_dir,
)
pkgconfig.generate(gstpylon, install_dir : plugins_pkgconfig_install_dir)
plugins += [gstpylon]

examples = [
  'pylonsrc',
  'list_properties',
]

examples_meta = [
 'show_meta'
]

# demonstrate usage of pylonsrc
foreach example : examples
  executable(example, example + '.c',
    dependencies: [gstvideo_dep, gst_dep],
    c_args : gst_plugin_pylon_args,
    include_directories : [configinc],
    install: false)
endforeach

# demonstrate usage of pylonsrc with access to metadata
pylonsrc_dep = dependency('gstpylon')
foreach example_meta : examples_meta
  executable(example_meta, example_meta + '.c',
    dependencies: [gstvideo_dep, gst_dep, pylonsrc_dep],
    c_args : gst_plugin_pylon_args,
    include_directories : [configinc],
    install: false)
endforeach


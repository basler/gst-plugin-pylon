examples = [
  'pylonsrc',
  'list_properties'
]

foreach example : examples
  executable(example, example + '.c',
    dependencies: [gstvideo_dep, gst_dep],
    c_args : gst_plugin_pylon_args,
    include_directories : [configinc],
    install: false)
endforeach

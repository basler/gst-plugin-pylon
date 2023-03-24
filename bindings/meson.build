py_mod = import('python')
py3 = py_mod.find_installation('python3')
py3_dep = py3.dependency()
message(py3.path())
message(py3.get_install_dir())

pygstpylon_sources = [
  'src/pygstpylon.cpp',
  'src/bindpylonmeta.cpp',
  'src/bindaccessfunctions.cpp'
]

pybind11_dep = dependency('pybind11', fallback : ['pybind11', 'pybind11_dep'])
gstpylon_dep = dependency('gstpylon')

pygstpylon_deps = [
    py3_dep,
    pybind11_dep,
    gstvideo_dep,
    gst_dep,
    gstpylon_dep
]

destdir = get_option('libdir') / 'python' + py3_dep.version() / 'site-packages/pygstpylon'
incdir = include_directories('include')

pymeta = py3.extension_module('pygstpylon',
                      pygstpylon_sources,
                      install : true,
                      cpp_args: ['-DHAVE_CONFIG_H=1'],
                      include_directories : [configinc, incdir],
                      gnu_symbol_visibility: 'inlineshidden',
                      dependencies : pygstpylon_deps,
                      )




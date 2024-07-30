project('gst-plugin-pylon', 'c', 'cpp',
  version : '0.7.3',
  meson_version : '>= 0.61',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

glib_req = '>= 2.56.0'

# TODO: Set this accordingly
gst_req = '>= 1.0.0'
gst_dep = dependency('gstreamer-1.0', version : gst_req)

gst_version = gst_dep.version()
message('Building against GStreamer ' + gst_version)

python3 = import('python').find_installation()

# generate project version from git info
py_script = '''
import subprocess
import sys, os

meson_project_path = sys.argv[1]

git_dir = os.path.join(meson_project_path, ".git")

try:
    cmd_result = subprocess.run(
        ["git", "--git-dir=" + git_dir, "describe", "--tags", "--abbrev=7", "--dirty=+"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=True,
    )
    vcs_tag = cmd_result.stdout.decode("utf-8")[1:-1]
except (FileNotFoundError, subprocess.CalledProcessError):
    sys.exit(1)

print(vcs_tag)
'''
git_version = vcs_tag(command : ['python3', '-c', py_script, meson.project_source_root()],
             input: 'version.h.in', output: 'version.h', replace_string: '@GIT_VERSION@',
             fallback: meson.project_version() + '-local')

version_arr = gst_version.split('.')
gst_version_major = version_arr[0].to_int()
gst_version_minor = version_arr[1].to_int()
gst_version_micro = version_arr[2].to_int()
 if version_arr.length() == 4
  gst_version_nano = version_arr[3].to_int()
else
  gst_version_nano = 0
endif
gst_version_is_dev = gst_version_minor % 2 == 1 and gst_version_micro < 90

api_version = '1.0'

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')
plugins = []

cc = meson.get_compiler('c')
host_system = host_machine.system()

if cc.get_id() == 'msvc'
  msvc_args = [
      # Ignore several spurious warnings for things gstreamer does very commonly
      # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
      # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
      # NOTE: Only add warnings here if you are sure they're spurious
      '/wd4018', # implicit signed/unsigned conversion
      '/wd4146', # unary minus on unsigned (beware INT_MIN)
      '/wd4244', # lossy type conversion (e.g. double -> int)
      '/wd4305', # truncating type conversion (e.g. double -> float)
      cc.get_supported_arguments(['/utf-8']), # set the input encoding to utf-8

      # Enable some warnings on MSVC to match GCC/Clang behaviour
      '/w14062', # enumerator 'identifier' in switch of enum 'enumeration' is not handled
      '/w14101', # 'identifier' : unreferenced local variable
      '/w14189', # 'identifier' : local variable is initialized but not referenced
  ]
  add_project_arguments(msvc_args, language: ['c', 'cpp'])

  # Disable SAFESEH with MSVC for plugins and libs that use external deps that
  # are built with MinGW
  noseh_link_args = ['/SAFESEH:NO']
else
  noseh_link_args = []
endif

if cc.has_link_argument('-Wl,-Bsymbolic-functions')
  add_project_link_arguments('-Wl,-Bsymbolic-functions', language : 'c')
endif

# Disable strict aliasing
if cc.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'c')
endif

# Define G_DISABLE_DEPRECATED for development versions
if gst_version_is_dev
  message('Disabling deprecated GLib API')
  add_project_arguments('-DG_DISABLE_DEPRECATED', language: 'c')
endif

cast_checks = get_option('gobject-cast-checks')
if cast_checks.disabled() or (cast_checks.auto() and not gst_version_is_dev)
  message('Disabling GLib cast checks')
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')
endif

glib_asserts = get_option('glib-asserts')
if glib_asserts.disabled() or (glib_asserts.auto() and not gst_version_is_dev)
  message('Disabling GLib asserts')
  add_project_arguments('-DG_DISABLE_ASSERT', language: 'c')
endif

glib_checks = get_option('glib-checks')
if glib_checks.disabled() or (glib_checks.auto() and not gst_version_is_dev)
  message('Disabling GLib checks')
  add_project_arguments('-DG_DISABLE_CHECKS', language: 'c')
endif

cdata = configuration_data()

check_headers = [
#  ['HAVE_DLFCN_H', 'dlfcn.h'],
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

check_functions = [
#  ['HAVE_ASINH', 'asinh', '#include<math.h>'],
]

foreach f : check_functions
  if cc.has_function(f.get(1), prefix : f.get(2), dependencies : [])
    cdata.set(f.get(0), 1)
  endif
endforeach

cdata.set('HAVE_IOS', host_system == 'ios')

cdata.set('SIZEOF_CHAR', cc.sizeof('char'))
cdata.set('SIZEOF_INT', cc.sizeof('int'))
cdata.set('SIZEOF_LONG', cc.sizeof('long'))
cdata.set('SIZEOF_SHORT', cc.sizeof('short'))
cdata.set('SIZEOF_VOIDP', cc.sizeof('void*'))
cdata.set('SIZEOF_OFF_T', cc.sizeof('off_t'))

# Here be fixmes.
# FIXME: check if this is correct
cdata.set('HAVE_CPU_X86_64', host_machine.cpu() == 'amd64')
cdata.set('HAVE_GCC_ASM', cc.get_id() != 'msvc')
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('GST_PACKAGE_LICENSE', 'BSD')
cdata.set_quoted('PACKAGE', 'gst-plugin-pylon')
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

# Symbol visibility
if cc.get_id() == 'msvc'
  export_define = '__declspec(dllexport) extern'
elif cc.has_argument('-fvisibility=hidden')
  add_project_arguments('-fvisibility=hidden', language: 'cpp')
  export_define = 'extern __attribute__ ((visibility ("default")))'
else
  export_define = 'extern'
endif

# Passing this through the command line would be too messy
cdata.set('GST_PYLON_API_EXPORT', export_define)

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
]

warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Waggregate-return',
]

cxx = meson.get_compiler('cpp')

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
  if cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

# GStreamer package name and origin url
gst_package_name = get_option('package-name')
if gst_package_name == ''
  if gst_version_nano == 0
    gst_package_name = 'GStreamer Pylon Plug-in source release'
  elif gst_version_nano == 1
    gst_package_name = 'GStreamer Pylon Plug-in git'
  else
    gst_package_name = 'GStreamer Pylon Plug-in prerelease'
  endif
endif
cdata.set_quoted('GST_PACKAGE_NAME', gst_package_name)
cdata.set_quoted('GST_PACKAGE_ORIGIN', get_option('package-origin'))

# Other Mandatory GST deps
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req)
gstcheck_dep = dependency('gstreamer-check-1.0', version : gst_req,
  required : get_option('tests'))
gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version : gst_req)
gstallocators_dep = dependency('gstreamer-allocators-1.0', version : gst_req)
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req)

glib_deps = [dependency('glib-2.0', version : glib_req),
             dependency('gobject-2.0')]

gst_plugin_pylon_args = ['-DHAVE_CONFIG_H']
configinc = include_directories(['.', 'gst-libs'])
#libsinc = include_directories('gst-libs')

# Disable compiler warnings for unused variables and args if gst debug system is disabled
if gst_dep.type_name() == 'internal'
  gst_debug_disabled = not subproject('gstreamer').get_variable('gst_debug')
else
  # We can't check that in the case of subprojects as we won't
  # be able to build against an internal dependency (which is not built yet)
  gst_debug_disabled = cc.has_header_symbol('gst/gstconfig.h', 'GST_DISABLE_GST_DEBUG', dependencies: gst_dep)
endif

if gst_debug_disabled
  message('GStreamer debug system is disabled')
  if cc.has_argument('-Wno-unused')
    add_project_arguments('-Wno-unused', language: 'c')
  endif
  if cxx.has_argument ('-Wno-unused')
    add_project_arguments('-Wno-unused', language: 'cpp')
  endif
else
  message('GStreamer debug system is enabled')
endif

presetdir = join_paths(get_option('datadir'), 'gstreamer-' + api_version, 'presets')

pkgconfig = import('pkgconfig')
plugins_pkgconfig_install_dir = join_paths(plugins_install_dir, 'pkgconfig')
if get_option('default_library') == 'shared'
  # If we don't build static plugins there is no need to generate pc files
  plugins_pkgconfig_install_dir = disabler()
endif

subdir('gst-libs')
subdir('ext')
subdir('tests')
subdir('docs')

meta_python_bindings = get_option('python-bindings')
if meta_python_bindings.enabled()
  message('Meta python bindings enabled')
  subdir('bindings')
else
  message('Meta python bindings disabled')
endif

configure_file(output : 'config.h', configuration : cdata)

# install hooks if git workspace available
fs = import('fs')
root = meson.project_source_root()
if fs.is_dir(join_paths(root,'.git/hooks'))
  message('install git commit hooks')
  hook_file_src = root / 'hooks' / 'pre-commit.hook'
  hook_file_dst = root / '.git' / 'hooks' / 'pre-commit'
  run_command(python3, '-c', 'import shutil; shutil.copy("@0@", "@1@")'.format(hook_file_src, hook_file_dst), check : true)
endif


if meson.version().version_compare('>= 0.54')
  plugin_names = []
  foreach plugin: plugins
    # FIXME: Use str.subtring() when we can depend on Meson 0.56
    split = plugin.name().split('gst')
    if split.length() == 2
      plugin_names += [split[1]]
    else
      warning('Need substring API in meson >= 0.56 to properly parse plugin name: ' + plugin.name())
      plugin_names += [plugin.name()]
    endif
  endforeach
  summary({'Plugins':plugin_names}, list_sep: ', ')
endif

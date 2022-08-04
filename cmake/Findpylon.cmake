#.rst:
# Findpylon.cmake
# Copyright (c) 2021-2022 Basler AG
# http://www.baslerweb.com
#
# Find and using pylon C++ SDK
# ----------------------------
#
# Try to locate the pylon C++ SDK package.
#
# The ``PYLON_DEV_DIR`` (CMake or Environment) variable should be used
# to pinpoint the pylon C++ SDK package files.
#
# If found, this will define the following variables:
#
# ``pylon_FOUND``
#     True if the pylon C++ SDK package has been found.
#
# ``pylon_INCLUDE_DIRS``
#     List containing needed include directory's for pylon C++ SDK.
#
# ``pylon_LIBRARIES``
#     List containing needed libraries for linking pylon C++ SDK.
#

# Checks the PYLON_DEV_DIR and PYLON_ROOT environment variables
if(DEFINED ENV{PYLON_DEV_DIR})
  set(PYLON_DEV_DIR "$ENV{PYLON_DEV_DIR}")
elseif(DEFINED ENV{PYLON_ROOT})
  set(PYLON_DEV_DIR "$ENV{PYLON_ROOT}")
endif()

if(APPLE)
  # Find pylon framework includes
  find_path(_PYLON_CORE_SDK_INCLUDE_DIR
    NAMES pylon/PylonIncludes.h
    HINTS
      "${PYLON_DEV_DIR}"
      "/Library/Frameworks"
  )

  # Find pylon framework GeniCam includes
  find_path(_PYLON_CORE_SDK_GENICAM_INCLUDE_DIR
    NAMES Base/GCTypes.h
    HINTS
      "${_PYLON_CORE_SDK_INCLUDE_DIR}/Headers"
    PATH_SUFFIXES GenICam
  )

  # Find pylon framework
  find_library(pylon_LIBRARIES
    NAMES pylon
    HINTS
      "${PYLON_DEV_DIR}"
      "/Library/Frameworks"
  )

  # Append found pylon include path
  if(_PYLON_CORE_SDK_INCLUDE_DIR)
    list(APPEND pylon_INCLUDE_DIRS ${_PYLON_CORE_SDK_INCLUDE_DIR})
  endif()

  # Append found pylon GenICam include path
  if(_PYLON_CORE_SDK_GENICAM_INCLUDE_DIR)
    list(APPEND pylon_INCLUDE_DIRS ${_PYLON_CORE_SDK_GENICAM_INCLUDE_DIR})
  endif()

  # Check if pylon framework was found
  if(pylon_INCLUDE_DIRS AND pylon_LIBRARIES)
    set(pylon_FOUND TRUE)
  endif()

elseif(MSVC)
  # Windows include paths
  find_path(_PYLON_CORE_SDK_INCLUDE_DIR
    NAMES pylon/PylonIncludes.h
    HINTS
      "${PYLON_DEV_DIR}/include"
      "C:/Program Files/Basler/pylon 6/Development/include"
  )

  # Search locations for libraries
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Windows 64 bits
    list(APPEND _PYLON_CORE_SDK_LIB_SEARCH_PATH "${_PYLON_CORE_SDK_INCLUDE_DIR}/../lib/x64")
  else()
    # Windows 32 bits
    list(APPEND _PYLON_CORE_SDK_LIB_SEARCH_PATH "${_PYLON_CORE_SDK_INCLUDE_DIR}/../lib/Win32")
  endif()

  # Find locations of pylon libraries
  find_library(_PYLON_CORE_SDK_BASE_LIBRARY
    NAMES PylonBase_v6_3.lib
    PATHS ${_PYLON_CORE_SDK_LIB_SEARCH_PATH}
  )
  find_library(_PYLON_CORE_SDK_GCBASE_LIBRARY
    NAMES GCBase_MD_VC141_v3_1_Basler_pylon.lib
    PATHS ${_PYLON_CORE_SDK_LIB_SEARCH_PATH}
  )
  find_library(_PYLON_CORE_SDK_GENAPI_LIBRARY
    NAMES GenApi_MD_VC141_v3_1_Basler_pylon.lib
    PATHS ${_PYLON_CORE_SDK_LIB_SEARCH_PATH}
  )
  find_library(_PYLON_CORE_SDK_UTILITY_LIBRARY
    NAMES PylonUtility_v6_3.lib
    PATHS ${_PYLON_CORE_SDK_LIB_SEARCH_PATH}
  )
  find_library(_PYLON_CORE_SDK_GUI_LIBRARY
    NAMES PylonGUI_v6_3.lib
    PATHS ${_PYLON_CORE_SDK_LIB_SEARCH_PATH}
  )

  # Append found pylon include path
  if(_PYLON_CORE_SDK_INCLUDE_DIR)
    list(APPEND pylon_INCLUDE_DIRS ${_PYLON_CORE_SDK_INCLUDE_DIR})
  endif()

  # Append found pylon libraries
  if(_PYLON_CORE_SDK_BASE_LIBRARY AND _PYLON_CORE_SDK_GCBASE_LIBRARY)
    list(APPEND pylon_LIBRARIES ${_PYLON_CORE_SDK_BASE_LIBRARY})
    list(APPEND pylon_LIBRARIES ${_PYLON_CORE_SDK_GCBASE_LIBRARY})
    list(APPEND pylon_LIBRARIES ${_PYLON_CORE_SDK_GENAPI_LIBRARY})
    list(APPEND pylon_LIBRARIES ${_PYLON_CORE_SDK_UTILITY_LIBRARY})
    list(APPEND pylon_LIBRARIES ${_PYLON_CORE_SDK_GUI_LIBRARY})
  endif()

  # Check if pylon include and libraries are found
  if(pylon_INCLUDE_DIRS AND pylon_LIBRARIES)
    set(pylon_FOUND TRUE)
  endif()

elseif(UNIX)
  # Find pylon-config tool
  find_program(_PYLON_CORE_SDK_CONFIG
    NAMES pylon-config
    HINTS
      "${PYLON_DEV_DIR}"
      "/opt/pylon"
    PATH_SUFFIXES bin
  )

  # Parse include and libraries from pylon-config tool
  if(_PYLON_CORE_SDK_CONFIG)
    execute_process(COMMAND ${_PYLON_CORE_SDK_CONFIG} "--libs"
                    OUTPUT_VARIABLE _PYLON_CORE_SDK_BASELIBS_LFLAGS
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${_PYLON_CORE_SDK_CONFIG} "--libs-rpath"
                    OUTPUT_VARIABLE _PYLON_CORE_SDK_RPATH_LINK_LFLAGS_TMP
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${_PYLON_CORE_SDK_CONFIG} "--cflags-only-I"
                    OUTPUT_VARIABLE _PYLON_CORE_SDK_INCLUDE_TMP
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "-I" "" pylon_INCLUDE_DIRS ${_PYLON_CORE_SDK_INCLUDE_TMP})

    # Remove the erroneous @ on the -rpath-link parameter comming from pylon-config,
    # manually assemble linker flags for pylon core linkage.
    string(REPLACE "@" "" _PYLON_CORE_SDK_RPATH_LINK_LFLAGS ${_PYLON_CORE_SDK_RPATH_LINK_LFLAGS_TMP})
    string(APPEND _pylon_LIBRARIES "${_PYLON_CORE_SDK_BASELIBS_LFLAGS} ${_PYLON_CORE_SDK_RPATH_LINK_LFLAGS}")
    string(REPLACE " " ";" pylon_LIBRARIES "${_pylon_LIBRARIES}")
    set(pylon_FOUND TRUE)
  endif()

endif()

if (pylon_FOUND)
  add_library(pylon INTERFACE IMPORTED)
  set_target_properties(pylon PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${pylon_INCLUDE_DIRS}"
    INTERFACE_LINK_OPTIONS "${pylon_LIBRARIES}"
  )
endif()

mark_as_advanced(pylon_INCLUDE_DIRS pylon_LIBRARIES)
message(pylon_INCLUDE_DIRS  pylon_LIBRARIES)

include(FeatureSummary)
set_package_properties(pylon PROPERTIES
  URL "https://www.baslerweb.com/de/produkte/software/basler-pylon-camera-software-suite"
  DESCRIPTION "Basler Camera Software Suite - pylon C++ SDK"
)

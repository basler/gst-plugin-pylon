#ifndef GST_PYLON_INCLUDES_H
#define GST_PYLON_INCLUDES_H

#ifdef _MSC_VER  // MSVC
#pragma warning(push)
#pragma warning(disable : 4265)
#elif __GNUC__  // GCC, CLANG, MinGW
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wunused-variable"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#endif
#endif

#include <pylon/BaslerUniversalInstantCamera.h>
#include <pylon/PylonIncludes.h>

#ifdef _MSC_VER  // MSVC
#pragma warning(pop)
#elif __GNUC__  // GCC, CLANG, MinWG
#pragma GCC diagnostic pop
#endif

#endif

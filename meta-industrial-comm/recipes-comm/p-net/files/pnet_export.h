/* pnet_export.h — static export macros for p-net built as a static library */
#pragma once

#ifndef PNET_EXPORT
#  define PNET_EXPORT
#endif

#ifndef PNET_NO_EXPORT
#  define PNET_NO_EXPORT
#endif

#ifndef PNET_DEPRECATED
#  define PNET_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef PNET_DEPRECATED_EXPORT
#  define PNET_DEPRECATED_EXPORT PNET_EXPORT PNET_DEPRECATED
#endif

#ifndef PNET_DEPRECATED_NO_EXPORT
#  define PNET_DEPRECATED_NO_EXPORT PNET_NO_EXPORT PNET_DEPRECATED
#endif

#ifndef TIFF_CONFIG_H
#define TIFF_CONFIG_H

#include "tiffconf.h"

#define CCITT_SUPPORT 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1
#define CHUNKY_STRIP_READ_SUPPORT 1
#define CXX_SUPPORT 1
#define DEFER_STRILE_LOAD 1
#define HAVE_ASSERT_H 1
#define HAVE_DECL_OPTARG 0
#define HAVE_JBG_NEWLEN 1
#define HAVE_SNPRINTF 1
#define HAVE_STRING_H 1

#if defined(_WIN32) || defined(_WIN64)
#define HAVE_IO_H 1
#define HAVE_SETMODE 1
#else
#define HAVE_FCNTL_H 1
#define HAVE_FSEEKO 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#endif

#define STRIP_SIZE_DEFAULT 8192
#define TIFF_MAX_DIR_COUNT 1048576
#define WORDS_BIGENDIAN 0

#if defined(_MSC_VER) && !defined(__clang__)
#if SIZEOF_SIZE_T == 8
#define TIFF_SIZE_FORMAT "I64u"
#else
#define TIFF_SIZE_FORMAT "u"
#endif
#elif defined(__MINGW32__)
#if SIZEOF_SIZE_T == 8
#define TIFF_SIZE_FORMAT PRIu64
#else
#define TIFF_SIZE_FORMAT PRIu32
#endif
#else
#define TIFF_SIZE_FORMAT "zu"
#endif
#if SIZEOF_SIZE_T == 8
#define TIFF_SSIZE_FORMAT PRId64
#else
#define TIFF_SSIZE_FORMAT PRId32
#endif

#endif

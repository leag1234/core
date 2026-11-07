#ifndef _TIFFCONF_
#define _TIFFCONF_

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#if defined(__SIZEOF_SIZE_T__)
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#elif defined(__SIZEOF_POINTER__)
#define SIZEOF_SIZE_T __SIZEOF_POINTER__
#elif defined(_MSC_VER)
#if defined(_WIN64)
#define SIZEOF_SIZE_T 8
#elif defined(_WIN32)
#define SIZEOF_SIZE_T 4
#endif
#endif

#if !defined(SIZEOF_SIZE_T)
#error "Unable to determine the size of size_t"
#elif SIZEOF_SIZE_T == 8
#define TIFF_SSIZE_T int64_t
#elif SIZEOF_SIZE_T == 4
#define TIFF_SSIZE_T int32_t
#else
#error "Unsupported size of size_t"
#endif

#define TIFF_INT8_T int8_t
#define TIFF_UINT8_T uint8_t
#define TIFF_INT16_T int16_t
#define TIFF_UINT16_T uint16_t
#define TIFF_INT32_T int32_t
#define TIFF_UINT32_T uint32_t
#define TIFF_INT64_T int64_t
#define TIFF_UINT64_T uint64_t

#define HAVE_IEEEFP 1
#define HOST_FILLORDER FILLORDER_LSB2MSB
#define HOST_BIGENDIAN 0

#define CCITT_SUPPORT 1
#define JPEG_SUPPORT 1
/* #undef JBIG_SUPPORT: not built, as in the previous libtiff 3.9 configuration */
#define LOGLUV_SUPPORT 1
#define LZW_SUPPORT 1
#define NEXT_SUPPORT 1
#define OJPEG_SUPPORT 1
#define PACKBITS_SUPPORT 1
/* #undef PIXARLOG_SUPPORT: not built, as in the previous libtiff 3.9 configuration */
#define THUNDER_SUPPORT 1
#define ZIP_SUPPORT 1
#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP
#define SUBIFD_SUPPORT 1
#define DEFAULT_EXTRASAMPLE_AS_ALPHA 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1
#define MDI_SUPPORT 1

#define COLORIMETRY_SUPPORT
#define YCBCR_SUPPORT
#define CMYK_SUPPORT
#define ICC_SUPPORT
#define PHOTOSHOP_SUPPORT
#define IPTC_SUPPORT

#endif

# Finds the GMP library, which libqore uses directly (exact integer arithmetic of number values, qore#6278) and
# which MPFR is built on
#
#  GMP_FOUND - the system has GMP
#  GMP_INCLUDE_DIRS - the GMP include directories (with gmp.h)
#  GMP_LIBRARIES - the libraries to link to use GMP
#
# A library that calls GMP must link it itself: it is only an indirect dependency of libmpfr, which the linker does
# not resolve the library's own references with on every platform (macOS).
#
# Copyright 2026 Qore Technologies, s.r.o.

if (GMP_INCLUDE_DIR AND GMP_LIBRARY)
    # already in the cache
    set(GMP_FIND_QUIETLY TRUE)
endif()

find_path(GMP_INCLUDE_DIR NAMES gmp.h PATH_SUFFIXES gmp)
find_library(GMP_LIBRARY NAMES gmp libgmp)

set(GMP_INCLUDE_DIRS ${GMP_INCLUDE_DIR})
set(GMP_LIBRARIES ${GMP_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG GMP_LIBRARY GMP_INCLUDE_DIR)

mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)

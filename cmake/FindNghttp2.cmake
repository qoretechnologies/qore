# Try to find the nghttp2 library for HTTP/2 support
#  NGHTTP2_FOUND - system has nghttp2 lib
#  NGHTTP2_INCLUDE_DIRS - the nghttp2 include directories
#  NGHTTP2_LIBRARIES - Libraries needed to use nghttp2
#  NGHTTP2_VERSION - Version of nghttp2

# Copyright (C) 2025 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

if (NGHTTP2_INCLUDE_DIRS AND NGHTTP2_LIBRARIES)
    # Already in cache, be silent
    set(NGHTTP2_FIND_QUIETLY TRUE)
endif ()

find_path(NGHTTP2_INCLUDE_DIR NAMES nghttp2/nghttp2.h)
find_library(NGHTTP2_LIBRARY NAMES nghttp2 libnghttp2)

if (NGHTTP2_INCLUDE_DIR)
    set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
endif ()

if (NGHTTP2_LIBRARY)
    set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})
endif ()

# Try to get version from header
if (NGHTTP2_INCLUDE_DIR)
    file(STRINGS "${NGHTTP2_INCLUDE_DIR}/nghttp2/nghttp2ver.h" _nghttp2_version_str
         REGEX "^#define[ \t]+NGHTTP2_VERSION[ \t]+\"[^\"]+\"")
    if (_nghttp2_version_str)
        string(REGEX REPLACE "^#define[ \t]+NGHTTP2_VERSION[ \t]+\"([^\"]+)\".*" "\\1"
               NGHTTP2_VERSION "${_nghttp2_version_str}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Nghttp2
    REQUIRED_VARS NGHTTP2_LIBRARIES NGHTTP2_INCLUDE_DIRS
    VERSION_VAR NGHTTP2_VERSION)

mark_as_advanced(NGHTTP2_INCLUDE_DIR NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIRS NGHTTP2_LIBRARIES)

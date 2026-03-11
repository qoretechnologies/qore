# Copyright (c) 2015 Niclas Rosenvik <youremailsarecrap@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# copies of this file or files derived from it .
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# Module to find iconv.
# Sets ICONV_INCLUDE_DIR and ICONV_LIBRARY in case libiconv is used.
#

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CMakePushCheckState)
include(FindPackageHandleStandardArgs)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_QUIET true)

if (DEFINED ENV{ICONV_DIR})
    find_path(ICONV_INCLUDE_DIR iconv.h
        PATHS $ENV{ICONV_DIR}/include
        NO_DEFAULT_PATH
    )
    find_library(ICONV_LIBRARY NAMES iconv libiconv libiconv-2
        PATHS $ENV{ICONV_DIR}/lib
        NO_DEFAULT_PATH
    )
else()
    if (DEFINED ENV{ICONV_INCLUDE_DIR})
        set(ICONV_INCLUDE_DIR $ENV{ICONV_INCLUDE_DIR})
    else()
        find_path(ICONV_INCLUDE_DIR iconv.h)
    endif()
    if (DEFINED ENV{ICONV_LIBRARY})
        set(ICONV_LIBRARY $ENV{ICONV_LIBRARY})
    else()
        find_library(ICONV_LIBRARY NAMES iconv libiconv libiconv-2)
    endif()
endif()

if (ICONV_LIBRARY AND ICONV_INCLUDE_DIR)
    set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBRARY})
    if (DEFINED ENV{ICONV_LIBRARY_TYPE})
        set(ICONV_LIBRARY_TYPE $ENV{ICONV_LIBRARY_TYPE})
        set(ICONV_FOUND_IN_LIBICONV 1)
    else()
        check_library_exists(${ICONV_LIBRARY} libiconv_open "" ICONV_FOUND_IN_LIBICONV)
        if(ICONV_FOUND_IN_LIBICONV)
            set(ICONV_LIBRARY_TYPE "GNU libiconv")
        else()
            # Apple/POSIX libiconv uses iconv_open (not libiconv_open)
            check_library_exists(${ICONV_LIBRARY} iconv_open "" ICONV_FOUND_POSIX_ICONV)
            if(ICONV_FOUND_POSIX_ICONV)
                set(ICONV_LIBRARY_TYPE "POSIX libiconv")
                set(ICONV_FOUND_IN_LIBICONV 1)
            else()
                set(ICONV_LIBRARY_TYPE "libc")
                message("-- libiconv not found: inc: ${ICONV_INCLUDE_DIR} lib: ${ICONV_LIBRARY}")
            endif()
        endif()
    endif()
endif()

if(NOT ICONV_LIBRARY OR NOT ICONV_FOUND_IN_LIBICONV)
    unset(ICONV_INCLUDE_DIR CACHE)
    unset(ICONV_LIBRARY CACHE)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
    check_include_file(iconv.h ICONV_INCLUDE_IN_LIBC)
    check_function_exists(iconv ICONV_FOUND_IN_LIBC)
    if(ICONV_FOUND_IN_LIBC AND ICONV_INCLUDE_IN_LIBC)
        set(ICONV_LIBRARY_TYPE "libc")
    endif()
endif()

if (DEFINED ICONV_LIBRARY_TYPE)
    message("-- ICONV_LIBRARY_TYPE is ${ICONV_LIBRARY_TYPE}")
endif()

cmake_pop_check_state()

find_package_handle_standard_args(ICONV REQUIRED_VARS ICONV_LIBRARY_TYPE)

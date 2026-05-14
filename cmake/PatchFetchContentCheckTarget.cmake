# Idempotently comments out the unconditional add_custom_target(check ...)
# line in a FetchContent dependency's top-level CMakeLists.txt.
#
# nghttp2, nghttp3 and ngtcp2 each declare:
#   add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
# at the top level. When more than one is pulled in via FetchContent_MakeAvailable,
# CMake errors out with a duplicate-target conflict on `check`. Commenting the line
# out removes the conflict; we do not need that target in our build.
#
# Invoked from FetchContent_Declare's PATCH_COMMAND, where the working directory
# is the populated source directory (so CMakeLists.txt is relative to cwd).
file(READ "CMakeLists.txt" _content)
# Only the line-start occurrence is replaced; a previously-commented line
# (`# add_custom_target(check`) is preserved unchanged.
string(REGEX REPLACE "(^|\n)add_custom_target\\(check" "\\1# add_custom_target(check"
    _content "${_content}")
file(WRITE "CMakeLists.txt" "${_content}")

# The external module path used by the core build and run_tests.sh.
option(QORE_BUILD_WITH_EXTERNAL_MODULES
       "Allow installed Qore modules to provide optional features in build outputs"
       ON)

set(QORE_EXTERNAL_MODULE_DIRS "" CACHE STRING
    "Ordered semicolon-separated list of external Qore module directories for the build (default: none)")
set(QORE_BUILD_EXTERNAL_MODULE_DIRS)
if (QORE_BUILD_WITH_EXTERNAL_MODULES)
    foreach(dir ${QORE_EXTERNAL_MODULE_DIRS})
        get_filename_component(dir "${dir}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        list(APPEND QORE_BUILD_EXTERNAL_MODULE_DIRS "${dir}")
    endforeach()
    list(REMOVE_DUPLICATES QORE_BUILD_EXTERNAL_MODULE_DIRS)
endif()

# Write the runner's record only when the search path changes.
string(REPLACE ";" ":" QORE_BUILD_EXTERNAL_MODULE_DIRS_STR "${QORE_BUILD_EXTERNAL_MODULE_DIRS}")
set(QORE_EXTERNAL_MODULE_DIRS_FILE "${CMAKE_BINARY_DIR}/qore-external-module-dirs.txt")
file(WRITE "${QORE_EXTERNAL_MODULE_DIRS_FILE}.in" "${QORE_BUILD_EXTERNAL_MODULE_DIRS_STR}\n")
configure_file("${QORE_EXTERNAL_MODULE_DIRS_FILE}.in" "${QORE_EXTERNAL_MODULE_DIRS_FILE}" COPYONLY)

# ARGN are in-tree directories in search order. Even an empty path must suppress the installed defaults.
function(qore_build_module_path out)
    set(dirs ${ARGN} ${QORE_BUILD_EXTERNAL_MODULE_DIRS})
    if (WIN32)
        string(REPLACE ";" "\\;" path "${dirs}")
    else()
        string(REPLACE ";" ":" path "${dirs}")
    endif()
    set(${out} "${path}" PARENT_SCOPE)
endfunction()

function(qore_build_module_env out)
    qore_build_module_path(path ${ARGN})
    set(${out} "QORE_MODULE_DIR=${path}" "QORE_MODULE_DIR_ONLY=1" PARENT_SCOPE)
endfunction()

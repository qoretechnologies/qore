# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Writes the stamp that AOT .qmod rules of modules built outside the Qore source tree depend on.
#
# The stamp is rewritten (and so every AOT .qmod of the build is recompiled) whenever qcc, the Qore library or the
# installed qcc format fingerprint changes; see QORE_USER_MODULE_AOT_RULES() in QoreMacros.cmake.  It records the AOT
# runtime identity that qcc reports in the environment the AOT rules run qcc in, which is the identity every module
# compiled by this build records and that a loading Qore library must have (see "AOT Module Runtime Compatibility" in
# the Qore module documentation).  The build fails here, before any module is compiled, when qcc cannot be run in that
# environment or reports no identity, because modules compiled with such a qcc would be refused when loaded.
#
# Inputs:
#   QCC     - the qcc executable used by the AOT rules
#   ENV     - ';'-separated NAME=VALUE environment entries the AOT rules run qcc with (QORE_QM_METADATA_ENV)
#   LIBRARY - the Qore library the module is built against (optional; reported in diagnostics)
#   OUTPUT  - the stamp file to write

cmake_policy(SET CMP0007 NEW)

if (NOT DEFINED QCC OR "${QCC}" STREQUAL "")
    message(FATAL_ERROR "QCC is required")
endif ()
if (NOT DEFINED OUTPUT OR "${OUTPUT}" STREQUAL "")
    message(FATAL_ERROR "OUTPUT is required")
endif ()

string(REPLACE "|" ";" _env "${ENV}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${_env} "${QCC}" --version
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)
if (NOT _rc EQUAL 0)
    message(FATAL_ERROR "cannot run '${QCC} --version' to determine the AOT runtime identity for AOT module "
        "compilation (exit code ${_rc}): ${_out}${_err}")
endif ()
string(REGEX MATCH "AOT runtime identity: ([0-9A-Za-z]+)" _match "${_out}")
if (NOT _match)
    message(FATAL_ERROR "'${QCC} --version' does not report an AOT runtime identity, so the AOT modules it "
        "compiles would be refused by the Qore library they are built for; use the qcc of the Qore installation "
        "that provides '${LIBRARY}' (output: ${_out})")
endif ()
set(_identity "${CMAKE_MATCH_1}")

get_filename_component(_output_dir "${OUTPUT}" DIRECTORY)
if (_output_dir)
    file(MAKE_DIRECTORY "${_output_dir}")
endif ()
# always written: the stamp's mtime is what makes every AOT .qmod of the build recompile
file(WRITE "${OUTPUT}" "aot-runtime-identity=${_identity}\nqcc=${QCC}\nlibrary=${LIBRARY}\n")
message(STATUS "AOT modules are compiled for AOT runtime identity ${_identity}")

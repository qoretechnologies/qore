#
# Qore Programming Languages cmake macros
#

include(CMakeParseArguments)

if (NOT DEFINED QORE_CMAKE_DIR)
    get_filename_component(QORE_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
endif ()

# Global list to collect user module names for documentation cross-referencing
set(QORE_USER_MODULE_NAMES "" CACHE INTERNAL "List of user module names for doc cross-referencing")

if (NOT DEFINED QORE_QDX_COMMAND)
    set(QORE_QDX_COMMAND ${QORE_QDX_EXECUTABLE})
endif ()
if (NOT DEFINED QORE_QJAR_COMMAND)
    set(QORE_QJAR_COMMAND ${QORE_QJAR_EXECUTABLE})
endif ()
if (NOT DEFINED QORE_BINARY_MODULE_INSTALL_COMPONENT)
    set(QORE_BINARY_MODULE_INSTALL_COMPONENT Unspecified)
endif()
if (NOT DEFINED QORE_QM_SOURCE_INSTALL_COMPONENT)
    set(QORE_QM_SOURCE_INSTALL_COMPONENT Unspecified)
endif()
if (NOT DEFINED QORE_QMOD_INSTALL_COMPONENT)
    set(QORE_QMOD_INSTALL_COMPONENT Unspecified)
endif()
if (NOT DEFINED QORE_DOC_DEFINES)
    set(_QORE_DOC_DEFINES_LIST QORE_QDX_RUN)
    if (WIN32 OR MSYS OR MINGW)
        list(APPEND _QORE_DOC_DEFINES_LIST Windows)
    else (WIN32 OR MSYS OR MINGW)
        list(APPEND _QORE_DOC_DEFINES_LIST Unix)
    endif (WIN32 OR MSYS OR MINGW)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _QORE_DOC_BUILD_TYPE_LWR)
    if (_QORE_DOC_BUILD_TYPE_LWR MATCHES "debug")
        list(APPEND _QORE_DOC_DEFINES_LIST QoreDebug)
    endif ()
    if (DEFINED HAVE_TERMIOS_H AND HAVE_TERMIOS_H)
        list(APPEND _QORE_DOC_DEFINES_LIST HAVE_TERMIOS)
    endif ()
    string(REPLACE ";" "," QORE_DOC_DEFINES "${_QORE_DOC_DEFINES_LIST}")
    if (DEFINED ENV{QORE_DOC_DEFINES})
        set(_QORE_DOC_DEFINES_ENV "$ENV{QORE_DOC_DEFINES}")
        string(REGEX REPLACE "[ \t\r\n]+" "," _QORE_DOC_DEFINES_ENV "${_QORE_DOC_DEFINES_ENV}")
        if (QORE_DOC_DEFINES AND _QORE_DOC_DEFINES_ENV)
            set(QORE_DOC_DEFINES "${QORE_DOC_DEFINES},${_QORE_DOC_DEFINES_ENV}")
        elseif (_QORE_DOC_DEFINES_ENV)
            set(QORE_DOC_DEFINES "${_QORE_DOC_DEFINES_ENV}")
        endif ()
    endif ()
endif ()
if (NOT DEFINED QORE_DOCS_ENV)
    if (DEFINED QORE_MODULE_DIR_FOR_DOCS)
        set(QORE_DOCS_ENV QORE_MODULE_DIR=${QORE_MODULE_DIR_FOR_DOCS} QORE_DOC_DEFINES=${QORE_DOC_DEFINES})
    elseif (DEFINED ENV{QORE_MODULE_DIR})
        set(QORE_DOCS_ENV QORE_MODULE_DIR=$ENV{QORE_MODULE_DIR} QORE_DOC_DEFINES=${QORE_DOC_DEFINES})
    else ()
        set(QORE_DOCS_ENV QORE_DOC_DEFINES=${QORE_DOC_DEFINES})
    endif ()
endif ()

function(QORE_WRITE_IF_CHANGED _path _content)
    get_filename_component(_qore_write_if_changed_dir "${_path}" DIRECTORY)
    if (_qore_write_if_changed_dir)
        file(MAKE_DIRECTORY "${_qore_write_if_changed_dir}")
    endif ()

    set(_current)
    if (EXISTS "${_path}")
        file(READ "${_path}" _current)
    endif ()
    if (NOT "${_current}" STREQUAL "${_content}")
        file(WRITE "${_path}" "${_content}")
    endif ()
endfunction()

function(QORE_CONTENT_DIGEST_TARGET _target)
    set(options ALL)
    set(oneValueArgs OUTPUT STAMP INPUT_LIST)
    set(multiValueArgs INPUTS DEPENDS)
    cmake_parse_arguments(_QORE_CDT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT _QORE_CDT_OUTPUT)
        message(FATAL_ERROR "QORE_CONTENT_DIGEST_TARGET(${_target}) requires OUTPUT")
    endif ()
    if (_QORE_CDT_INPUT_LIST AND _QORE_CDT_INPUTS)
        message(FATAL_ERROR "QORE_CONTENT_DIGEST_TARGET(${_target}) accepts INPUT_LIST or INPUTS, not both")
    endif ()
    if (NOT _QORE_CDT_INPUT_LIST AND NOT _QORE_CDT_INPUTS)
        message(FATAL_ERROR "QORE_CONTENT_DIGEST_TARGET(${_target}) requires INPUT_LIST or INPUTS")
    endif ()

    if (_QORE_CDT_STAMP)
        set(_qore_cdt_stamp "${_QORE_CDT_STAMP}")
    else ()
        set(_qore_cdt_stamp "${_QORE_CDT_OUTPUT}.stamp")
    endif ()

    if (_QORE_CDT_INPUT_LIST)
        set(_qore_cdt_input_list "${_QORE_CDT_INPUT_LIST}")
        set(_qore_cdt_input_deps "${_qore_cdt_input_list}")
    else ()
        set(_qore_cdt_input_list "${CMAKE_CURRENT_BINARY_DIR}/${_target}-inputs.txt")
        set(_qore_cdt_input_content)
        foreach(_qore_cdt_input ${_QORE_CDT_INPUTS})
            string(APPEND _qore_cdt_input_content "${_qore_cdt_input}\n")
        endforeach()
        QORE_WRITE_IF_CHANGED("${_qore_cdt_input_list}" "${_qore_cdt_input_content}")
        set(_qore_cdt_input_deps "${_qore_cdt_input_list}" ${_QORE_CDT_INPUTS})
    endif ()

    set(_qore_cdt_script "${QORE_CMAKE_DIR}/QoreWriteContentDigest.cmake")
    add_custom_command(
        OUTPUT ${_qore_cdt_stamp}
        BYPRODUCTS ${_QORE_CDT_OUTPUT}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_LIST=${_qore_cdt_input_list}
            -DOUTPUT=${_QORE_CDT_OUTPUT}
            -DSUCCESS_STAMP=${_qore_cdt_stamp}
            -P ${_qore_cdt_script}
        DEPENDS
            ${_qore_cdt_script}
            ${_qore_cdt_input_deps}
            ${_QORE_CDT_DEPENDS}
        COMMENT "Checking ${_target} input content"
        VERBATIM
    )

    if (_QORE_CDT_ALL)
        add_custom_target(${_target} ALL DEPENDS ${_qore_cdt_stamp})
    else ()
        add_custom_target(${_target} DEPENDS ${_qore_cdt_stamp})
    endif ()
endfunction()

# Returns the in-tree binary-module directories as a colon-separated QORE_MODULE_DIR fragment.
#
# Binary modules are emitted to <build>/modules/<name>/<name>-api-<ver>.qmod, and none of those
# directories is on the module path by default.  Without them, a user module that %requires an
# in-tree binary module (ex: a module using json's jws_sign()) resolves the *installed* copy of
# that module at AOT parse time, so anything added to the module in this tree is invisible.  The
# artifact still compiles, but every slot referencing the new symbol fails to register at runtime.
#
# The list is derived from the source tree because the binary directories do not exist yet on a
# fresh configure; entries that never materialize are simply ignored when the path is searched.
function(QORE_GET_BINARY_MODULE_PATH _out_var)
    set(_qore_bm_path "")
    file(GLOB _qore_bm_dirs LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/modules/*")
    foreach (_qore_bm_dir ${_qore_bm_dirs})
        if (IS_DIRECTORY "${_qore_bm_dir}")
            get_filename_component(_qore_bm_name "${_qore_bm_dir}" NAME)
            if (_qore_bm_path STREQUAL "")
                set(_qore_bm_path "${CMAKE_BINARY_DIR}/modules/${_qore_bm_name}")
            else ()
                set(_qore_bm_path "${_qore_bm_path}:${CMAKE_BINARY_DIR}/modules/${_qore_bm_name}")
            endif ()
        endif ()
    endforeach ()
    set(${_out_var} "${_qore_bm_path}" PARENT_SCOPE)
endfunction()

function(QORE_GET_QCC_COMMAND _out_var)
    if (TARGET qcc)
        set(_qore_qcc_command $<TARGET_FILE:qcc>)
    elseif (DEFINED QORE_QCC_EXECUTABLE AND NOT "${QORE_QCC_EXECUTABLE}" STREQUAL "")
        set(_qore_qcc_command ${QORE_QCC_EXECUTABLE})
    else ()
        message(FATAL_ERROR "qcc is required but neither target qcc nor QORE_QCC_EXECUTABLE is available")
    endif ()
    set(${_out_var} ${_qore_qcc_command} PARENT_SCOPE)
endfunction()

function(QORE_GET_QCC_DEPS _out_var)
    set(options TRACK_BINARY)
    set(oneValueArgs QCC_COMMAND)
    cmake_parse_arguments(_QORE_GQD "${options}" "${oneValueArgs}" "" ${ARGN})

    if (_QORE_GQD_QCC_COMMAND)
        set(_qore_qcc_command "${_QORE_GQD_QCC_COMMAND}")
    else ()
        QORE_GET_QCC_COMMAND(_qore_qcc_command)
    endif ()

    set(_deps)
    if (DEFINED QORE_QCC_FORMAT_STAMP AND NOT "${QORE_QCC_FORMAT_STAMP}" STREQUAL "")
        list(APPEND _deps ${QORE_QCC_FORMAT_STAMP})
    elseif (DEFINED QCC_FORMAT_STAMP AND NOT "${QCC_FORMAT_STAMP}" STREQUAL "")
        list(APPEND _deps ${QCC_FORMAT_STAMP})
    elseif (IS_ABSOLUTE "${_qore_qcc_command}" AND EXISTS "${_qore_qcc_command}")
        get_filename_component(_qore_qcc_dir "${_qore_qcc_command}" DIRECTORY)
        set(_qore_qcc_candidate_stamp "${_qore_qcc_dir}/qcc-format.stamp")
        if (EXISTS "${_qore_qcc_candidate_stamp}")
            list(APPEND _deps ${_qore_qcc_candidate_stamp})
        endif ()
        # The format stamp only advances when one of the curated qcc format
        # sources changes, but AOT output is generated by libqore, whose
        # codegen spans far more of the library than that list names.  An
        # installed qore that changed codegen anywhere else therefore leaves
        # every consumer .qo silently stale, which surfaces much later as a
        # link or deferred-resolution error naming a file the developer never
        # touched.  A reinstall is rare and a full .qo rebuild is cheap, so
        # against an installed qore the library itself is the dependency.
        if (DEFINED QORE_LIBRARY AND IS_ABSOLUTE "${QORE_LIBRARY}"
                AND EXISTS "${QORE_LIBRARY}")
            list(APPEND _deps ${QORE_LIBRARY})
        endif ()
    endif ()

    if (_QORE_GQD_TRACK_BINARY OR NOT _deps)
        if (IS_ABSOLUTE "${_qore_qcc_command}" AND EXISTS "${_qore_qcc_command}")
            list(APPEND _deps ${_qore_qcc_command})
        endif ()
    endif ()
    set(${_out_var} ${_deps} PARENT_SCOPE)
endfunction()

function(QORE_QCC_SIDECAR_PATHS _output)
    set(options)
    set(oneValueArgs DEPFILE INDEX_JSON STATUS_JSON SUCCESS_STAMP CONTENT_STAMP
        COMPILE_CONTRACT_STAMP MANIFEST_JSON)
    cmake_parse_arguments(_QORE_QSP "${options}" "${oneValueArgs}" "" ${ARGN})

    if (_QORE_QSP_DEPFILE)
        set(${_QORE_QSP_DEPFILE} "${_output}.d" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_INDEX_JSON)
        set(${_QORE_QSP_INDEX_JSON} "${_output}.idx.json" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_STATUS_JSON)
        set(${_QORE_QSP_STATUS_JSON} "${_output}.status.json" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_SUCCESS_STAMP)
        set(${_QORE_QSP_SUCCESS_STAMP} "${_output}.stamp" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_CONTENT_STAMP)
        set(${_QORE_QSP_CONTENT_STAMP} "${_output}.content.stamp" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_COMPILE_CONTRACT_STAMP)
        set(${_QORE_QSP_COMPILE_CONTRACT_STAMP}
            "${_output}.compile-contract.stamp" PARENT_SCOPE)
    endif ()
    if (_QORE_QSP_MANIFEST_JSON)
        set(${_QORE_QSP_MANIFEST_JSON} "${_output}.source.manifest.json" PARENT_SCOPE)
    endif ()
endfunction()

function(QORE_QCC_SIDECAR_FLAGS _out_var _output)
    set(options ALL DEPFILE INDEX_JSON STATUS_JSON SUCCESS_STAMP CONTENT_STAMP
        COMPILE_CONTRACT_STAMP MANIFEST_JSON SKIP_IF_MANIFEST_CURRENT
        QO_INPUT_CONTENT_STAMPS COMPILE_CONTRACT_DEPENDENCIES
        SCRIPT_AGGREGATE_NATIVE_REGISTERS)
    set(oneValueArgs DEPFILE_TARGET QOLINK_MAP AGGREGATE_SYMBOL SCRIPT_AGGREGATE)
    cmake_parse_arguments(_QORE_QSF "${options}" "${oneValueArgs}" "" ${ARGN})

    if (_QORE_QSF_ALL)
        set(_QORE_QSF_DEPFILE TRUE)
        set(_QORE_QSF_INDEX_JSON TRUE)
        set(_QORE_QSF_STATUS_JSON TRUE)
        set(_QORE_QSF_SUCCESS_STAMP TRUE)
        set(_QORE_QSF_CONTENT_STAMP TRUE)
        set(_QORE_QSF_COMPILE_CONTRACT_STAMP TRUE)
        set(_QORE_QSF_COMPILE_CONTRACT_DEPENDENCIES TRUE)
        set(_QORE_QSF_MANIFEST_JSON TRUE)
        set(_QORE_QSF_SKIP_IF_MANIFEST_CURRENT TRUE)
    endif ()

    set(_flags)
    if (_QORE_QSF_DEPFILE)
        list(APPEND _flags "--depfile=${_output}.d")
    endif ()
    if (_QORE_QSF_DEPFILE_TARGET)
        list(APPEND _flags "--depfile-target=${_QORE_QSF_DEPFILE_TARGET}")
    endif ()
    if (_QORE_QSF_INDEX_JSON)
        list(APPEND _flags "--write-index-json=${_output}.idx.json")
    endif ()
    if (_QORE_QSF_STATUS_JSON)
        list(APPEND _flags "--write-status-json=${_output}.status.json")
    endif ()
    if (_QORE_QSF_SUCCESS_STAMP)
        list(APPEND _flags "--success-stamp=${_output}.stamp")
    endif ()
    if (_QORE_QSF_CONTENT_STAMP)
        list(APPEND _flags "--content-stamp=${_output}.content.stamp")
    endif ()
    if (_QORE_QSF_COMPILE_CONTRACT_STAMP)
        list(APPEND _flags
            "--compile-contract-stamp=${_output}.compile-contract.stamp")
    endif ()
    if (_QORE_QSF_COMPILE_CONTRACT_DEPENDENCIES)
        list(APPEND _flags "--depfile-compile-contract-stamps")
    endif ()
    if (_QORE_QSF_MANIFEST_JSON)
        list(APPEND _flags "--write-manifest=${_output}.source.manifest.json")
    endif ()
    if (_QORE_QSF_SKIP_IF_MANIFEST_CURRENT)
        list(APPEND _flags "--skip-if-manifest-current")
    endif ()
    if (_QORE_QSF_QO_INPUT_CONTENT_STAMPS)
        list(APPEND _flags "--depfile-qo-input-content-stamps")
    endif ()
    if (_QORE_QSF_QOLINK_MAP)
        list(APPEND _flags "--qolink-map=${_QORE_QSF_QOLINK_MAP}")
    endif ()
    if (_QORE_QSF_AGGREGATE_SYMBOL)
        list(APPEND _flags "--aggregate-symbol=${_QORE_QSF_AGGREGATE_SYMBOL}")
    endif ()
    if (_QORE_QSF_SCRIPT_AGGREGATE)
        list(APPEND _flags "--script-aggregate=${_QORE_QSF_SCRIPT_AGGREGATE}")
    endif ()
    if (_QORE_QSF_SCRIPT_AGGREGATE_NATIVE_REGISTERS)
        list(APPEND _flags "--script-aggregate-native-registers")
    endif ()

    set(${_out_var} ${_flags} PARENT_SCOPE)
endfunction()

function(QORE_QCC_SOURCE_ID _out_var _path)
    get_filename_component(_qore_qcc_source_id_abs "${_path}" ABSOLUTE)
    get_filename_component(_qore_qcc_source_id_real "${_qore_qcc_source_id_abs}" REALPATH)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _qore_qcc_source_id "${_qore_qcc_source_id_real}")
    set(${_out_var} "${_qore_qcc_source_id}" PARENT_SCOPE)
endfunction()

function(QORE_QCC_HELPER_PATH _out_var _name)
    if ("${_name}" STREQUAL "qore-qo-incremental"
            AND DEFINED QORE_QCC_INCREMENTAL_HELPER
            AND NOT "${QORE_QCC_INCREMENTAL_HELPER}" STREQUAL "")
        set(_qore_qcc_helper "${QORE_QCC_INCREMENTAL_HELPER}")
    elseif ("${_name}" STREQUAL "qore-qo-source-order"
            AND DEFINED QORE_QCC_SOURCE_ORDER_HELPER
            AND NOT "${QORE_QCC_SOURCE_ORDER_HELPER}" STREQUAL "")
        set(_qore_qcc_helper "${QORE_QCC_SOURCE_ORDER_HELPER}")
    elseif ("${_name}" STREQUAL "qore-qo-batch-bootstrap"
            AND DEFINED QORE_QCC_BATCH_BOOTSTRAP_HELPER
            AND NOT "${QORE_QCC_BATCH_BOOTSTRAP_HELPER}" STREQUAL "")
        set(_qore_qcc_helper "${QORE_QCC_BATCH_BOOTSTRAP_HELPER}")
    elseif (DEFINED QORE_CMAKE_DIR AND EXISTS "${QORE_CMAKE_DIR}/${_name}")
        set(_qore_qcc_helper "${QORE_CMAKE_DIR}/${_name}")
    elseif (EXISTS "${CMAKE_CURRENT_LIST_DIR}/../tools/${_name}")
        get_filename_component(_qore_qcc_helper "${CMAKE_CURRENT_LIST_DIR}/../tools/${_name}" REALPATH)
    elseif (EXISTS "${CMAKE_CURRENT_LIST_DIR}/${_name}")
        set(_qore_qcc_helper "${CMAKE_CURRENT_LIST_DIR}/${_name}")
    else ()
        message(FATAL_ERROR "cannot find Qore qcc helper ${_name}")
    endif ()
    set(${_out_var} "${_qore_qcc_helper}" PARENT_SCOPE)
endfunction()

function(QORE_QCC_APPEND_CONTEXT _out_var _key)
    set(_qore_qcc_context "${${_out_var}}")
    foreach(_qore_qcc_context_value ${ARGN})
        string(APPEND _qore_qcc_context "${_key}=${_qore_qcc_context_value}\n")
    endforeach()
    set(${_out_var} "${_qore_qcc_context}" PARENT_SCOPE)
endfunction()

function(QORE_QCC_LOAD_MODULE_TARGET_DEPS _out_var)
    set(_qore_qcc_load_module_target_deps)
    foreach(_qore_qcc_module ${ARGN})
        if (_qore_qcc_module MATCHES "^[A-Za-z0-9_.+-]+$")
            # The generator expression is resolved after all CMakeLists have
            # been read, so it also finds qmod targets declared after the qcc
            # consumer.  External modules simply contribute no dependency.
            list(APPEND _qore_qcc_load_module_target_deps
                "$<TARGET_NAME_IF_EXISTS:${_qore_qcc_module}-qmod>")
        endif ()
    endforeach()
    set(${_out_var} ${_qore_qcc_load_module_target_deps} PARENT_SCOPE)
endfunction()

# Compile a source group through one shared parse. BOOTSTRAP_AGGREGATE_OUTPUT
# and BOOTSTRAP_AGGREGATE_SYMBOL optionally emit a script aggregate during the
# initial batch build; CONTEXT and MANIFEST_INPUTS let its later standalone
# rule adopt the same manifest without reparsing.
function(QORE_QCC_COMPILE_OBJECTS _out_var)
    set(options WARNINGS_ARE_ERRORS BOOTSTRAP_AGGREGATE_NATIVE_REGISTERS)
    set(oneValueArgs GROUP OUTPUT_DIR SCRIPT_DIR INCLUDE_DIR MODULE_DIR METADATA_COMPRESSION
        STAMPS_VAR CONTENT_STAMPS_VAR COMPILE_CONTRACT_STAMPS_VAR
        ORDER_TARGETS_VAR CONTEXT_VAR BOOTSTRAP_AGGREGATE_OUTPUT
        BOOTSTRAP_AGGREGATE_SYMBOL BOOTSTRAP_AGGREGATE_CONTEXT)
    set(multiValueArgs SOURCES STUBS LOAD_MODULES PARSE_DEFINES PARSE_OPTIONS
        MANIFEST_INPUTS DEPENDS BOOTSTRAP_AGGREGATE_MANIFEST_INPUTS)
    cmake_parse_arguments(_QORE_QCO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT _QORE_QCO_GROUP)
        message(FATAL_ERROR "QORE_QCC_COMPILE_OBJECTS(${_out_var}) requires GROUP")
    endif ()
    if ((NOT _QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT
            AND _QORE_QCO_BOOTSTRAP_AGGREGATE_SYMBOL)
            OR (_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT
            AND NOT _QORE_QCO_BOOTSTRAP_AGGREGATE_SYMBOL))
        message(FATAL_ERROR
            "QORE_QCC_COMPILE_OBJECTS(${_out_var}) requires both "
            "BOOTSTRAP_AGGREGATE_OUTPUT and BOOTSTRAP_AGGREGATE_SYMBOL")
    endif ()
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _qore_qcc_group_id "${_QORE_QCO_GROUP}")
    if (_qore_qcc_group_id MATCHES "^[0-9]")
        set(_qore_qcc_group_id "qcc_${_qore_qcc_group_id}")
    endif ()
    if ("${_qore_qcc_group_id}" STREQUAL "")
        set(_qore_qcc_group_id "qcc_group")
    endif ()
    if (NOT _QORE_QCO_OUTPUT_DIR)
        set(_QORE_QCO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/qo/${_qore_qcc_group_id}")
    endif ()
    if (NOT _QORE_QCO_SCRIPT_DIR)
        set(_QORE_QCO_SCRIPT_DIR "${CMAKE_CURRENT_BINARY_DIR}/qo-scripts/${_qore_qcc_group_id}")
    endif ()
    if (NOT _QORE_QCO_INCLUDE_DIR)
        set(_QORE_QCO_INCLUDE_DIR "$ENV{QORE_INCLUDE_DIR}")
    endif ()
    if (NOT _QORE_QCO_MODULE_DIR)
        set(_QORE_QCO_MODULE_DIR "$ENV{QORE_MODULE_DIR}")
    endif ()

    file(MAKE_DIRECTORY "${_QORE_QCO_OUTPUT_DIR}")
    file(MAKE_DIRECTORY "${_QORE_QCO_SCRIPT_DIR}")

    QORE_GET_QCC_COMMAND(_qore_qcc_command)
    QORE_GET_QCC_DEPS(_qore_qcc_deps QCC_COMMAND "${_qore_qcc_command}")
    QORE_QCC_HELPER_PATH(_qore_qcc_incremental_helper qore-qo-incremental)
    QORE_QCC_HELPER_PATH(_qore_qcc_source_order_helper qore-qo-source-order)
    QORE_QCC_HELPER_PATH(_qore_qcc_batch_bootstrap_helper qore-qo-batch-bootstrap)

    set(_qore_qcc_outputs)
    set(_qore_qcc_stamps)
    set(_qore_qcc_content_stamps)
    set(_qore_qcc_compile_contract_stamps)
    set(_qore_qcc_order_targets)
    set(_qore_qcc_abs_sources)
    set(_qore_qcc_index 0)
    foreach(_qore_qcc_source ${_QORE_QCO_SOURCES})
        get_filename_component(_qore_qcc_abs_source "${_qore_qcc_source}" ABSOLUTE)
        get_filename_component(_qore_qcc_real_source "${_qore_qcc_abs_source}" REALPATH)
        QORE_QCC_SOURCE_ID(_qore_qcc_source_id "${_qore_qcc_real_source}")
        set(_qore_qcc_output "${_QORE_QCO_OUTPUT_DIR}/${_qore_qcc_source_id}.qo")
        set(_qore_qcc_order_target "qore_qcc_${_qore_qcc_group_id}_${_qore_qcc_index}")
        list(APPEND _qore_qcc_abs_sources "${_qore_qcc_real_source}")
        list(APPEND _qore_qcc_outputs "${_qore_qcc_output}")
        list(APPEND _qore_qcc_stamps "${_qore_qcc_output}.stamp")
        list(APPEND _qore_qcc_content_stamps "${_qore_qcc_output}.content.stamp")
        list(APPEND _qore_qcc_compile_contract_stamps
            "${_qore_qcc_output}.compile-contract.stamp")
        list(APPEND _qore_qcc_order_targets "${_qore_qcc_order_target}")
        math(EXPR _qore_qcc_index "${_qore_qcc_index} + 1")
    endforeach()

    set(_qore_qcc_context_path "${_QORE_QCO_SCRIPT_DIR}/.qcc-context")
    set(_qore_qcc_context "format=1\nkind=qcc-object-group\ngroup=${_QORE_QCO_GROUP}\nqcc=${_qore_qcc_command}\n")
    string(APPEND _qore_qcc_context "qore_include_dir=${_QORE_QCO_INCLUDE_DIR}\n")
    string(APPEND _qore_qcc_context "qore_module_dir=${_QORE_QCO_MODULE_DIR}\n")
    string(APPEND _qore_qcc_context "metadata_compression=${_QORE_QCO_METADATA_COMPRESSION}\n")
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context qcc_dep ${_qore_qcc_deps})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context stub ${_QORE_QCO_STUBS})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context load_module ${_QORE_QCO_LOAD_MODULES})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context parse_define ${_QORE_QCO_PARSE_DEFINES})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context parse_option ${_QORE_QCO_PARSE_OPTIONS})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context input ${_qore_qcc_abs_sources})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context output ${_qore_qcc_outputs})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context manifest_input ${_QORE_QCO_MANIFEST_INPUTS})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context bootstrap_aggregate_output
        ${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context bootstrap_aggregate_symbol
        ${_QORE_QCO_BOOTSTRAP_AGGREGATE_SYMBOL})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context bootstrap_aggregate_context
        ${_QORE_QCO_BOOTSTRAP_AGGREGATE_CONTEXT})
    QORE_QCC_APPEND_CONTEXT(_qore_qcc_context bootstrap_aggregate_manifest_input
        ${_QORE_QCO_BOOTSTRAP_AGGREGATE_MANIFEST_INPUTS})
    QORE_WRITE_IF_CHANGED("${_qore_qcc_context_path}" "${_qore_qcc_context}")

    set(_qore_qcc_source_symbols "${_QORE_QCO_SCRIPT_DIR}/source-symbols.manifest")

    set(_qore_qcc_manifest_input_flags)
    foreach(_qore_qcc_manifest_input
            ${_qore_qcc_context_path}
            ${_qore_qcc_source_symbols}
            ${_qore_qcc_incremental_helper}
            ${_qore_qcc_source_order_helper}
            ${_qore_qcc_deps}
            ${_QORE_QCO_MANIFEST_INPUTS})
        list(APPEND _qore_qcc_manifest_input_flags "--manifest-input=${_qore_qcc_manifest_input}")
    endforeach()

    set(_qore_qcc_direct_deps_prefix "QORE_QCC_DIRECT_DEPS_${_qore_qcc_group_id}")
    set(_qore_qcc_direct_deps_cmake "${_QORE_QCO_SCRIPT_DIR}/direct-deps.cmake")
    execute_process(
        COMMAND ${_qore_qcc_source_order_helper}
            --direct-map-cmake
            ${_qore_qcc_direct_deps_prefix}
            ${_qore_qcc_context_path}
        OUTPUT_VARIABLE _qore_qcc_direct_deps_cmake_content
        ERROR_VARIABLE _qore_qcc_direct_deps_error
        RESULT_VARIABLE _qore_qcc_direct_deps_result
    )
    if (NOT _qore_qcc_direct_deps_result EQUAL 0)
        message(FATAL_ERROR
            "qore-qo-source-order failed for ${_QORE_QCO_GROUP}: ${_qore_qcc_direct_deps_error}")
    endif ()
    QORE_WRITE_IF_CHANGED("${_qore_qcc_direct_deps_cmake}" "${_qore_qcc_direct_deps_cmake_content}")
    include("${_qore_qcc_direct_deps_cmake}")

    execute_process(
        COMMAND ${_qore_qcc_source_order_helper}
            --source-symbol-manifest
            ${_qore_qcc_source_symbols}
            ${_qore_qcc_context_path}
        ERROR_VARIABLE _qore_qcc_source_symbols_error
        RESULT_VARIABLE _qore_qcc_source_symbols_result
    )
    if (NOT _qore_qcc_source_symbols_result EQUAL 0)
        message(FATAL_ERROR
            "qore-qo-source-order failed for ${_QORE_QCO_GROUP}: ${_qore_qcc_source_symbols_error}")
    endif ()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${_qore_qcc_source_order_helper}
        ${_qore_qcc_abs_sources}
        ${_QORE_QCO_MANIFEST_INPUTS})

    set(_qore_qcc_stub_flags)
    foreach(_qore_qcc_stub ${_QORE_QCO_STUBS})
        list(APPEND _qore_qcc_stub_flags "--stub=${_qore_qcc_stub}")
    endforeach()
    set(_qore_qcc_load_flags)
    foreach(_qore_qcc_module ${_QORE_QCO_LOAD_MODULES})
        list(APPEND _qore_qcc_load_flags -l ${_qore_qcc_module})
    endforeach()
    QORE_QCC_LOAD_MODULE_TARGET_DEPS(_qore_qcc_load_module_target_deps
        ${_QORE_QCO_LOAD_MODULES})
    set(_qore_qcc_define_flags)
    foreach(_qore_qcc_define ${_QORE_QCO_PARSE_DEFINES})
        list(APPEND _qore_qcc_define_flags "--define=${_qore_qcc_define}")
    endforeach()
    set(_qore_qcc_parse_option_flags)
    foreach(_qore_qcc_parse_option ${_QORE_QCO_PARSE_OPTIONS})
        list(APPEND _qore_qcc_parse_option_flags "--parse-option=${_qore_qcc_parse_option}")
    endforeach()
    set(_qore_qcc_source_symbol_flags "--source-symbol-manifest=${_qore_qcc_source_symbols}")
    set(_qore_qcc_metadata_compression_flags)
    if (_QORE_QCO_METADATA_COMPRESSION)
        list(APPEND _qore_qcc_metadata_compression_flags
            "--aot-metadata-compression=${_QORE_QCO_METADATA_COMPRESSION}")
    endif ()
    set(_qore_qcc_warning_flags)
    if (_QORE_QCO_WARNINGS_ARE_ERRORS)
        list(APPEND _qore_qcc_warning_flags --warnings-are-errors)
    endif ()
    set(_qore_qcc_bootstrap_aggregate_flags)
    set(_qore_qcc_bootstrap_aggregate_dir "${_QORE_QCO_OUTPUT_DIR}")
    if (_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT)
        list(APPEND _qore_qcc_bootstrap_aggregate_flags
            "--batch-script-aggregate=${_QORE_QCO_BOOTSTRAP_AGGREGATE_SYMBOL}"
            "--batch-script-aggregate-output=${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}"
            "--write-status-json=${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}.status.json"
            "--success-stamp=${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}.stamp"
            "--content-stamp=${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}.content.stamp"
            "--write-manifest=${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}.script-aggregate.manifest.json")
        if (_QORE_QCO_BOOTSTRAP_AGGREGATE_NATIVE_REGISTERS)
            list(APPEND _qore_qcc_bootstrap_aggregate_flags
                --script-aggregate-native-registers)
        endif ()
        foreach(_qore_qcc_aggregate_manifest_input
                ${_QORE_QCO_BOOTSTRAP_AGGREGATE_CONTEXT}
                ${_qore_qcc_deps}
                ${_QORE_QCO_BOOTSTRAP_AGGREGATE_MANIFEST_INPUTS})
            list(APPEND _qore_qcc_bootstrap_aggregate_flags
                "--batch-script-aggregate-manifest-input=${_qore_qcc_aggregate_manifest_input}")
        endforeach()
        get_filename_component(_qore_qcc_bootstrap_aggregate_dir
            "${_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT}" DIRECTORY)
        if (NOT _qore_qcc_bootstrap_aggregate_dir)
            set(_qore_qcc_bootstrap_aggregate_dir
                "${CMAKE_CURRENT_BINARY_DIR}")
        endif ()
    endif ()

    set(_qore_qcc_single_script "${_QORE_QCO_SCRIPT_DIR}/qcc-single.sh")
    set(_qore_qcc_single_cmd "#!/bin/sh\nset -e\nsrc=$1\nout=$2\npreload_dir=$3\nif [ -z \"$preload_dir\" ]; then\n    preload_dir='${_QORE_QCO_OUTPUT_DIR}'\nfi\nextra_parse_define_file=\${QORE_QCC_SOURCE_PARSE_DEFINE_FILE:-\${QORUS_QO_SOURCE_PARSE_DEFINE_FILE:-}}\nmanifest_input_file=\${QORE_QCC_SOURCE_MANIFEST_INPUT_FILE:-\${QORUS_QO_SOURCE_MANIFEST_INPUT_FILE:-}}\nset --\nif [ -n \"$extra_parse_define_file\" ] && [ -f \"$extra_parse_define_file\" ]; then\n    set -- \"$@\" \"--manifest-input=$extra_parse_define_file\"\n    while IFS= read -r def; do\n        [ -n \"$def\" ] || continue\n        set -- \"$@\" \"--define=$def\"\n    done < \"$extra_parse_define_file\"\nfi\nif [ -n \"$manifest_input_file\" ] && [ -f \"$manifest_input_file\" ]; then\n    while IFS= read -r dep; do\n        [ -n \"$dep\" ] || continue\n        set -- \"$@\" \"--manifest-input=$dep\"\n    done < \"$manifest_input_file\"\nfi\nQORE_INCLUDE_DIR='${_QORE_QCO_INCLUDE_DIR}' QORE_MODULE_DIR='${_QORE_QCO_MODULE_DIR}' '${_qore_qcc_command}'")
    foreach(_qore_qcc_arg
            ${_qore_qcc_warning_flags}
            ${_qore_qcc_stub_flags}
            ${_qore_qcc_load_flags}
            ${_qore_qcc_define_flags}
            ${_qore_qcc_parse_option_flags}
            ${_qore_qcc_source_symbol_flags}
            ${_qore_qcc_metadata_compression_flags}
            ${_qore_qcc_manifest_input_flags})
        set(_qore_qcc_single_cmd "${_qore_qcc_single_cmd} '${_qore_qcc_arg}'")
    endforeach()
    set(_qore_qcc_single_cmd "${_qore_qcc_single_cmd} -c -L \"$preload_dir\" --manifest-skip-qo-library-inputs \"$@\" --depfile=\"$out.d\" --depfile-target=\"$out.stamp\" --depfile-compile-contract-stamps --write-index-json=\"$out.idx.json\" --write-status-json=\"$out.status.json\" --success-stamp=\"$out.stamp\" --content-stamp=\"$out.content.stamp\" --compile-contract-stamp=\"$out.compile-contract.stamp\" --write-manifest=\"$out.source.manifest.json\" --skip-if-manifest-current -o \"$out\" \"$src\"\n")
    QORE_WRITE_IF_CHANGED("${_qore_qcc_single_script}" "${_qore_qcc_single_cmd}")

    list(LENGTH _qore_qcc_abs_sources _qore_qcc_count)
    if (_QORE_QCO_BOOTSTRAP_AGGREGATE_OUTPUT
            AND _qore_qcc_count LESS 2)
        message(FATAL_ERROR
            "QORE_QCC_COMPILE_OBJECTS(${_out_var}) bootstrap aggregate "
            "requires at least two sources")
    endif ()
    set(_qore_qcc_bootstrap_stamp)
    set(_qore_qcc_bootstrap_target)
    if (_qore_qcc_count GREATER 1)
        set(_qore_qcc_batch_script "${_QORE_QCO_SCRIPT_DIR}/qcc-batch.sh")
        set(_qore_qcc_batch_cmd "#!/bin/sh\nset -e\nQORE_INCLUDE_DIR='${_QORE_QCO_INCLUDE_DIR}' QORE_MODULE_DIR='${_QORE_QCO_MODULE_DIR}' exec '${_qore_qcc_command}'")
        foreach(_qore_qcc_arg
                ${_qore_qcc_warning_flags}
                ${_qore_qcc_stub_flags}
                ${_qore_qcc_load_flags}
                ${_qore_qcc_define_flags}
                ${_qore_qcc_parse_option_flags}
                ${_qore_qcc_source_symbol_flags}
                ${_qore_qcc_metadata_compression_flags}
                ${_qore_qcc_manifest_input_flags})
            set(_qore_qcc_batch_cmd "${_qore_qcc_batch_cmd} '${_qore_qcc_arg}'")
        endforeach()
        set(_qore_qcc_batch_cmd "${_qore_qcc_batch_cmd} -c")
        foreach(_qore_qcc_arg ${_qore_qcc_bootstrap_aggregate_flags})
            set(_qore_qcc_batch_cmd "${_qore_qcc_batch_cmd} '${_qore_qcc_arg}'")
        endforeach()
        set(_qore_qcc_batch_cmd "${_qore_qcc_batch_cmd} --batch-build-sidecars --depfile-compile-contract-stamps --output-dir='${_QORE_QCO_OUTPUT_DIR}' --depfile-dir='${_QORE_QCO_OUTPUT_DIR}'")
        foreach(_qore_qcc_source ${_qore_qcc_abs_sources})
            set(_qore_qcc_batch_cmd "${_qore_qcc_batch_cmd} '${_qore_qcc_source}'")
        endforeach()
        string(APPEND _qore_qcc_batch_cmd "\n")
        QORE_WRITE_IF_CHANGED("${_qore_qcc_batch_script}" "${_qore_qcc_batch_cmd}")

        set(_qore_qcc_bootstrap_stamp "${_QORE_QCO_SCRIPT_DIR}/.qcc-batch-bootstrap.stamp")
        set(_qore_qcc_bootstrap_target "qore_qcc_${_qore_qcc_group_id}_batch_bootstrap")
        add_custom_command(OUTPUT ${_qore_qcc_bootstrap_stamp}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_QORE_QCO_OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                ${_qore_qcc_bootstrap_aggregate_dir}
            COMMAND ${_qore_qcc_batch_bootstrap_helper}
                ${_qore_qcc_context_path}
                ${_qore_qcc_bootstrap_stamp}
                ${_qore_qcc_batch_script}
                ${_qore_qcc_source_order_helper}
            DEPENDS
                ${_qore_qcc_batch_script}
                ${_qore_qcc_context_path}
                ${_qore_qcc_source_symbols}
                ${_QORE_QCO_STUBS}
                ${_QORE_QCO_DEPENDS}
                ${_qore_qcc_load_module_target_deps}
                ${_qore_qcc_deps}
                ${_qore_qcc_batch_bootstrap_helper}
                ${_qore_qcc_source_order_helper}
                ${_QORE_QCO_MANIFEST_INPUTS}
                ${_QORE_QCO_BOOTSTRAP_AGGREGATE_CONTEXT}
                ${_QORE_QCO_BOOTSTRAP_AGGREGATE_MANIFEST_INPUTS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "qcc -c: batch bootstrapping ${_QORE_QCO_GROUP} (${_qore_qcc_count} sources)"
            VERBATIM)
        add_custom_target(${_qore_qcc_bootstrap_target}
            DEPENDS ${_qore_qcc_bootstrap_stamp})
    endif ()

    if (_qore_qcc_count GREATER 0)
        math(EXPR _qore_qcc_last "${_qore_qcc_count} - 1")
        foreach(_qore_qcc_idx RANGE 0 ${_qore_qcc_last})
            list(GET _qore_qcc_order_targets ${_qore_qcc_idx} _qore_qcc_order_target)
            list(GET _qore_qcc_stamps ${_qore_qcc_idx} _qore_qcc_stamp)
            add_custom_target(${_qore_qcc_order_target}
                DEPENDS ${_qore_qcc_stamp})
            if (_qore_qcc_bootstrap_target)
                add_dependencies(${_qore_qcc_order_target} ${_qore_qcc_bootstrap_target})
            endif ()
        endforeach()
        foreach(_qore_qcc_idx RANGE 0 ${_qore_qcc_last})
            list(GET _qore_qcc_abs_sources ${_qore_qcc_idx} _qore_qcc_source)
            list(GET _qore_qcc_outputs ${_qore_qcc_idx} _qore_qcc_output)
            set(_qore_qcc_stamp "${_qore_qcc_output}.stamp")
            set(_qore_qcc_direct_deps_var "${_qore_qcc_direct_deps_prefix}_${_qore_qcc_idx}")
            set(_qore_qcc_direct_deps ${${_qore_qcc_direct_deps_var}})
            set(_qore_qcc_direct_dep_compile_contract_stamps)
            set(_qore_qcc_direct_dep_order_targets)
            foreach(_qore_qcc_direct_dep ${_qore_qcc_direct_deps})
                list(APPEND _qore_qcc_direct_dep_compile_contract_stamps
                    "${_qore_qcc_direct_dep}.compile-contract.stamp")
                list(FIND _qore_qcc_outputs "${_qore_qcc_direct_dep}" _qore_qcc_direct_dep_idx)
                if (NOT _qore_qcc_direct_dep_idx EQUAL -1)
                    list(GET _qore_qcc_order_targets ${_qore_qcc_direct_dep_idx}
                        _qore_qcc_direct_dep_order_target)
                    list(APPEND _qore_qcc_direct_dep_order_targets
                        ${_qore_qcc_direct_dep_order_target})
                endif ()
            endforeach()
            add_custom_command(OUTPUT ${_qore_qcc_stamp}
                BYPRODUCTS
                    ${_qore_qcc_output}
                    ${_qore_qcc_output}.d
                    ${_qore_qcc_output}.idx.json
                    ${_qore_qcc_output}.status.json
                    ${_qore_qcc_output}.content.stamp
                    ${_qore_qcc_output}.compile-contract.stamp
                    ${_qore_qcc_output}.source.manifest.json
                    ${_qore_qcc_output}.source-parse-defines
                COMMAND ${CMAKE_COMMAND} -E make_directory ${_QORE_QCO_OUTPUT_DIR}
                COMMAND ${CMAKE_COMMAND} -E env
                    "QORE_QCC_BATCH_BOOTSTRAP_STAMP=${_qore_qcc_bootstrap_stamp}"
                    ${_qore_qcc_incremental_helper}
                        ${_qore_qcc_output}
                        ${_qore_qcc_source}
                        ${_qore_qcc_single_script}
                DEPENDS
                    ${_qore_qcc_source}
                    ${_qore_qcc_source_symbols}
                    ${_qore_qcc_single_script}
                    ${_qore_qcc_context_path}
                    ${_QORE_QCO_STUBS}
                    ${_QORE_QCO_DEPENDS}
                    ${_qore_qcc_load_module_target_deps}
                    ${_qore_qcc_deps}
                    ${_qore_qcc_direct_dep_order_targets}
                    ${_qore_qcc_direct_dep_compile_contract_stamps}
                    ${_qore_qcc_incremental_helper}
                    ${_qore_qcc_source_order_helper}
                    ${_QORE_QCO_MANIFEST_INPUTS}
                DEPFILE ${_qore_qcc_output}.d
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "qcc -c: compiling ${_QORE_QCO_GROUP} ${_qore_qcc_source} (.qo)"
                VERBATIM)
        endforeach()
    endif ()

    set_source_files_properties(${_qore_qcc_outputs}
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_source_files_properties(${_qore_qcc_stamps} ${_qore_qcc_content_stamps}
        ${_qore_qcc_compile_contract_stamps}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)

    set(${_out_var} ${_qore_qcc_outputs} PARENT_SCOPE)
    if (_QORE_QCO_STAMPS_VAR)
        set(${_QORE_QCO_STAMPS_VAR} ${_qore_qcc_stamps} PARENT_SCOPE)
    endif ()
    if (_QORE_QCO_CONTENT_STAMPS_VAR)
        set(${_QORE_QCO_CONTENT_STAMPS_VAR} ${_qore_qcc_content_stamps} PARENT_SCOPE)
    endif ()
    if (_QORE_QCO_COMPILE_CONTRACT_STAMPS_VAR)
        set(${_QORE_QCO_COMPILE_CONTRACT_STAMPS_VAR}
            ${_qore_qcc_compile_contract_stamps} PARENT_SCOPE)
    endif ()
    if (_QORE_QCO_ORDER_TARGETS_VAR)
        set(${_QORE_QCO_ORDER_TARGETS_VAR} ${_qore_qcc_order_targets} PARENT_SCOPE)
    endif ()
    if (_QORE_QCO_CONTEXT_VAR)
        set(${_QORE_QCO_CONTEXT_VAR} "${_qore_qcc_context_path}" PARENT_SCOPE)
    endif ()
endfunction()

function(QORE_QCC_LINK_OBJECTS _out_var)
    set(options WARNINGS_ARE_ERRORS ALLOW_UNRESOLVED_IMPORTS)
    set(oneValueArgs OUTPUT INCLUDE_DIR MODULE_DIR AGGREGATE_SYMBOL STAMP_VAR
        CONTENT_STAMP_VAR LINK_MAP_VAR INDEX_JSON_VAR STATUS_JSON_VAR MANIFEST_JSON_VAR)
    set(multiValueArgs INPUTS INPUT_CONTENT_STAMPS INPUT_ORDER_TARGETS MANIFEST_INPUTS DEPENDS)
    cmake_parse_arguments(_QORE_QLO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT _QORE_QLO_OUTPUT)
        message(FATAL_ERROR "QORE_QCC_LINK_OBJECTS(${_out_var}) requires OUTPUT")
    endif ()
    if (NOT _QORE_QLO_AGGREGATE_SYMBOL)
        message(FATAL_ERROR "QORE_QCC_LINK_OBJECTS(${_out_var}) requires AGGREGATE_SYMBOL")
    endif ()
    if (NOT _QORE_QLO_INCLUDE_DIR)
        set(_QORE_QLO_INCLUDE_DIR "$ENV{QORE_INCLUDE_DIR}")
    endif ()
    if (NOT _QORE_QLO_MODULE_DIR)
        set(_QORE_QLO_MODULE_DIR "$ENV{QORE_MODULE_DIR}")
    endif ()
    get_filename_component(_qore_qlo_dir "${_QORE_QLO_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_qore_qlo_dir}")
    QORE_GET_QCC_COMMAND(_qore_qcc_command)
    QORE_GET_QCC_DEPS(_qore_qcc_deps QCC_COMMAND "${_qore_qcc_command}")

    set(_qore_qlo_link_map "${_QORE_QLO_OUTPUT}.qolink.json")
    set(_qore_qlo_context "${_QORE_QLO_OUTPUT}.qolink-context")
    set(_qore_qlo_manifest "${_QORE_QLO_OUTPUT}.qolink.manifest.json")
    set(_qore_qlo_idx "${_QORE_QLO_OUTPUT}.idx.json")
    set(_qore_qlo_status "${_QORE_QLO_OUTPUT}.status.json")
    set(_qore_qlo_content_stamp "${_QORE_QLO_OUTPUT}.content.stamp")
    set(_qore_qlo_stamp "${_QORE_QLO_OUTPUT}.stamp")
    set(_qore_qlo_depfile "${_QORE_QLO_OUTPUT}.d")
    set(_qore_qlo_link_flags)
    if (_QORE_QLO_ALLOW_UNRESOLVED_IMPORTS)
        list(APPEND _qore_qlo_link_flags --allow-unresolved-imports)
    endif ()
    set(_qore_qlo_warning_flags)
    if (_QORE_QLO_WARNINGS_ARE_ERRORS)
        list(APPEND _qore_qlo_warning_flags --warnings-are-errors)
    endif ()
    set(_qore_qlo_aggregate_symbol "${_QORE_QLO_AGGREGATE_SYMBOL}")

    set(_qore_qlo_context_content "format=1\nkind=qcc-link-qo\nqcc=${_qore_qcc_command}\n")
    string(APPEND _qore_qlo_context_content "aggregate_symbol=${_qore_qlo_aggregate_symbol}\n")
    QORE_QCC_APPEND_CONTEXT(_qore_qlo_context_content link_flag ${_qore_qlo_link_flags})
    QORE_QCC_APPEND_CONTEXT(_qore_qlo_context_content input ${_QORE_QLO_INPUTS})
    QORE_QCC_APPEND_CONTEXT(_qore_qlo_context_content manifest_input ${_QORE_QLO_MANIFEST_INPUTS})
    QORE_WRITE_IF_CHANGED("${_qore_qlo_context}" "${_qore_qlo_context_content}")
    set(_qore_qlo_manifest_input_flags "--manifest-input=${_qore_qlo_context}")
    foreach(_qore_qlo_manifest_input ${_qore_qcc_deps} ${_QORE_QLO_MANIFEST_INPUTS})
        list(APPEND _qore_qlo_manifest_input_flags "--manifest-input=${_qore_qlo_manifest_input}")
    endforeach()

    add_custom_command(OUTPUT ${_qore_qlo_stamp}
        BYPRODUCTS
            ${_QORE_QLO_OUTPUT}
            ${_qore_qlo_link_map}
            ${_qore_qlo_manifest}
            ${_qore_qlo_depfile}
            ${_qore_qlo_idx}
            ${_qore_qlo_status}
            ${_qore_qlo_content_stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_qore_qlo_dir}
        COMMAND ${CMAKE_COMMAND} -E env
            "QORE_INCLUDE_DIR=${_QORE_QLO_INCLUDE_DIR}"
            "QORE_MODULE_DIR=${_QORE_QLO_MODULE_DIR}"
            ${_qore_qcc_command}
                ${_qore_qlo_warning_flags}
                --link-qo
                ${_qore_qlo_link_flags}
                -o ${_QORE_QLO_OUTPUT}
                --aggregate-symbol=${_qore_qlo_aggregate_symbol}
                --qolink-map=${_qore_qlo_link_map}
                --depfile=${_qore_qlo_depfile}
                --depfile-target=${_qore_qlo_stamp}
                --depfile-qo-input-content-stamps
                --write-index-json=${_qore_qlo_idx}
                --write-status-json=${_qore_qlo_status}
                --success-stamp=${_qore_qlo_stamp}
                --content-stamp=${_qore_qlo_content_stamp}
                ${_qore_qlo_manifest_input_flags}
                --write-manifest=${_qore_qlo_manifest}
                --skip-if-manifest-current
                ${_QORE_QLO_INPUTS}
        DEPENDS
            ${_QORE_QLO_INPUT_ORDER_TARGETS}
            ${_QORE_QLO_INPUT_CONTENT_STAMPS}
            ${_qore_qlo_context}
            ${_qore_qcc_deps}
            ${_QORE_QLO_DEPENDS}
            ${_QORE_QLO_MANIFEST_INPUTS}
        DEPFILE ${_qore_qlo_depfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "qcc --link-qo: ${_QORE_QLO_OUTPUT}"
        VERBATIM)
    set_source_files_properties(${_QORE_QLO_OUTPUT}
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_source_files_properties(${_qore_qlo_stamp}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    set(${_out_var} "${_QORE_QLO_OUTPUT}" PARENT_SCOPE)
    if (_QORE_QLO_STAMP_VAR)
        set(${_QORE_QLO_STAMP_VAR} "${_qore_qlo_stamp}" PARENT_SCOPE)
    endif ()
    if (_QORE_QLO_CONTENT_STAMP_VAR)
        set(${_QORE_QLO_CONTENT_STAMP_VAR} "${_qore_qlo_content_stamp}" PARENT_SCOPE)
    endif ()
    if (_QORE_QLO_LINK_MAP_VAR)
        set(${_QORE_QLO_LINK_MAP_VAR} "${_qore_qlo_link_map}" PARENT_SCOPE)
    endif ()
    if (_QORE_QLO_INDEX_JSON_VAR)
        set(${_QORE_QLO_INDEX_JSON_VAR} "${_qore_qlo_idx}" PARENT_SCOPE)
    endif ()
    if (_QORE_QLO_STATUS_JSON_VAR)
        set(${_QORE_QLO_STATUS_JSON_VAR} "${_qore_qlo_status}" PARENT_SCOPE)
    endif ()
    if (_QORE_QLO_MANIFEST_JSON_VAR)
        set(${_QORE_QLO_MANIFEST_JSON_VAR} "${_qore_qlo_manifest}" PARENT_SCOPE)
    endif ()
endfunction()

function(QORE_QCC_SCRIPT_AGGREGATE _out_var)
    set(options WARNINGS_ARE_ERRORS NATIVE_REGISTERS)
    set(oneValueArgs OUTPUT AGGREGATE INCLUDE_DIR MODULE_DIR METADATA_COMPRESSION
        STAMP_VAR CONTENT_STAMP_VAR INDEX_JSON_VAR STATUS_JSON_VAR MANIFEST_JSON_VAR)
    set(multiValueArgs SOURCES INPUT_CONTENT_STAMPS INPUT_ORDER_TARGETS STUBS LOAD_MODULES
        PARSE_DEFINES PARSE_OPTIONS MANIFEST_INPUTS DEPENDS)
    cmake_parse_arguments(_QORE_QSA "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT _QORE_QSA_OUTPUT)
        message(FATAL_ERROR "QORE_QCC_SCRIPT_AGGREGATE(${_out_var}) requires OUTPUT")
    endif ()
    if (NOT _QORE_QSA_AGGREGATE)
        message(FATAL_ERROR "QORE_QCC_SCRIPT_AGGREGATE(${_out_var}) requires AGGREGATE")
    endif ()
    if (NOT _QORE_QSA_INCLUDE_DIR)
        set(_QORE_QSA_INCLUDE_DIR "$ENV{QORE_INCLUDE_DIR}")
    endif ()
    if (NOT _QORE_QSA_MODULE_DIR)
        set(_QORE_QSA_MODULE_DIR "$ENV{QORE_MODULE_DIR}")
    endif ()
    get_filename_component(_qore_qsa_dir "${_QORE_QSA_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${_qore_qsa_dir}")
    QORE_GET_QCC_COMMAND(_qore_qcc_command)
    QORE_GET_QCC_DEPS(_qore_qcc_deps QCC_COMMAND "${_qore_qcc_command}")

    set(_qore_qsa_manifest "${_QORE_QSA_OUTPUT}.script-aggregate.manifest.json")
    set(_qore_qsa_context "${_QORE_QSA_OUTPUT}.script-aggregate-context")
    set(_qore_qsa_idx "${_QORE_QSA_OUTPUT}.idx.json")
    set(_qore_qsa_status "${_QORE_QSA_OUTPUT}.status.json")
    set(_qore_qsa_content_stamp "${_QORE_QSA_OUTPUT}.content.stamp")
    set(_qore_qsa_stamp "${_QORE_QSA_OUTPUT}.stamp")
    set(_qore_qsa_stub_flags)
    foreach(_qore_qsa_stub ${_QORE_QSA_STUBS})
        list(APPEND _qore_qsa_stub_flags "--stub=${_qore_qsa_stub}")
    endforeach()
    set(_qore_qsa_load_flags)
    foreach(_qore_qsa_module ${_QORE_QSA_LOAD_MODULES})
        list(APPEND _qore_qsa_load_flags -l ${_qore_qsa_module})
    endforeach()
    QORE_QCC_LOAD_MODULE_TARGET_DEPS(_qore_qsa_load_module_target_deps
        ${_QORE_QSA_LOAD_MODULES})
    set(_qore_qsa_define_flags)
    foreach(_qore_qsa_define ${_QORE_QSA_PARSE_DEFINES})
        list(APPEND _qore_qsa_define_flags "--define=${_qore_qsa_define}")
    endforeach()
    set(_qore_qsa_parse_option_flags)
    foreach(_qore_qsa_parse_option ${_QORE_QSA_PARSE_OPTIONS})
        list(APPEND _qore_qsa_parse_option_flags "--parse-option=${_qore_qsa_parse_option}")
    endforeach()
    set(_qore_qsa_metadata_flags)
    if (_QORE_QSA_METADATA_COMPRESSION)
        list(APPEND _qore_qsa_metadata_flags
            "--aot-metadata-compression=${_QORE_QSA_METADATA_COMPRESSION}")
    endif ()
    set(_qore_qsa_warning_flags)
    if (_QORE_QSA_WARNINGS_ARE_ERRORS)
        list(APPEND _qore_qsa_warning_flags --warnings-are-errors)
    endif ()
    set(_qore_qsa_native_flags)
    if (_QORE_QSA_NATIVE_REGISTERS)
        list(APPEND _qore_qsa_native_flags --script-aggregate-native-registers)
    endif ()
    set(_qore_qsa_index_byproducts)
    set(_qore_qsa_index_flags)
    set(_qore_qsa_index_cleanup)
    if (_QORE_QSA_INDEX_JSON_VAR)
        list(APPEND _qore_qsa_index_byproducts ${_qore_qsa_idx})
        list(APPEND _qore_qsa_index_flags --write-index-json=${_qore_qsa_idx})
    else ()
        file(REMOVE "${_qore_qsa_idx}")
        set(_qore_qsa_index_cleanup
            COMMAND ${CMAKE_COMMAND} -E rm -f ${_qore_qsa_idx})
    endif ()

    set(_qore_qsa_context_content "format=1\nkind=qcc-script-aggregate\nqcc=${_qore_qcc_command}\n")
    string(APPEND _qore_qsa_context_content "aggregate=${_QORE_QSA_AGGREGATE}\n")
    string(APPEND _qore_qsa_context_content "qore_include_dir=${_QORE_QSA_INCLUDE_DIR}\n")
    string(APPEND _qore_qsa_context_content "qore_module_dir=${_QORE_QSA_MODULE_DIR}\n")
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content stub ${_QORE_QSA_STUBS})
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content load_module ${_QORE_QSA_LOAD_MODULES})
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content parse_define ${_QORE_QSA_PARSE_DEFINES})
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content parse_option ${_QORE_QSA_PARSE_OPTIONS})
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content input ${_QORE_QSA_SOURCES})
    QORE_QCC_APPEND_CONTEXT(_qore_qsa_context_content manifest_input ${_QORE_QSA_MANIFEST_INPUTS})
    QORE_WRITE_IF_CHANGED("${_qore_qsa_context}" "${_qore_qsa_context_content}")
    set(_qore_qsa_manifest_input_flags "--manifest-input=${_qore_qsa_context}")
    foreach(_qore_qsa_manifest_input ${_qore_qcc_deps} ${_QORE_QSA_MANIFEST_INPUTS})
        list(APPEND _qore_qsa_manifest_input_flags "--manifest-input=${_qore_qsa_manifest_input}")
    endforeach()

    add_custom_command(OUTPUT ${_qore_qsa_stamp}
        BYPRODUCTS
            ${_QORE_QSA_OUTPUT}
            ${_qore_qsa_manifest}
            ${_qore_qsa_index_byproducts}
            ${_qore_qsa_status}
            ${_qore_qsa_content_stamp}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_qore_qsa_dir}
        ${_qore_qsa_index_cleanup}
        COMMAND ${CMAKE_COMMAND} -E env
            "QORE_INCLUDE_DIR=${_QORE_QSA_INCLUDE_DIR}"
            "QORE_MODULE_DIR=${_QORE_QSA_MODULE_DIR}"
            ${_qore_qcc_command}
                ${_qore_qsa_warning_flags}
                -c
                -o ${_QORE_QSA_OUTPUT}
                --script-aggregate=${_QORE_QSA_AGGREGATE}
                ${_qore_qsa_native_flags}
                ${_qore_qsa_index_flags}
                --write-status-json=${_qore_qsa_status}
                --success-stamp=${_qore_qsa_stamp}
                --content-stamp=${_qore_qsa_content_stamp}
                --write-manifest=${_qore_qsa_manifest}
                --skip-if-manifest-current
                ${_qore_qsa_stub_flags}
                ${_qore_qsa_load_flags}
                ${_qore_qsa_define_flags}
                ${_qore_qsa_parse_option_flags}
                ${_qore_qsa_metadata_flags}
                ${_qore_qsa_manifest_input_flags}
                ${_QORE_QSA_SOURCES}
        DEPENDS
            ${_QORE_QSA_INPUT_ORDER_TARGETS}
            ${_QORE_QSA_INPUT_CONTENT_STAMPS}
            ${_QORE_QSA_STUBS}
            ${_qore_qsa_context}
            ${_qore_qsa_load_module_target_deps}
            ${_qore_qcc_deps}
            ${_QORE_QSA_DEPENDS}
            ${_QORE_QSA_MANIFEST_INPUTS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "qcc --script-aggregate: ${_QORE_QSA_AGGREGATE}"
        VERBATIM)

    set_source_files_properties(${_QORE_QSA_OUTPUT}
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_source_files_properties(${_qore_qsa_stamp}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    set(${_out_var} "${_QORE_QSA_OUTPUT}" PARENT_SCOPE)
    if (_QORE_QSA_STAMP_VAR)
        set(${_QORE_QSA_STAMP_VAR} "${_qore_qsa_stamp}" PARENT_SCOPE)
    endif ()
    if (_QORE_QSA_CONTENT_STAMP_VAR)
        set(${_QORE_QSA_CONTENT_STAMP_VAR} "${_qore_qsa_content_stamp}" PARENT_SCOPE)
    endif ()
    if (_QORE_QSA_INDEX_JSON_VAR)
        set(${_QORE_QSA_INDEX_JSON_VAR} "${_qore_qsa_idx}" PARENT_SCOPE)
    endif ()
    if (_QORE_QSA_STATUS_JSON_VAR)
        set(${_QORE_QSA_STATUS_JSON_VAR} "${_qore_qsa_status}" PARENT_SCOPE)
    endif ()
    if (_QORE_QSA_MANIFEST_JSON_VAR)
        set(${_QORE_QSA_MANIFEST_JSON_VAR} "${_qore_qsa_manifest}" PARENT_SCOPE)
    endif ()
endfunction()

#
# Create C++ code using the new value API from the QPP files
#
#  _cpp_files : output list of filenames created in CMAKE_CURRENT_BINARY_DIR.
#
# Optional one-value keyword args:
#  DOXLIST   : output list variable — receives the generated .dox.h paths.
#  METALIST  : output list variable — receives the generated .meta.json
#              metadata paths (used for `qore_install_qpp_metadata`).
#  STUBLIST  : output list variable — receives the generated .stub.qc
#              compile-time Qore-syntax stub paths.  When set, qpp is
#              invoked with `--stub-output=<path>` so the stub file is
#              built alongside the .cpp.  Stubs are consumed by qcc's
#              `--stub=<path>` flag in AOT pipelines that need Qore-
#              syntax declarations of the C++-backed classes qpp emits.
#              Omit the keyword (or pass an empty target variable) to
#              skip stub generation entirely — most core/library callers
#              that don't feed an AOT pipeline should leave this unset.
#
# usage:
# set(MY_QPP foo.qpp bar.qpp)
# qore_wrap_qpp_value(MY_CPP ${MY_QPP})
# qore_wrap_qpp_value(MY_CPP DOXLIST MY_DOX METALIST MY_META STUBLIST MY_STUB ${MY_QPP})
#
MACRO (QORE_WRAP_QPP_VALUE _cpp_files)
    set(options)
    set(oneValueArgs DOXLIST METALIST STUBLIST)
    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(_WRAP_QPP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    FOREACH (it ${_WRAP_QPP_UNPARSED_ARGUMENTS})
        GET_FILENAME_COMPONENT(_outfile ${it} NAME_WE)
        GET_FILENAME_COMPONENT(_infile ${it} ABSOLUTE)
        SET(_cppfile ${CMAKE_CURRENT_BINARY_DIR}/${_outfile}.cpp)
        SET(_doxfile ${CMAKE_CURRENT_BINARY_DIR}/${_outfile}.dox.h)
        SET(_metafile ${CMAKE_CURRENT_BINARY_DIR}/${_outfile}.meta.json)

        # Stub output is opt-in: the caller must pass STUBLIST <var> to
        # activate it, otherwise qpp is not told to produce a stub and
        # no .stub.qc file is listed as an OUTPUT.  Callers outside AOT
        # pipelines don't need stubs and shouldn't pay the generation
        # cost (or the extra dependency edge).
        SET(_stub_arg)
        SET(_stub_outputs)
        IF(_WRAP_QPP_STUBLIST)
            SET(_stubfile ${CMAKE_CURRENT_BINARY_DIR}/${_outfile}.stub.qc)
            SET(_stub_arg --stub-output=${_stubfile})
            SET(_stub_outputs ${_stubfile})
        ENDIF(_WRAP_QPP_STUBLIST)

        ADD_CUSTOM_COMMAND(OUTPUT ${_cppfile} ${_doxfile} ${_metafile} ${_stub_outputs}
                           COMMAND ${QORE_QPP_EXECUTABLE}
                           ARGS --javadoc=${CMAKE_CURRENT_BINARY_DIR}/java --output=${_cppfile} --dox-output=${_doxfile} --metadata=${_metafile} ${_stub_arg} ${_infile}
                           MAIN_DEPENDENCY ${_infile}
                           DEPENDS ${QORE_QPP_EXECUTABLE}
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                           VERBATIM
                        )
        SET(${_cpp_files} ${${_cpp_files}} ${_cppfile})
        IF(_WRAP_QPP_DOXLIST)
           SET(${_WRAP_QPP_DOXLIST} ${${_WRAP_QPP_DOXLIST}} ${_doxfile} ${_javadocfile})
        ENDIF(_WRAP_QPP_DOXLIST)
        IF(_WRAP_QPP_METALIST)
           SET(${_WRAP_QPP_METALIST} ${${_WRAP_QPP_METALIST}} ${_metafile})
        ENDIF(_WRAP_QPP_METALIST)
        IF(_WRAP_QPP_STUBLIST)
           SET(${_WRAP_QPP_STUBLIST} ${${_WRAP_QPP_STUBLIST}} ${_stubfile})
        ENDIF(_WRAP_QPP_STUBLIST)
        #MESSAGE(STATUS "DEBUG D: " _WRAP_QPP_DOXLIST " ${D}:" ${_WRAP_QPP_DOXLIST} " ${${D}}:" ${${_WRAP_QPP_DOXLIST}})
    ENDFOREACH (it)

ENDMACRO (QORE_WRAP_QPP_VALUE)

#
# Install qpp-generated .meta.json files into a per-module subdirectory
# under the metadata directory.
#
# usage:
#   qore_install_qpp_metadata(mymodule ${QPP_META_SOURCES})
#
MACRO (QORE_INSTALL_QPP_METADATA _module_name)
    set(_meta_files ${ARGN})
    if (_meta_files AND DEFINED QORE_METADATA_DIR)
        install(FILES ${_meta_files}
                DESTINATION ${QORE_METADATA_DIR}/${_module_name})
    endif()
ENDMACRO (QORE_INSTALL_QPP_METADATA)

#
# Extract metadata from Qore source (.qm/.qc) files at build time using
# qore-extract-qm-metadata. Generates .meta.json files and installs them
# into a per-module subdirectory under the metadata directory.
#
# Requires: qore + astparser + QoreApiMetadata modules to be available.
# Set QORE_QM_METADATA_ENV for custom environment (e.g. QORE_MODULE_DIR,
# LD_LIBRARY_PATH) and QORE_QM_METADATA_DEPENDS for build dependencies.
#
# usage:
#   qore_extract_qm_metadata(mymodule qlib/Foo.qm qlib/Bar.qm)
#
MACRO (QORE_EXTRACT_QM_METADATA _module_name)
    if(DEFINED QORE_QM_METADATA_EXECUTABLE AND DEFINED QORE_METADATA_DIR)
        if(DEFINED QORE_EXECUTABLE)
            set(_qore_qm_metadata_command
                ${QORE_EXECUTABLE} ${QORE_QM_METADATA_EXECUTABLE})
        else()
            set(_qore_qm_metadata_command ${QORE_QM_METADATA_EXECUTABLE})
        endif()
        set(_qm_meta_files "")
        foreach(_qm_file ${ARGN})
            get_filename_component(_qm_basename ${_qm_file} NAME)
            set(_qm_meta_out ${CMAKE_CURRENT_BINARY_DIR}/${_qm_basename}.meta.json)
            if(DEFINED QORE_QM_METADATA_ENV)
                add_custom_command(OUTPUT ${_qm_meta_out}
                    COMMAND ${CMAKE_COMMAND} -E env ${QORE_QM_METADATA_ENV}
                        ${_qore_qm_metadata_command}
                        ${CMAKE_CURRENT_SOURCE_DIR}/${_qm_file}
                        ${_qm_meta_out}
                    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_qm_file}
                    COMMENT "Extracting metadata from ${_qm_file}"
                    VERBATIM
                )
            else()
                add_custom_command(OUTPUT ${_qm_meta_out}
                    COMMAND ${_qore_qm_metadata_command}
                        ${CMAKE_CURRENT_SOURCE_DIR}/${_qm_file}
                        ${_qm_meta_out}
                    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_qm_file}
                    COMMENT "Extracting metadata from ${_qm_file}"
                    VERBATIM
                )
            endif()
            list(APPEND _qm_meta_files ${_qm_meta_out})
        endforeach()
        add_custom_target(${_module_name}-qm-metadata ALL
            DEPENDS ${_qm_meta_files})
        if(QORE_QM_METADATA_DEPENDS)
            add_dependencies(${_module_name}-qm-metadata
                ${QORE_QM_METADATA_DEPENDS})
        endif()
        install(FILES ${_qm_meta_files}
                DESTINATION ${QORE_METADATA_DIR}/${_module_name})
    endif()
ENDMACRO (QORE_EXTRACT_QM_METADATA)

#
# Create dox code from dox.tmpl files
#
#  _dox_files : output dox filenames created in CMAKE_CURRENT_BINARY_DIR
#
# usage:
# set(MY_DOX_TMPL foo.dox.tmpl bar.dox.tmpl)
# qore_wrap_dox(MY_DOX ${MY_DOX_TMPL})
#
MACRO (QORE_WRAP_DOX _dox_files)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(_WRAP_QPP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    FOREACH (it ${_WRAP_QPP_UNPARSED_ARGUMENTS})

        GET_FILENAME_COMPONENT(_outfile ${it} NAME_WE)
        GET_FILENAME_COMPONENT(_infile ${it} ABSOLUTE)
        SET(_doxfile ${CMAKE_CURRENT_BINARY_DIR}/${_outfile}.dox)

        ADD_CUSTOM_COMMAND(OUTPUT ${_doxfile}
                           COMMAND ${QORE_QPP_EXECUTABLE}
                           ARGS --table=${_infile} --output=${_doxfile}
                           MAIN_DEPENDENCY ${_infile}
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                           VERBATIM
                        )
        SET(${_dox_files} ${${_dox_files}} ${_doxfile})
    ENDFOREACH (it)

ENDMACRO (QORE_WRAP_DOX)

MACRO (QORE_BINARY_MODULE _module_name _version)
   QORE_BINARY_MODULE_INTERN(${_module_name} ${_version} "" ${ARGN})
ENDMACRO (QORE_BINARY_MODULE)

# Create qore binary module.
# Arguments:
#  _module_name - string name of the module
#  _version - a version. Used in -DPACKAGE_VERSION=...
# [additional libraries] - optional list of libs
#
# The initial shared library is renamed to for Qore modules naming convention.
# Linking with [additional libraries] is enabled.
# libqore is linked automatically.
#
# additional targets created:
#  'make docs' - if there is Doxygen found
#  'make uninstall' - if exists CMAKE_CURRENT_SOURCE_DIR/cmake/cmake_uninstall.cmake.in
MACRO (QORE_BINARY_MODULE_INTERN _module_name _version _install_suffix)
    QORE_BINARY_MODULE_INTERN2(${_module_name} ${_version} "${_install_suffix}" "" ${ARGN})
ENDMACRO (QORE_BINARY_MODULE_INTERN)

MACRO (QORE_BINARY_MODULE_QORE _module_name _version _install_suffix)
    QORE_BINARY_MODULE_INTERN2(${_module_name} ${_version} "${_install_suffix}" 1 ${ARGN})
ENDMACRO (QORE_BINARY_MODULE_QORE)

# macro for external binary modules; supports complex documentation generation with QORE_EXTERNAL_USER_MODULE as well
MACRO (QORE_EXTERNAL_BINARY_MODULE _module_name _version)
    QORE_BINARY_MODULE_INTERN2(${_module_name} ${_version} "" 2 ${ARGN})
    set(_external_module_name ${_module_name})
    if (TARGET ${_module_name})
        list(FIND QORE_AOT_BINARY_MODULE_TARGETS ${_module_name} _qore_aot_binary_module_found)
        if (_qore_aot_binary_module_found EQUAL -1)
            list(APPEND QORE_AOT_BINARY_MODULE_TARGETS ${_module_name})
        endif()
        unset(_qore_aot_binary_module_found)
        list(FIND QORE_QM_METADATA_DEPENDS ${_module_name} _qore_qm_metadata_dep_found)
        if (_qore_qm_metadata_dep_found EQUAL -1)
            list(APPEND QORE_QM_METADATA_DEPENDS ${_module_name})
        endif()
        unset(_qore_qm_metadata_dep_found)
    endif()
    if (NOT DEFINED QORE_QM_METADATA_ENV)
        set(_qore_external_module_path
            "${CMAKE_SOURCE_DIR}/qlib:${CMAKE_BINARY_DIR}/qlib-qmod:${CMAKE_BINARY_DIR}")
        # see QORE_GET_BINARY_MODULE_PATH(): in-tree binary modules take precedence over installed ones
        QORE_GET_BINARY_MODULE_PATH(_qore_external_binary_module_path)
        if (NOT "${_qore_external_binary_module_path}" STREQUAL "")
            set(_qore_external_module_path
                "${_qore_external_binary_module_path}:${_qore_external_module_path}")
        endif()
        unset(_qore_external_binary_module_path)
        if (DEFINED QORE_BUILDTREE_USER_MODULE_PATH
                AND NOT "${QORE_BUILDTREE_USER_MODULE_PATH}" STREQUAL "")
            set(_qore_external_module_path
                "${_qore_external_module_path}:${QORE_BUILDTREE_USER_MODULE_PATH}")
        endif()
        set(QORE_QM_METADATA_ENV
            "QORE_MODULE_DIR=${_qore_external_module_path}"
            "QORE_INCLUDE_DIR="
            "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}")
        unset(_qore_external_module_path)
    endif()
ENDMACRO (QORE_EXTERNAL_BINARY_MODULE)

MACRO (QORE_BINARY_MODULE_INTERN2 _module_name _version _install_suffix _mod_suffix)
    if ("${_mod_suffix}" STREQUAL "")
        set(_docs_targ docs)
        set(_uninstall_targ uninstall)
        set(_working_dir ${CMAKE_BINARY_DIR})
        set(_dox_src ${CMAKE_SOURCE_DIR})
        set(_dox_output ${CMAKE_BINARY_DIR})
    elseif ("${_mod_suffix}" STREQUAL "1")
        set(_docs_targ docs-${_module_name})
        set(_uninstall_targ uninstall-${_module_name})
        set(_working_dir ${CMAKE_BINARY_DIR}/modules/${_module_name})
        set(_dox_src ${CMAKE_SOURCE_DIR}/modules/${_module_name}/src)
        set(_dox_output ${CMAKE_BINARY_DIR}/docs/modules/${_module_name})
        set(QORE_MOD_NAME ${_module_name})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

        if ("${QORE_USERMODULE_DOXYGEN_TEMPLATE}" STREQUAL "")
            set(QORE_USERMODULE_DOXYGEN_TEMPLATE ${CMAKE_SOURCE_DIR}/doxygen/modules/Doxyfile.cmake.in)
        endif ()
    else()
        # this configurtion ensures that the primary binary module docs are build first as target "docs-module"
        # the "docs" target them will depend on "docs-module", and all user modules will also depend on "docs-module"
        # so that they can use symbols from the primary binary module
        set(_docs_targ docs-module)
        set(_extra_docs_targ docs)
        set(_uninstall_targ uninstall)
        set(_working_dir ${CMAKE_BINARY_DIR})
        set(_dox_src ${CMAKE_SOURCE_DIR})
        set(_dox_output "${CMAKE_BINARY_DIR}/docs/${_module_name}")
    endif()

    if (DEFINED MODULE_DOX_INPUT)
        string(REPLACE ";" " " MODULE_DOX_INPUT_STR "${MODULE_DOX_INPUT}")
        set(_dox_input ${MODULE_DOX_INPUT_STR})
    else ()
        set(_dox_input ${CMAKE_BINARY_DIR})
    endif ()

    # standard repeating stuff for modules
    add_definitions("-DPACKAGE_VERSION=\"${_version}\"")
    if(DEFINED QORE_PRE_INCLUDES)
       include_directories( ${QORE_PRE_INCLUDES} )
    endif()
    include_directories( ${QORE_INCLUDE_DIRS} )
    include_directories( ${CMAKE_BINARY_DIR} )

    # compiler stuff
    if (CMAKE_COMPILER_IS_GNUCXX)
        SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
    endif (CMAKE_COMPILER_IS_GNUCXX)

    # add BUILDING_DLL=1 define to modules' CXXFLAGS
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILDING_DLL=1")

    # setup the target
    set (_libs "")
    foreach (value ${ARGN})
        set(_libs "${_libs};${value}")
    endforeach (value)

    # add pthread library on Windows
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_libs "${_libs};pthread")
    endif()

    # add additional libraries
    if(DEFINED QORE_POST_LIBS)
        foreach (value ${QORE_POST_LIBS})
            set(_libs "${_libs};${value}")
        endforeach (value)
    endif()

    # set install target dir
    set(_mod_target_dir ${QORE_MODULES_DIR}${_install_suffix})

    set_target_properties(${_module_name} PROPERTIES PREFIX "" SUFFIX "-api-${QORE_API_VERSION}.qmod")

    # issue #3802: do not link with the qore library
    target_link_libraries(${_module_name} ${_libs})

    # ensure that modules use dynamic lookups; works with g++ & clang++
    if(CMAKE_HOST_APPLE)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-undefined -Wl,dynamic_lookup")
    endif(CMAKE_HOST_APPLE)

    # NOTE: install(TARGETS) for a shared module already unlinks-then-writes a
    # NEW inode on install (libqore.so itself showed "(deleted)" in the
    # rebuilt-under-a-live-process crash and did not tear), so a running
    # dlopen() keeps its old inode mapping intact.  Only the AOT .qmod path,
    # which used plain install(FILES), needed the atomic stage-and-rename
    # treatment (see QORE_INSTALL_QMOD_ATOMIC).
    install(TARGETS ${_module_name}
        DESTINATION ${_mod_target_dir}
        COMPONENT ${QORE_BINARY_MODULE_INSTALL_COMPONENT})

    if (APPLE)
        # It should allow to use full path in the module reference itself. otool -L /path/to/module.qmod, 1st line.
        set_target_properties(${_module_name} PROPERTIES INSTALL_NAME_DIR ${QORE_MODULES_DIR})
    endif (APPLE)
    set_target_properties(${_module_name} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

    # uninstall
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in")
        # make uninstall
        CONFIGURE_FILE(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY
        )
        ADD_CUSTOM_TARGET(${_uninstall_targ}
            "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

        if (NOT "${_uninstall_suffix}" STREQUAL "")
            add_dependencies(uninstall ${_uninstall_targ})
        endif ()

        message(STATUS "")
        message(STATUS "Module ${_module_name} uninstall target: make uninstall")
        message(STATUS "")
    elseif (NOT "${_mod_suffix}" STREQUAL "1")
        message(WARNING "Module ${_module_name} uninstall script: no file: ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in")
    endif()

    # docs: do not try to find doxygen again when building Qore
    if (NOT "${_mod_suffix}" STREQUAL "1")
        FIND_PACKAGE(Doxygen)
    endif()
    if (DOXYGEN_FOUND)
        if (EXISTS "${QORE_USERMODULE_DOXYGEN_TEMPLATE}")
            set(CURRENT_MODULE_NAME ${_module_name})
            configure_file(${QORE_USERMODULE_DOXYGEN_TEMPLATE} ${_working_dir}/Doxyfile @ONLY)

            if (WIN32 OR MSYS OR MINGW)
                set(_qore_qjar_module_dir "${_working_dir}")
                if (DEFINED QORE_MODULE_DIR_FOR_DOCS AND NOT "${QORE_MODULE_DIR_FOR_DOCS}" STREQUAL "")
                    set(_qore_qjar_module_dir "${_qore_qjar_module_dir}\\;${QORE_MODULE_DIR_FOR_DOCS}")
                elseif (DEFINED ENV{QORE_MODULE_DIR} AND NOT "$ENV{QORE_MODULE_DIR}" STREQUAL "")
                    set(_qore_qjar_module_dir "${_qore_qjar_module_dir}\\;$ENV{QORE_MODULE_DIR}")
                endif()
            else()
                set(_qore_qjar_module_dir "${_working_dir}")
                if (DEFINED QORE_MODULE_DIR_FOR_DOCS AND NOT "${QORE_MODULE_DIR_FOR_DOCS}" STREQUAL "")
                    set(_qore_qjar_module_dir "${_qore_qjar_module_dir}:${QORE_MODULE_DIR_FOR_DOCS}")
                elseif (DEFINED ENV{QORE_MODULE_DIR} AND NOT "$ENV{QORE_MODULE_DIR}" STREQUAL "")
                    set(_qore_qjar_module_dir "${_qore_qjar_module_dir}:$ENV{QORE_MODULE_DIR}")
                endif()
            endif()
            set(_qore_qjar_env QORE_MODULE_DIR=${_qore_qjar_module_dir} QORE_DOC_DEFINES=${QORE_DOC_DEFINES})

            add_custom_target(${_docs_targ}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${_dox_output}
                COMMAND ${DOXYGEN_EXECUTABLE} ${_working_dir}/Doxyfile
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${_dox_output}/html ${_dox_output}/html/search
                COMMAND ${_qore_qjar_env} ${QORE_QJAR_COMMAND} -i ${CMAKE_BINARY_DIR}/java -m ${_module_name}
                BYPRODUCTS ${CMAKE_BINARY_DIR}/java
                WORKING_DIRECTORY ${_working_dir}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM
            )
            add_dependencies(${_docs_targ} ${_module_name})

            if (NOT "${_extra_docs_targ}" STREQUAL "")
                add_custom_target(${_extra_docs_targ})
                add_dependencies(${_extra_docs_targ} ${_docs_targ})
            endif ()

            if (NOT "${_mod_suffix}" STREQUAL "")
                add_dependencies(docs ${_docs_targ})
            endif ()

            message(STATUS "")
            message(STATUS "Module ${_module_name} documentation target: make docs")
            message(STATUS "")
        else()
            message(WARNING "User module doxygen template file does not exist: ${QORE_USERMODULE_DOXYGEN_TEMPLATE}")
        endif()
    else (DOXYGEN_FOUND)
        message(WARNING "Doxygen not found. Documentation won't be built.")
    endif (DOXYGEN_FOUND)
ENDMACRO (QORE_BINARY_MODULE_INTERN2)

MACRO (QORE_AOT_APPEND_INSTALL_REMOVE_PATH _out_var _base_dir _relative_path)
    if (IS_ABSOLUTE "${_base_dir}")
        set(_qore_aot_remove_path "\$ENV{DESTDIR}${_base_dir}/${_relative_path}")
    else()
        set(_qore_aot_remove_path
            "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_base_dir}/${_relative_path}")
    endif()
    string(APPEND ${_out_var} "  \"${_qore_aot_remove_path}\"\n")
ENDMACRO (QORE_AOT_APPEND_INSTALL_REMOVE_PATH)

FUNCTION (QORE_AOT_REMOVE_STALE_SOURCE_QMOD_INSTALL_RULES _name _is_dir _base_dir)
    set(_qore_aot_remove_component_args "")
    if (ARGC GREATER 3 AND NOT "${ARGV3}" STREQUAL "")
        set(_qore_aot_remove_component_args COMPONENT ${ARGV3})
    endif()
    set(_qore_aot_stale_source_qmods "")
    QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
        _qore_aot_stale_source_qmods "${_base_dir}" "${_name}.qmod")
    QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
        _qore_aot_stale_source_qmods "${_base_dir}" "${_name}.qmod.d")
    if (${_is_dir})
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qore_aot_stale_source_qmods "${_base_dir}" "${_name}/${_name}.qmod")
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qore_aot_stale_source_qmods "${_base_dir}" "${_name}/${_name}.qmod.d")
    endif()
    install(CODE "file(REMOVE\n${_qore_aot_stale_source_qmods})"
        ${_qore_aot_remove_component_args})
ENDFUNCTION (QORE_AOT_REMOVE_STALE_SOURCE_QMOD_INSTALL_RULES)

MACRO (QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL _name _is_dir)
    QORE_AOT_REMOVE_STALE_SOURCE_QMOD_INSTALL_RULES(
        ${_name} ${_is_dir} "${QORE_USER_MODULES_DIR}" ${QORE_QM_SOURCE_INSTALL_COMPONENT})
    if (DEFINED QORE_AOT_MODULES_DIR
            AND NOT "${QORE_AOT_MODULES_DIR}" STREQUAL "${QORE_USER_MODULES_DIR}")
        QORE_AOT_REMOVE_STALE_SOURCE_QMOD_INSTALL_RULES(
            ${_name} ${_is_dir} "${QORE_AOT_MODULES_DIR}" ${QORE_QM_SOURCE_INSTALL_COMPONENT})
    endif()
ENDMACRO (QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL)

# Atomically publish a built AOT .qmod into its install location.
#
# CMake's plain install(FILES) copies file content into the destination path.
# On some filesystems (notably overlayfs on containerized dev/CI nodes) this
# rewrites the EXISTING destination inode in place.  A process that already has
# the old .qmod dlopen()'d holds a MAP_PRIVATE mapping of that inode; an
# in-place rewrite tears the mapping -- clean text pages re-fault to the new
# bytes while the relocated GOT/PLT pages keep the old copy.  AOT code then
# calls a libqore runtime helper through an un-relocated PLT slot, jumps to a
# garbage (link-time) address and SIGSEGVs.  Reusing a freed inode can also
# make glibc's dlopen() dedup (keyed on dev+inode) return a stale,
# already-relocated link_map for the new file, with the same result.
#
# Publishing via a freshly-created temp file in the destination directory
# followed by rename(2) guarantees:
#   * the new .qmod always gets a brand-new inode the running process never
#     mapped (defeats both the torn-mapping and the dlopen inode-reuse paths);
#   * the swap is atomic, so a concurrent dlopen() never observes a partial
#     file (also closes the overlayfs truncated-read window);
#   * the old inode is unlinked but kept alive by any live mapping, so already
#     running services keep working on the old code until they reload.
#
# This only matters for development installs over a live tree (production
# installs modules before starting the process), so the extra copy is cheap.
#
# _src        absolute path of the built .qmod in the build tree
# _dest_dir   install directory (absolute, or relative to CMAKE_INSTALL_PREFIX)
# _dest_name  installed file name (e.g. <name>.qmod)
# _component  install component (may be empty)
FUNCTION (QORE_INSTALL_QMOD_ATOMIC _src _dest_dir _dest_name _component)
    if (IS_ABSOLUTE "${_dest_dir}")
        set(_qore_qmod_dest "\$ENV{DESTDIR}${_dest_dir}")
    else()
        set(_qore_qmod_dest "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_dest_dir}")
    endif()
    set(_qore_qmod_component_args "")
    if (NOT "${_component}" STREQUAL "")
        set(_qore_qmod_component_args COMPONENT ${_component})
    endif()
    install(CODE "
set(_qore_qmod_dir \"${_qore_qmod_dest}\")
set(_qore_qmod_dst \"\${_qore_qmod_dir}/${_dest_name}\")
set(_qore_qmod_tmp \"\${_qore_qmod_dst}.inst.tmp\")
file(MAKE_DIRECTORY \"\${_qore_qmod_dir}\")
file(REMOVE \"\${_qore_qmod_tmp}\")
# Stage the new bytes into a fresh inode in the destination directory, then
# atomically rename(2) it onto the final name.  cmake -E copy (rather than
# file(COPY_FILE), which needs CMake 3.21) keeps this working at the declared
# 3.14 floor.  Staging in the same directory keeps src and dst on one
# filesystem so the rename is a single atomic inode swap.
execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E copy \"${_src}\" \"\${_qore_qmod_tmp}\"
    RESULT_VARIABLE _qore_qmod_rc)
if (NOT _qore_qmod_rc EQUAL 0)
    message(FATAL_ERROR \"failed to stage AOT qmod '${_src}' -> '\${_qore_qmod_tmp}': \${_qore_qmod_rc}\")
endif()
file(RENAME \"\${_qore_qmod_tmp}\" \"\${_qore_qmod_dst}\")
message(STATUS \"Atomically installed: \${_qore_qmod_dst}\")
" ${_qore_qmod_component_args})
ENDFUNCTION (QORE_INSTALL_QMOD_ATOMIC)

# Emit AOT-build rules for a user module producing a .qmod via qcc.
#
# Gated on QORE_BUILD_AOT_MODULES; uses the in-tree qcc target when building
# Qore itself, and QORE_QCC_EXECUTABLE from QoreConfig.cmake for external
# binary module builds.
#
# Args (positional):
#   _name        module base name (e.g. Util, DataProvider)
#   _is_dir      "1" for split-dir modules (qlib/<name>/), "0" for single-file
#   _source_root for _is_dir=1: absolute path to the module directory
#                for _is_dir=0: absolute path to the primary .qm file
# Variadic:     absolute paths of all .qm + .qc source files in the module
#
# The .qmod lands in ${QORE_AOT_MODULES_DIR} at install time.  Core qlib uses
# the versioned arch-specific module directory, while external module builds
# default to their arch-specific module directory from QoreConfig.cmake.
MACRO (QORE_USER_MODULE_AOT_RULES _name _is_dir _source_root)
    if (TARGET qcc)
        set(_qore_qcc_command $<TARGET_FILE:qcc>)
    elseif (DEFINED QORE_QCC_EXECUTABLE)
        set(_qore_qcc_command ${QORE_QCC_EXECUTABLE})
    else()
        message(FATAL_ERROR "QORE_BUILD_AOT_MODULES=ON requires qcc or QORE_QCC_EXECUTABLE")
    endif()

    # The qcc AOT compile below runs under ${QORE_QM_METADATA_ENV}, whose
    # QORE_MODULE_DIR must include this build's own modules so that a module
    # which %requires a sibling built earlier in the SAME tree (e.g. a
    # provider module requiring its loader module) can resolve it at AOT
    # parse time.  QORE_EXTERNAL_BINARY_MODULE sets this env, but a pure-Qore
    # user-module repo (no binary module) never calls it, leaving the env
    # unset -- qcc then sees only the installed module path, which does not
    # yet contain the just-built sibling, and fails with LOAD-MODULE-ERROR.
    # Default the env here (mirroring the binary-module path) so in-tree
    # siblings resolve through the build qlib-qmod dir ahead of dependents
    # (the qmod target dependency added in QORE_USER_MODULE guarantees the
    # ordering).
    if (NOT DEFINED QORE_QM_METADATA_ENV)
        # qlib-qmod is where each sibling's AOT qmod is emitted
        # (<dir>/<name>/<name>.qmod).  Include qlib-qmod explicitly so
        # resolution does not depend on generated source-tree symlinks.
        set(_qore_user_module_path
            "${CMAKE_SOURCE_DIR}/qlib:${CMAKE_BINARY_DIR}/qlib-qmod:${CMAKE_BINARY_DIR}")
        # in-tree binary modules must come first so that a user module which %requires one
        # resolves the just-built copy rather than the one installed on the system
        QORE_GET_BINARY_MODULE_PATH(_qore_user_binary_module_path)
        if (NOT "${_qore_user_binary_module_path}" STREQUAL "")
            set(_qore_user_module_path
                "${_qore_user_binary_module_path}:${_qore_user_module_path}")
        endif()
        unset(_qore_user_binary_module_path)
        if (DEFINED QORE_BUILDTREE_USER_MODULE_PATH
                AND NOT "${QORE_BUILDTREE_USER_MODULE_PATH}" STREQUAL "")
            set(_qore_user_module_path
                "${_qore_user_module_path}:${QORE_BUILDTREE_USER_MODULE_PATH}")
        endif()
        set(QORE_QM_METADATA_ENV
            "QORE_MODULE_DIR=${_qore_user_module_path}"
            "QORE_INCLUDE_DIR="
            "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}")
        unset(_qore_user_module_path)
    endif()

    if (NOT DEFINED QCC_FORMAT_STAMP)
        set(QCC_FORMAT_STAMP ${CMAKE_BINARY_DIR}/qcc-format.stamp)
        add_custom_command(
            OUTPUT ${QCC_FORMAT_STAMP}
            COMMAND ${CMAKE_COMMAND} -E touch ${QCC_FORMAT_STAMP}
            DEPENDS ${_qore_qcc_command}
            COMMENT "Updating qcc format-version stamp"
            VERBATIM
        )
        if (NOT TARGET qcc-format-version)
            add_custom_target(qcc-format-version DEPENDS ${QCC_FORMAT_STAMP})
        endif()
    endif()
    if (NOT DEFINED QORE_AOT_LINK_SOURCE_MODULES)
        # Default ON: the in-tree test suite anchors module lookup at the source
        # qlib/ directory, so without these qlib/<name>.qmod -> built-qmod symlinks
        # every .qm module loads as slow source-parse instead of the AOT artifact.
        # (This silently defaulted OFF after the macro rewrite, regressing in-tree
        # AOT module load times ~20x; develop has always defaulted this ON.)
        set(QORE_AOT_LINK_SOURCE_MODULES ON)
    endif()
    if (DEFINED QORE_AOT_MODULES_DIR)
        set(_qmod_install_dir ${QORE_AOT_MODULES_DIR})
    elseif (DEFINED QORE_MODULES_DIR)
        set(_qmod_install_dir ${QORE_MODULES_DIR})
    else()
        set(_qmod_install_dir ${QORE_USER_MODULES_DIR})
    endif()

    # Split-dir modules emit `build/qlib-qmod/<name>/<name>.qmod` and copy
    # sibling resources (logo SVGs, asyncapi YAMLs, etc.) into the same
    # directory so that `get_script_dir()` during AOT init resolves to a
    # directory containing those resources.  Constants like
    # `const FooLogo = File::readTextFile(get_script_dir() + "/foo-logo.svg")`
    # fire during `qore_aot_module_ns_init` under a
    # ProgramThreadCountContextHelper that points at the module's own
    # Program — whose script_dir is the .qmod's directory.  A flat qmod
    # without sibling resources would hit FILE-OPEN2-ERROR and raise
    # AOT-PENDING-CONSTANT on every downstream `registerApp(...)` call.
    # Single-file modules (no subdir) have no resources, so they keep the
    # flat layout.
    set(_qmod_flat_dir ${CMAKE_BINARY_DIR}/qlib-qmod)
    if (${_is_dir})
        set(_qmod_out_dir ${_qmod_flat_dir}/${_name})
        set(_qmod_source_link ${CMAKE_SOURCE_DIR}/qlib/${_name}/${_name}.qmod)
    else()
        set(_qmod_out_dir ${_qmod_flat_dir})
        set(_qmod_source_link ${CMAKE_SOURCE_DIR}/qlib/${_name}.qmod)
    endif()
    set(_qmod_out ${_qmod_out_dir}/${_name}.qmod)
    file(MAKE_DIRECTORY ${_qmod_out_dir})

    # One-shot cleanup of stale flat artifacts from the pre-subdir layout
    # (cmake commit that moved split-dir qmods into subdirs).  Without this,
    # ModuleManager::loadModuleIntern finds `<dir>/<name>.qmod` first
    # (flat takes precedence in the search order) and loads the stale
    # pre-fix qmod instead of the current subdir one.  Runs at configure
    # time; `file(REMOVE)` is a no-op on nonexistent paths, so it's
    # idempotent across clean configures.
    if (${_is_dir})
        file(REMOVE
            "${_qmod_flat_dir}/${_name}.qmod"
            "${_qmod_flat_dir}/${_name}.qmod.d"
            "${CMAKE_SOURCE_DIR}/qlib/${_name}.qmod")
    endif()

    # Both split-dir and single-file use the direct `qcc -m` path.
    # Split-dir takes the module directory; single-file takes the .qm.
    #
    # Historical note: the macro originally used per-file `qcc -c` +
    # `qcc -m --from-objects` for split-dir modules, intending to exploit
    # make parallelism across the N component .qc files.  Measurement on
    # DataProvider (134 files) showed whole-dir compile is actually
    # FASTER wall-clock (69s vs 93s @ -j8) — per-file re-parses the full
    # context directory N times and the redundant N-parse overhead eats
    # the parallelism benefit.  Per-file's real win is incremental
    # rebuild granularity, not clean-build throughput.  Additionally the
    # per-file path hits a latent slice-4 bug in extractAOTSlotIdentities
    # (SIGSEGV on some modules, e.g. FileLocationHandlerHttp.qc) that
    # whole-dir avoids.  Revisit per-file mode after the slice-4 fix and
    # add an opt-in option for dev-loop incremental builds.
    set(_qmod_dep ${_qmod_out}.d)

    # For split-dir modules, collect sibling resource files at cmake-time
    # so they can be declared as BYPRODUCTS of the copy step and tracked
    # by make for rebuild triggers when the source changes.
    set(_qmod_resource_copies "")
    set(_qmod_resource_srcs "")
    set(_qmod_jar_srcs "")
    set(_qmod_jar_stage_commands "")
    if (${_is_dir})
        file(GLOB _qmod_resource_srcs
            "${_source_root}/*.svg"
            "${_source_root}/*.yaml"
            "${_source_root}/*.json"
            "${_source_root}/*.proto")
        if (IS_DIRECTORY "${_source_root}/jar")
            file(GLOB _qmod_jar_srcs "${_source_root}/jar/*.jar")
            set(_qmod_jar_stage_commands
                COMMAND ${CMAKE_COMMAND} -E make_directory ${_qmod_out_dir}/jar
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${_source_root}/jar ${_qmod_out_dir}/jar)
        endif()
    endif()

    # Dependency strategy: qmod output depends on the module's own
    # source files (ARGN), ${QCC_FORMAT_STAMP}, and
    # ${QORE_AOT_PROBE_STAMP} (see root CMakeLists.txt).  ${QCC_FORMAT_STAMP}
    # tracks files that change qmod wire format or qcc compile semantics --
    # not every libqore source edit.  ${QORE_AOT_PROBE_STAMP} tracks which
    # external binary modules (json, yaml, xml, ...) were loadable at
    # cmake configure time; the .qm sources use %try-module / %ifdef
    # No<Name> to branch on availability, and that decision is baked
    # into the AOT-compiled .qmod, so re-AOT must fire when the probe
    # outcome changes.  Target-level `add_dependencies` below ensures
    # qcc and libqore are BUILT before qmod rules run (needed for cold
    # builds) without making qmod rules mtime-dependent on their
    # binaries.  Previously `DEPENDS $<TARGET_FILE:qcc>` triggered all
    # 238 qmod rebuilds on any libqore relink, even for runtime-only
    # fixes that don't affect qmod output.  QoreAOTRuntime.cpp is part of
    # the stamp because it deserializes and registers slot-map metadata from
    # qmods; stale qmods can fail at load time if that path changes.
    set(_qmod_probe_dep "")
    if (DEFINED QORE_AOT_PROBE_STAMP)
        set(_qmod_probe_dep ${QORE_AOT_PROBE_STAMP})
    endif()
    if (${_is_dir})
        # Optional symlink at qlib/<name>/<name>.qmod (inside the source
        # subdir, alongside the resources).  Tests using
        # %prepend-module-path "${SCRIPT_DIR}/../../../../qlib" drive
        # module lookup into the <name>/ folder where the loader prefers
        # the `.qmod` form per the within-folder preference rule added
        # to ModuleManager::loadModuleIntern.
        add_custom_command(
            OUTPUT ${_qmod_out}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_qmod_out_dir}
            ${_qmod_jar_stage_commands}
            COMMAND ${CMAKE_COMMAND} -E env ${QORE_QM_METADATA_ENV}
                ${_qore_qcc_command} -m ${_source_root}
                --depfile=${_qmod_dep} --depfile-module-deps=source -o ${_qmod_out}
            DEPENDS ${ARGN} ${_qmod_jar_srcs} ${QCC_FORMAT_STAMP} ${_qmod_probe_dep}
            DEPFILE ${_qmod_dep}
            WORKING_DIRECTORY ${_qmod_out_dir}
            COMMENT "AOT compile ${_name}.qmod"
            VERBATIM
        )
        # Per-resource copy commands so rebuilds fire when a resource
        # changes in the source tree.  Outputs go into the same dir as
        # the qmod so `get_script_dir()` finds them at init time.
        foreach(_res ${_qmod_resource_srcs})
            get_filename_component(_res_name ${_res} NAME)
            set(_res_out ${_qmod_out_dir}/${_res_name})
            add_custom_command(
                OUTPUT ${_res_out}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${_qmod_out_dir}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_res} ${_res_out}
                DEPENDS ${_res}
                COMMENT "Copy ${_name} resource ${_res_name}"
                VERBATIM
            )
            list(APPEND _qmod_resource_copies ${_res_out})
        endforeach()
    else()
        # Optionally drop a symlink at qlib/<name>.qmod pointing at the
        # freshly built qmod.  The in-tree test suite uses
        # %prepend-module-path "${SCRIPT_DIR}/../../../../qlib" to anchor
        # module lookup at the source qlib/ directory; placing the qmod
        # there alongside the .qm lets loadModuleIntern's within-dir
        # preference order pick the AOT artifact.
        add_custom_command(
            OUTPUT ${_qmod_out}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_qmod_out_dir}
            COMMAND ${CMAKE_COMMAND} -E env ${QORE_QM_METADATA_ENV}
                ${_qore_qcc_command} -m ${_source_root}
                --depfile=${_qmod_dep} --depfile-module-deps=source -o ${_qmod_out}
            DEPENDS ${ARGN} ${QCC_FORMAT_STAMP} ${_qmod_probe_dep}
            DEPFILE ${_qmod_dep}
            WORKING_DIRECTORY ${_qmod_out_dir}
            COMMENT "AOT compile ${_name}.qmod"
            VERBATIM
        )
    endif()

    set(_qmod_source_link_dep "")
    if (QORE_AOT_LINK_SOURCE_MODULES)
        add_custom_command(
            OUTPUT ${_qmod_source_link}
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${_qmod_out} ${_qmod_source_link}
            DEPENDS ${_qmod_out}
            COMMENT "Link ${_name}.qmod into source qlib"
            VERBATIM
        )
        set(_qmod_source_link_dep ${_qmod_source_link})
    endif()

    add_custom_target(${_name}-qmod ALL DEPENDS
        ${_qmod_out}
        ${_qmod_source_link_dep}
        ${_qmod_resource_copies})
    # Collect qmod targets so a post-build "ensure symlinks" step can run after
    # all of them and self-heal any source qmod symlink the format-change cleanup
    # removed but a partial/up-to-date build did not recreate.
    if (QORE_AOT_LINK_SOURCE_MODULES)
        set_property(GLOBAL APPEND PROPERTY QORE_SOURCE_QMOD_LINK_TARGETS ${_name}-qmod)
    endif()
    # Ensure qcc executable + libqore.so are built before this qmod
    # rule runs — target-level deps, not mtime deps, so a newer qcc
    # binary doesn't force a qmod rebuild on its own.
    if (TARGET qcc)
        add_dependencies(${_name}-qmod qcc)
    endif()
    if (TARGET qcc-format-version)
        add_dependencies(${_name}-qmod qcc-format-version)
    endif()
    if (TARGET qcc-format-source-qmod-clean)
        add_dependencies(${_name}-qmod qcc-format-source-qmod-clean)
    endif()
    if (DEFINED QORE_AOT_BINARY_MODULE_TARGETS)
        foreach(_qore_aot_binary_module ${QORE_AOT_BINARY_MODULE_TARGETS})
            if (TARGET ${_qore_aot_binary_module})
                add_dependencies(${_name}-qmod ${_qore_aot_binary_module})
            endif()
        endforeach()
    endif()

    # Install qmods into an arch-specific module directory.  Split-dir module
    # qmods keep their resources beside the qmod so get_script_dir() resolves
    # the same way it does in the build tree.
    set(_qmod_stale_install_paths "")
    QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
        _qmod_stale_install_paths "${QORE_USER_MODULES_DIR}" "${_name}.qmod")
    QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
        _qmod_stale_install_paths "${QORE_USER_MODULES_DIR}" "${_name}.qmod.d")
    if (${_is_dir})
        # Remove stale artifacts from older layouts.  Flat qmods shadow split
        # qmods within one module path entry, and old share-installed qmods can
        # be loaded when AOT is later disabled or an installed qmod is missing.
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qmod_stale_install_paths "${QORE_USER_MODULES_DIR}" "${_name}/${_name}.qmod")
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qmod_stale_install_paths "${QORE_USER_MODULES_DIR}" "${_name}/${_name}.qmod.d")
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qmod_stale_install_paths "${_qmod_install_dir}" "${_name}.qmod")
        QORE_AOT_APPEND_INSTALL_REMOVE_PATH(
            _qmod_stale_install_paths "${_qmod_install_dir}" "${_name}.qmod.d")
        install(CODE "file(REMOVE\n${_qmod_stale_install_paths})"
            COMPONENT ${QORE_QMOD_INSTALL_COMPONENT})
        # Atomic stage-and-rename publish (new inode) so rebuilding under a live
        # process doesn't tear the running dlopen() mapping -- see
        # QORE_INSTALL_QMOD_ATOMIC.
        QORE_INSTALL_QMOD_ATOMIC("${_qmod_out}" "${_qmod_install_dir}/${_name}"
            "${_name}.qmod" "${QORE_QMOD_INSTALL_COMPONENT}")
        if (_qmod_resource_srcs)
            install(FILES ${_qmod_resource_srcs}
                DESTINATION ${_qmod_install_dir}/${_name}
                COMPONENT ${QORE_QMOD_INSTALL_COMPONENT})
        endif()
        # Make the module's jar/ dir available beside the AOT .qmod so its
        # baked `add-relative-classpath ./jar/...` (and cross-module refs like
        # `../ExcelDataProvider/jar/...`) resolve at module load.  The jars are
        # already installed beside the .qm source under QORE_USER_MODULES_DIR,
        # and they are large (POI/Camel/... ~160 MB across modules), so where
        # symlinks are supported we link the AOT-side jar/ to the source-side
        # one instead of duplicating it.  The link is RELATIVE so it survives
        # DESTDIR staging and prefix relocation (e.g. the docker build's
        # `mv /opt /buildroot`).  This is done at INSTALL time, not via a
        # configure-time file list, because a jni module's own
        # qore-dataprovider-*.jar is generated during the build (and is
        # gitignored), so it is absent when cmake first configures.  Falls back
        # to a real copy on Windows.  Cross-module deps resolve because the
        # referenced module installs its own jar/ link by the same rule.
        if (NOT WIN32 AND NOT "${_qmod_install_dir}" STREQUAL "${QORE_USER_MODULES_DIR}")
            file(RELATIVE_PATH _qmod_jar_rel
                "${_qmod_install_dir}/${_name}"
                "${QORE_USER_MODULES_DIR}/${_name}/jar")
            install(CODE
"set(_src_jar_dir \"\$ENV{DESTDIR}${QORE_USER_MODULES_DIR}/${_name}/jar\")
if (EXISTS \"\${_src_jar_dir}\")
    set(_aot_mod_dir \"\$ENV{DESTDIR}${_qmod_install_dir}/${_name}\")
    file(MAKE_DIRECTORY \"\${_aot_mod_dir}\")
    file(REMOVE_RECURSE \"\${_aot_mod_dir}/jar\")
    file(CREATE_LINK \"${_qmod_jar_rel}\" \"\${_aot_mod_dir}/jar\" SYMBOLIC)
    message(STATUS \"AOT jar dir symlink: \${_aot_mod_dir}/jar -> ${_qmod_jar_rel}\")
endif()"
                COMPONENT ${QORE_QMOD_INSTALL_COMPONENT})
        else()
            install(DIRECTORY ${_source_root}/jar/
                DESTINATION ${_qmod_install_dir}/${_name}/jar
                COMPONENT ${QORE_QMOD_INSTALL_COMPONENT}
                FILES_MATCHING PATTERN "*.jar")
        endif()
    else()
        install(CODE "file(REMOVE\n${_qmod_stale_install_paths})"
            COMPONENT ${QORE_QMOD_INSTALL_COMPONENT})
        # Atomic stage-and-rename publish (new inode); see QORE_INSTALL_QMOD_ATOMIC.
        QORE_INSTALL_QMOD_ATOMIC("${_qmod_out}" "${_qmod_install_dir}"
            "${_name}.qmod" "${QORE_QMOD_INSTALL_COMPONENT}")
    endif()
ENDMACRO (QORE_USER_MODULE_AOT_RULES)

# Install qore native/user module (.qm file) into the proper location.
#
# NOTE: this macro is for the library only
#
# Param #1: path to the module; e.g. "qlib/RestHandler.qm"
# Param #2: list of module dependencies separated by semicolon; e.g. "HttpServerUtil;Mime;Util"
#
# Example:
#     qore_user_module("qlib/RestHandler.qm" "HttpServerUtil;Mime;Util")
#
# The module will be installed automatically in 'make install' target.
MACRO (QORE_USER_MODULE _module_file)
    get_filename_component(f ${_module_file} NAME_WE)
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
        file(GLOB _mod_targets "${CMAKE_SOURCE_DIR}/qlib/${f}/*.qm" "${CMAKE_SOURCE_DIR}/qlib/${f}/*.qc"
            "${CMAKE_SOURCE_DIR}/qlib/${f}/*.yaml" "${CMAKE_SOURCE_DIR}/qlib/${f}/*.svg"
            "${CMAKE_SOURCE_DIR}/qlib/${f}/*.proto")
        set(qm_install_subdir "${f}") # install files into a subdir
        #message(STATUS "_mod_targets ${_mod_targets}")
    else()
        set(_mod_targets ${_module_file})
        set(qm_install_subdir "") # common qm file
    endif()

    # any args after the module path are extra resource files (logo SVGs,
    # asyncapi YAMLs, etc.) installed alongside the module
    set (_extra_files ${ARGN})

    # Register the module for the final dependency-wiring pass.  Inter-module
    # build-order dependencies are derived from each module's own %requires
    # directives in QORE_FINALIZE_USER_MODULE_DEPENDENCIES (the single source of
    # truth), run after every module target exists -- so a module may %requires
    # a sibling declared later in this file without any ordering concern.
    set_property(GLOBAL APPEND PROPERTY QORE_USER_MODULE_TARGETS ${f})

    # Add module name to the global list for documentation cross-referencing
    set(QORE_USER_MODULE_NAMES ${QORE_USER_MODULE_NAMES} ${f} CACHE INTERNAL "List of user module names for doc cross-referencing")

    if (DOXYGEN_FOUND)
        # get module name
        #message(STATUS "Preparing generation of documentation for module: ${f}")

        # prepare directories for the documentation
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/doxygen/qlib/)

        # prepare needed vars
        set(MOD_DOXYFILE "${CMAKE_BINARY_DIR}/doxygen/Doxyfile.${f}")
        unset(MOD_DEPS)
        foreach(i ${_effective_mod_deps})
            # we must use relative directories for tags; using absolute paths for tags will break the documentation
            # when used on any system except the one where it's generated
            get_filename_component(f0 ${i} NAME)
            SET(MOD_DEPS ${MOD_DEPS} -t${i}.tag=../../${f0}/html)
        endforeach(i)

        SET(EXTRA_FILES)
        foreach(i ${_extra_files})
            SET(EXTRA_FILES ${EXTRA_FILES} ${i})
        endforeach(i)

        # prepare QDX arguments
        if (EXTRA_FILES)
            if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
                set(_module_src_dir ${CMAKE_SOURCE_DIR}/qlib/${f})
            else()
                get_filename_component(_module_src_dir ${CMAKE_SOURCE_DIR}/${_module_file} DIRECTORY)
            endif()
            set(_qdx_extra_file_args)
            foreach(i ${EXTRA_FILES})
                list(APPEND _qdx_extra_file_args --extra-files ${i})
            endforeach()
            set(QDX_DOXYFILE_ARGS -T${CMAKE_SOURCE_DIR} -M=${CMAKE_SOURCE_DIR}/${_module_file}:${CMAKE_BINARY_DIR}/doxygen/qlib/${f}.qm.dox.h ${MOD_DEPS} ${CMAKE_SOURCE_DIR}/doxygen/qlib/Doxyfile.cmake.tmpl ${MOD_DOXYFILE} --extra-prefix ${_module_src_dir}/ ${_qdx_extra_file_args})
            unset(_qdx_extra_file_args)
        else (EXTRA_FILES)
            set(QDX_DOXYFILE_ARGS -T${CMAKE_SOURCE_DIR} -M=${CMAKE_SOURCE_DIR}/${_module_file}:${CMAKE_BINARY_DIR}/doxygen/qlib/${f}.qm.dox.h ${MOD_DEPS} ${CMAKE_SOURCE_DIR}/doxygen/qlib/Doxyfile.cmake.tmpl ${MOD_DOXYFILE})
        endif (EXTRA_FILES)
        set(QDX_QMDOXH_ARGS ${CMAKE_SOURCE_DIR}/${_module_file} ${CMAKE_BINARY_DIR}/doxygen/qlib/${f}.qm.dox.h)

        # add CMake target for the documentation
        if (WIN32 AND (NOT MINGW) AND (NOT MSYS))
            add_custom_target(docs-${f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/modules/${f}
                COMMAND set ${QORE_DOCS_ENV}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_DOXYFILE_ARGS}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_QMDOXH_ARGS}
                COMMAND ${DOXYGEN_EXECUTABLE} ${MOD_DOXYFILE}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/search/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QJAR_COMMAND} -i ${CMAKE_BINARY_DIR}/java -m ${f}
                BYPRODUCTS ${CMAKE_BINARY_DIR}/java
                COMMENT "Generating API documentation with Doxygen for module: ${f}"
                VERBATIM
            )
            add_dependencies(docs-${f} qore)
            add_custom_target(docs-fast-${f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/modules/${f}
                COMMAND set ${QORE_DOCS_ENV}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_DOXYFILE_ARGS}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_QMDOXH_ARGS}
                COMMAND ${DOXYGEN_EXECUTABLE} ${MOD_DOXYFILE}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/search/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QJAR_COMMAND} -i ${CMAKE_BINARY_DIR}/java -m ${f}
                BYPRODUCTS ${CMAKE_BINARY_DIR}/java
                COMMENT "Generating API documentation with Doxygen for module: ${f}"
                VERBATIM
            )
            add_dependencies(docs-fast-${f} qore)
        else (WIN32 AND (NOT MINGW) AND (NOT MSYS))
            add_custom_target(docs-${f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/modules/${f}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_DOXYFILE_ARGS}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_QMDOXH_ARGS}
                COMMAND ${DOXYGEN_EXECUTABLE} ${MOD_DOXYFILE}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/search/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QJAR_COMMAND} -i ${CMAKE_BINARY_DIR}/java -m ${f}
                BYPRODUCTS ${CMAKE_BINARY_DIR}/java
                COMMENT "Generating API documentation with Doxygen for module: ${f}"
                VERBATIM
            )
            add_dependencies(docs-${f} qore)
            add_custom_target(docs-fast-${f}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/modules/${f}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_DOXYFILE_ARGS}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_QMDOXH_ARGS}
                COMMAND ${DOXYGEN_EXECUTABLE} ${MOD_DOXYFILE}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/modules/${f}/html/search/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QJAR_COMMAND} -i ${CMAKE_BINARY_DIR}/java -m ${f}
                BYPRODUCTS ${CMAKE_BINARY_DIR}/java
                COMMENT "Generating API documentation with Doxygen for module: ${f}"
                VERBATIM
            )
            add_dependencies(docs-fast-${f} qore)
        endif (WIN32 AND (NOT MINGW) AND (NOT MSYS))

        # make 'docs' target dependent on this module documentation
        add_dependencies(docs docs-${f})

        # make this dependent on Qore lang and lib documentation targets
        add_dependencies(docs-${f} docs-lang)
        add_dependencies(docs-${f} docs-lib)

        # docs cross-module dependencies are wired comprehensively in
        # QORE_FINALIZE_USER_MODULE_DEPENDENCIES from each module's %requires
        # directives, after every docs-<module> target exists.
    endif (DOXYGEN_FOUND)

    # install qm file
    install(FILES ${_mod_targets}
        DESTINATION ${QORE_USER_MODULES_DIR}/${qm_install_subdir}
        COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
        QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL(${f} 1)
    else()
        QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL(${f} 0)
    endif()

    # extract and install QM metadata (automatic for user modules)
    if(DEFINED QORE_QM_METADATA_EXECUTABLE AND DEFINED QORE_METADATA_DIR
            AND DEFINED QORE_QM_METADATA_SUBDIR)
        if(DEFINED QORE_EXECUTABLE)
            set(_qore_qm_metadata_command
                ${QORE_EXECUTABLE} ${QORE_QM_METADATA_EXECUTABLE})
        else()
            set(_qore_qm_metadata_command ${QORE_QM_METADATA_EXECUTABLE})
        endif()
        set(_um_qm_meta_files "")
        # use module-specific subdirectory to avoid collisions when
        # different modules contain .qc files with the same basename
        set(_um_meta_dir ${CMAKE_CURRENT_BINARY_DIR}/qm-metadata/${f})
        file(MAKE_DIRECTORY ${_um_meta_dir})
        foreach(_src_file ${_mod_targets})
            get_filename_component(_src_ext ${_src_file} EXT)
            if("${_src_ext}" STREQUAL ".qm" OR "${_src_ext}" STREQUAL ".qc")
                # resolve relative paths to absolute
                if(NOT IS_ABSOLUTE "${_src_file}")
                    set(_src_file "${CMAKE_SOURCE_DIR}/${_src_file}")
                endif()
                get_filename_component(_src_name ${_src_file} NAME)
                set(_meta_out ${_um_meta_dir}/${_src_name}.meta.json)
                if(DEFINED QORE_QM_METADATA_ENV)
                    add_custom_command(OUTPUT ${_meta_out}
                        COMMAND ${CMAKE_COMMAND} -E env
                            ${QORE_QM_METADATA_ENV}
                            ${_qore_qm_metadata_command}
                            ${_src_file} ${_meta_out}
                        DEPENDS ${_src_file}
                        COMMENT "Extracting metadata from ${_src_name}"
                        VERBATIM
                    )
                else()
                    add_custom_command(OUTPUT ${_meta_out}
                        COMMAND ${_qore_qm_metadata_command}
                            ${_src_file} ${_meta_out}
                        DEPENDS ${_src_file}
                        COMMENT "Extracting metadata from ${_src_name}"
                        VERBATIM
                    )
                endif()
                list(APPEND _um_qm_meta_files ${_meta_out})
            endif()
        endforeach()
        if(_um_qm_meta_files)
            add_custom_target(qm-metadata-${f} ALL
                DEPENDS ${_um_qm_meta_files})
            set_property(GLOBAL APPEND PROPERTY
                QORE_QM_METADATA_TARGETS qm-metadata-${f})
            if(QORE_QM_METADATA_DEPENDS)
                add_dependencies(qm-metadata-${f}
                    ${QORE_QM_METADATA_DEPENDS})
            endif()
            install(FILES ${_um_qm_meta_files}
                DESTINATION ${QORE_METADATA_DIR}/${QORE_QM_METADATA_SUBDIR}
                COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
        endif()
    endif()

    # AOT: build a .qmod alongside the .qm source when enabled
    if (QORE_BUILD_AOT_MODULES AND (TARGET qcc OR DEFINED QORE_QCC_EXECUTABLE))
        if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
            QORE_USER_MODULE_AOT_RULES(${f} 1
                ${CMAKE_SOURCE_DIR}/qlib/${f} ${_mod_targets})
        else()
            QORE_USER_MODULE_AOT_RULES(${f} 0
                ${CMAKE_SOURCE_DIR}/${_module_file} ${_mod_targets})
        endif()
        # Inter-qmod build-order edges are wired comprehensively in
        # QORE_FINALIZE_USER_MODULE_DEPENDENCIES, after every module target
        # exists, from each module's %requires directives.
    endif()
ENDMACRO (QORE_USER_MODULE)

# Parse the module names a user module depends on from the %requires directives
# in its source.  Sets <_out_var> in the caller scope to a de-duplicated list of
# module leaf-names, excluding the "qore" language pseudo-module and self.
#
# Scans the main .qm plus, for split-dir modules, every .qm/.qc in the directory
# (split-dir .qc files carry their own %requires, e.g. DataProvider's .qc files
# `%requires reflection`).  Matches `%requires Foo` and `%requires(reexport) Foo`
# but deliberately NOT `%try-module Foo` (optional dependencies that load
# gracefully when absent, so they must not create hard build-order edges).
#
# Each scanned source is registered on CMAKE_CONFIGURE_DEPENDS so that editing a
# module's %requires re-runs CMake configure (nothing else triggers reconfigure
# on a .qm edit).
function(_QORE_PARSE_MODULE_REQUIRES _mod _out_var)
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${_mod})
        file(GLOB _src_files
            "${CMAKE_SOURCE_DIR}/qlib/${_mod}/*.qm"
            "${CMAKE_SOURCE_DIR}/qlib/${_mod}/*.qc")
    elseif (EXISTS ${CMAKE_SOURCE_DIR}/qlib/${_mod}.qm)
        set(_src_files "${CMAKE_SOURCE_DIR}/qlib/${_mod}.qm")
    else()
        set(${_out_var} "" PARENT_SCOPE)
        return()
    endif()

    set(_reqs "")
    foreach(_f ${_src_files})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_f})
        file(STRINGS ${_f} _lines)
        set(_in_doc_example FALSE)
        foreach(_line ${_lines})
            # %requires inside doxygen @code/@verbatim example blocks are sample
            # user scripts, not real module dependencies -- skip them.  (Check the
            # @end* markers first; they do not contain the opening token.)
            if (_line MATCHES "@endcode" OR _line MATCHES "@endverbatim")
                set(_in_doc_example FALSE)
            elseif (_line MATCHES "@code" OR _line MATCHES "@verbatim")
                set(_in_doc_example TRUE)
            elseif (NOT _in_doc_example AND
                    _line MATCHES "^%requires(\\(reexport\\))?[ \t]+([^ \t><=\r]+)")
                # capture the module token after an optional (reexport) option, up
                # to the first whitespace or version-operator character
                get_filename_component(_name "${CMAKE_MATCH_2}" NAME_WE)
                if (NOT _name STREQUAL "qore" AND NOT _name STREQUAL "${_mod}")
                    list(APPEND _reqs "${_name}")
                endif()
            endif()
        endforeach()
    endforeach()
    if (_reqs)
        list(REMOVE_DUPLICATES _reqs)
    endif()
    set(${_out_var} "${_reqs}" PARENT_SCOPE)
endfunction()

function(QORE_FINALIZE_USER_MODULE_DEPENDENCIES)
    get_property(_qore_user_modules GLOBAL PROPERTY QORE_USER_MODULE_TARGETS)
    foreach(_mod ${_qore_user_modules})
        # Build-order dependencies are derived solely from the module's own
        # %requires directives -- the single source of truth.  A module's AOT
        # compile loads exactly what it %requires (directly or transitively), so
        # these edges are necessary and sufficient to avoid compiling against a
        # stale dependency .qmod; per-module transitivity covers indirect deps.
        _qore_parse_module_requires(${_mod} _all_deps)
        if (NOT _all_deps)
            continue()
        endif()

        set(_seen "")
        foreach(_dep ${_all_deps})
            get_filename_component(_dep_name ${_dep} NAME_WE)
            list(FIND _seen "${_dep_name}" _si)
            if (NOT _si EQUAL -1)
                continue()
            endif()
            list(APPEND _seen "${_dep_name}")

            if (TARGET docs-${_mod} AND TARGET docs-${_dep_name})
                add_dependencies(docs-${_mod} docs-${_dep_name})
            endif()

            if (TARGET ${_mod}-qmod)
                if (TARGET ${_dep_name})
                    # Only wire a build-order edge to a sibling qore module target, not to an
                    # unrelated third-party target that happens to share the dependency's name
                    # (e.g. nghttp2's "json" FetchContent target collides with the external qore
                    # "json" module).  Qore module targets live in the source tree; FetchContent
                    # targets live under a "/_deps/" build directory -- use SOURCE_DIR to tell
                    # them apart.
                    get_target_property(_dep_src_dir ${_dep_name} SOURCE_DIR)
                    if (NOT _dep_src_dir MATCHES "/_deps/")
                        add_dependencies(${_mod}-qmod ${_dep_name})
                    endif()
                    unset(_dep_src_dir)
                endif()
                if (TARGET ${_dep_name}-qmod)
                    add_dependencies(${_mod}-qmod ${_dep_name}-qmod)
                endif()
            endif()
        endforeach()
    endforeach()
endfunction()

# Install qore native/user module (.qm file) into proper location.
#
# NOTE: must be called afer QORE_EXTERNAL_BINARY_MODULE
#
# Param #1: path to the module; e.g. "qlib/RestHandler.qm"
# Param #2: list of module dependencies separated by semicolon; e.g. "HttpServerUtil;Mime;Util"
#
# Example:
#     qore_module_user_module("qlib/RestHandler.qm" "HttpServerUtil;Mime;Util")
#
# The module will be installed automatically in 'make install' target.
MACRO (QORE_EXTERNAL_USER_MODULE _module_file _mod_deps)
    get_filename_component(f ${_module_file} NAME_WE)
    unset(_mod_targets)
    unset(_mod_jar_targets)
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
        file(GLOB _mod_targets "${CMAKE_SOURCE_DIR}/qlib/${f}/*.qm" "${CMAKE_SOURCE_DIR}/qlib/${f}/*.qc"
            "${CMAKE_SOURCE_DIR}/qlib/${f}/*.yaml" "${CMAKE_SOURCE_DIR}/qlib/${f}/*.svg"
            "${CMAKE_SOURCE_DIR}/qlib/${f}/*.proto")
        file(GLOB _mod_jar_targets "${CMAKE_SOURCE_DIR}/qlib/${f}/jar/*.jar")
        set(qm_install_subdir "${f}") # install files into a subdir
        #message(STATUS "_mod_targets ${_mod_targets}")
    else()
        set(_mod_targets ${_module_file})
        set(qm_install_subdir "") # common qm file
    endif()

    if (DOXYGEN_FOUND)
        # get module name
        #message(STATUS "Preparing generation of documentation for module: ${f}")

        # prepare directories for the documentation
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/doxygen/qlib/${f})

        # prepare needed vars
        set(MOD_DOXYFILE "${CMAKE_BINARY_DIR}/doxygen/Doxyfile.${f}")
        #set(MOD_DEPS -t${_external_module_name}.tag=../../${_external_module_name}/html)
        set(CURRENT_MODULE_NAME ${f})
        set(TAGFILES ${CMAKE_BINARY_DIR}/${_external_module_name}.tag=../../${_external_module_name}/html)
        foreach(i ${_mod_deps})
            # we must use relative directories for tags; using absolute paths for tags will break the documentation
            # when used on any system except the one where it's generated
            #SET(MOD_DEPS ${MOD_DEPS} -t${i}.tag=../../${i}/html)
            SET(TAGFILES ${TAGFILES} ${CMAKE_BINARY_DIR}/${i}.tag=../../${i}/html)
        endforeach(i)
        string (REPLACE ";" " " TAGFILES "${TAGFILES}")

        set(_dox_output ${CMAKE_BINARY_DIR}/docs/${f})
        #set(_dox_input ${CMAKE_BINARY_DIR}/doxygen/qlib/${f}.qm.dox.h)
        set(_dox_input "")
        foreach(fn0 ${_mod_targets})
            get_filename_component(fn1 ${fn0} NAME)
            set(_dox_input ${_dox_input} ${CMAKE_BINARY_DIR}/doxygen/qlib/${f}/${fn1}.dox.h)
        endforeach(fn0)
        string(REPLACE ";" " " _dox_input "${_dox_input}")

        # prepare QDX arguments - process .qm file
        configure_file(${QORE_USERMODULE_DOXYGEN_TEMPLATE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.${f} @ONLY)
        set(QDX_QMDOXH_ARGS ${CMAKE_SOURCE_DIR}/${_module_file} ${CMAKE_BINARY_DIR}/doxygen/qlib/${f}/${f}.qm.dox.h)

        # Build list of qdx commands for all .qc files
        set(_qdx_qc_commands "")
        foreach(fn0 ${_mod_targets})
            get_filename_component(fn1 ${fn0} NAME)
            get_filename_component(fn_ext ${fn0} EXT)
            if("${fn_ext}" STREQUAL ".qc")
                list(APPEND _qdx_qc_commands COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${fn0} ${CMAKE_BINARY_DIR}/doxygen/qlib/${f}/${fn1}.dox.h)
            endif()
        endforeach(fn0)

        # add CMake target for the documentation
        set(QORE_QMOD_FNAME ${f}) # used for configure_file line below

        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/docs/${f}/${qm_install_subdir}/)
        add_custom_target(docs-${f}
            # Process the .qm file
            COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} ${QDX_QMDOXH_ARGS}
            # Process all .qc files
            ${_qdx_qc_commands}
            # Run doxygen
            COMMAND ${DOXYGEN_EXECUTABLE} ${MOD_DOXYFILE}
            COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/${f}/html/*.html
            COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/${f}/html/search/*.html

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen for: ${f}" VERBATIM
        )
        if (TARGET qore)
            add_dependencies(docs-${f} qore)
        endif ()

        # make 'docs' target dependent on this module documentation
        add_dependencies(docs docs-${f})

        # make this dependent on the other module targets
        add_dependencies(docs-${f} docs-module)
        foreach(i ${_mod_deps})
            add_dependencies(docs-${f} docs-${i})
        endforeach(i)
    endif (DOXYGEN_FOUND)

    # install user module files
    install(FILES ${_mod_targets}
        DESTINATION ${QORE_USER_MODULES_DIR}/${qm_install_subdir}
        COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
    if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
        QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL(${f} 1)
    else()
        QORE_AOT_REMOVE_INSTALLED_QMODS_FOR_SOURCE_INSTALL(${f} 0)
    endif()
    if (_mod_jar_targets)
        install(FILES ${_mod_jar_targets}
            DESTINATION ${QORE_USER_MODULES_DIR}/${qm_install_subdir}/jar
            COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
        message(STATUS "called install for ${_mod_jar_targets} -> ${QORE_USER_MODULES_DIR}/${qm_install_subdir}/jar")
    endif()

    # extract and install QM metadata (automatic for external user modules)
    if(DEFINED QORE_QM_METADATA_EXECUTABLE AND DEFINED QORE_METADATA_DIR
            AND DEFINED _external_module_name)
        if(DEFINED QORE_EXECUTABLE)
            set(_qore_qm_metadata_command
                ${QORE_EXECUTABLE} ${QORE_QM_METADATA_EXECUTABLE})
        else()
            set(_qore_qm_metadata_command ${QORE_QM_METADATA_EXECUTABLE})
        endif()
        set(_ext_qm_meta_files "")
        # use module-specific subdirectory to avoid collisions when
        # different modules contain .qc files with the same basename
        set(_ext_meta_dir ${CMAKE_CURRENT_BINARY_DIR}/qm-metadata/${f})
        file(MAKE_DIRECTORY ${_ext_meta_dir})
        foreach(_src_file ${_mod_targets})
            get_filename_component(_src_ext ${_src_file} EXT)
            if("${_src_ext}" STREQUAL ".qm" OR "${_src_ext}" STREQUAL ".qc")
                # resolve relative paths to absolute
                if(NOT IS_ABSOLUTE "${_src_file}")
                    set(_src_file "${CMAKE_SOURCE_DIR}/${_src_file}")
                endif()
                get_filename_component(_src_name ${_src_file} NAME)
                set(_meta_out ${_ext_meta_dir}/${_src_name}.meta.json)
                if(DEFINED QORE_QM_METADATA_ENV)
                    add_custom_command(OUTPUT ${_meta_out}
                        COMMAND ${CMAKE_COMMAND} -E env
                            ${QORE_QM_METADATA_ENV}
                            ${_qore_qm_metadata_command}
                            ${_src_file} ${_meta_out}
                        DEPENDS ${_src_file}
                        COMMENT "Extracting metadata from ${_src_name}"
                        VERBATIM
                    )
                else()
                    add_custom_command(OUTPUT ${_meta_out}
                        COMMAND ${_qore_qm_metadata_command}
                            ${_src_file} ${_meta_out}
                        DEPENDS ${_src_file}
                        COMMENT "Extracting metadata from ${_src_name}"
                        VERBATIM
                    )
                endif()
                list(APPEND _ext_qm_meta_files ${_meta_out})
            endif()
        endforeach()
        if(_ext_qm_meta_files)
            if(NOT TARGET ${_external_module_name}-qm-metadata)
                add_custom_target(${_external_module_name}-qm-metadata ALL)
            endif()
            add_custom_target(qm-metadata-${f} ALL
                DEPENDS ${_ext_qm_meta_files})
            set_property(GLOBAL APPEND PROPERTY
                QORE_QM_METADATA_TARGETS qm-metadata-${f})
            add_dependencies(${_external_module_name}-qm-metadata
                qm-metadata-${f})
            if(QORE_QM_METADATA_DEPENDS)
                add_dependencies(qm-metadata-${f}
                    ${QORE_QM_METADATA_DEPENDS})
            endif()
            install(FILES ${_ext_qm_meta_files}
                DESTINATION ${QORE_METADATA_DIR}/${_external_module_name}
                COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
        endif()
    endif()

    # AOT: build a .qmod alongside the .qm source when enabled
    if (QORE_BUILD_AOT_MODULES AND (TARGET qcc OR DEFINED QORE_QCC_EXECUTABLE))
        if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/qlib/${f})
            QORE_USER_MODULE_AOT_RULES(${f} 1
                ${CMAKE_SOURCE_DIR}/qlib/${f} ${_mod_targets})
        else()
            QORE_USER_MODULE_AOT_RULES(${f} 0
                ${CMAKE_SOURCE_DIR}/${_module_file} ${_mod_targets})
        endif()
        foreach(_dep ${_mod_deps})
            get_filename_component(_dep_name ${_dep} NAME_WE)
            if (TARGET ${_dep_name})
                add_dependencies(${f}-qmod ${_dep_name})
            endif()
            if (TARGET ${_dep_name}-qmod)
                add_dependencies(${f}-qmod ${_dep_name}-qmod)
            endif()
        endforeach()
        get_property(_qore_external_user_modules GLOBAL PROPERTY QORE_EXTERNAL_USER_MODULE_TARGETS)
        foreach(_existing_mod ${_qore_external_user_modules})
            get_property(_existing_deps GLOBAL PROPERTY QORE_EXTERNAL_USER_MODULE_DEPS_${_existing_mod})
            foreach(_dep ${_existing_deps})
                get_filename_component(_dep_name ${_dep} NAME_WE)
                if ("${_dep_name}" STREQUAL "${f}" AND TARGET ${_existing_mod}-qmod)
                    add_dependencies(${_existing_mod}-qmod ${f}-qmod)
                endif()
            endforeach()
        endforeach()
        set_property(GLOBAL APPEND PROPERTY QORE_EXTERNAL_USER_MODULE_TARGETS ${f})
        set_property(GLOBAL PROPERTY QORE_EXTERNAL_USER_MODULE_DEPS_${f} "${_mod_deps}")
    endif()
ENDMACRO (QORE_EXTERNAL_USER_MODULE)

# Configure a two-phase documentation build for an external binary module that
# contains one or more user sub-modules, so that the binary module's mainpage
# and other doc pages can cross-reference symbols in the user module docs via
# @ref.
#
# Must be called AFTER qore_external_binary_module() and all
# qore_external_user_module() calls for the sub-modules.
#
# The binary module docs are laid out as ${CMAKE_BINARY_DIR}/docs/${binary}/html/
# and user module docs as ${CMAKE_BINARY_DIR}/docs/${usermod}/html/, so the
# correct binary-to-user relative path is "../../${usermod}/html" (two levels
# of ".." to escape the binary module's html subdirectory to the shared docs/
# parent).
#
# Param #1: binary module name (e.g. "krb5")
# Param #2: list of user module names separated by semicolons
#           (e.g. "Krb5Util" or "NcursesUi;NcursesReplUi")
#
# Example:
#     qore_external_binary_module(krb5 ${PROJECT_VERSION})
#     qore_external_user_module("qlib/Krb5Util" "")
#     qore_binary_module_two_phase_docs(krb5 "Krb5Util")
MACRO (QORE_BINARY_MODULE_TWO_PHASE_DOCS _binary_module _user_modules)
    if (DOXYGEN_FOUND)
        # Suppress unresolved cross-reference warnings on the initial pass,
        # which runs before any user module tag files exist.
        file(APPEND ${CMAKE_BINARY_DIR}/Doxyfile
            "\n# Suppress warnings for initial pass (no user module TAGFILES available)\nWARN_IF_DOC_ERROR = NO\n")

        # Build the TAGFILES line that pulls in every user module's tag file.
        # Binary HTML is at docs/${binary}/html/; user HTML is at docs/${usermod}/html/;
        # from the former, the correct relative path is ../../${usermod}/html.
        set(_qb2pd_tagfiles "")
        foreach(_qb2pd_um ${_user_modules})
            if (_qb2pd_tagfiles STREQUAL "")
                set(_qb2pd_tagfiles "${CMAKE_BINARY_DIR}/${_qb2pd_um}.tag=../../${_qb2pd_um}/html")
            else()
                set(_qb2pd_tagfiles "${_qb2pd_tagfiles} ${CMAKE_BINARY_DIR}/${_qb2pd_um}.tag=../../${_qb2pd_um}/html")
            endif()
        endforeach()

        # Create the final-pass Doxyfile by copying the initial one and appending
        # the correct TAGFILES line plus re-enabling doc error warnings.
        file(COPY_FILE ${CMAKE_BINARY_DIR}/Doxyfile ${CMAKE_BINARY_DIR}/Doxyfile.final)
        file(APPEND ${CMAKE_BINARY_DIR}/Doxyfile.final
            "\n# Final pass: enable user module cross-references and re-enable doc error warnings\nTAGFILES = ${_qb2pd_tagfiles}\nWARN_IF_DOC_ERROR = YES\n")

        add_custom_target(docs-module-final
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile.final
            COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/${_binary_module}/html ${CMAKE_BINARY_DIR}/docs/${_binary_module}/html/search
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen (final pass with user module cross-references)"
            VERBATIM
        )
        foreach(_qb2pd_um ${_user_modules})
            add_dependencies(docs-module-final docs-${_qb2pd_um})
        endforeach()
        add_dependencies(docs docs-module-final)
    endif()
ENDMACRO (QORE_BINARY_MODULE_TWO_PHASE_DOCS)

# Install qore native/user modules (qm files) into the proper location.
# Example:
#   set(QM_FILES foo.qm bar.qm)
#   qore_user_modules(${QM_FILES})
# Files will be installed automatically in 'make install' target
MACRO (QORE_USER_MODULES _inputs)
    # first - handle qlib documentation
    if (NOT DOXYGEN_FOUND)
        find_package(Doxygen)
    endif()
    foreach(f ${_inputs})
        unset(_mod_targets)
        if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/${f})
            if (${f} MATCHES "^qlib/")
                string(LENGTH "${f}" f_len)
                MATH(EXPR f_len "${f_len}-5")
                string(SUBSTRING ${f} 5 ${f_len} new_f)

                file(GLOB _mod_targets "${CMAKE_SOURCE_DIR}/${f}/*.qm" "${CMAKE_SOURCE_DIR}/${f}/*.qc"
                    "${CMAKE_SOURCE_DIR}/${f}/*.yaml" "${CMAKE_SOURCE_DIR}/qlib/${f}/*.svg"
                    "${CMAKE_SOURCE_DIR}/${f}/*.proto")
                set(qm_install_subdir "${new_f}") # install files into a subdir
                #message(STATUS "_mod_targets ${_mod_targets}")
            else()
                file(GLOB _mod_targets "${CMAKE_SOURCE_DIR}/${f}/*.qm" "${CMAKE_SOURCE_DIR}/${f}/*.qc")
                set(qm_install_subdir "${f}") # install files into a subdir
                #message(STATUS "_mod_targets ${_mod_targets}")
            endif()
        else()
            set(_mod_targets ${f})
            set(qm_install_subdir "") # common qm file
        endif()

        if (xDOXYGEN_FOUND)
            get_filename_component(file ${f} NAME_WE)
            #message(STATUS "Doxyfile for ${file}")
            set(CURRENT_MODULE_NAME ${_file}) # used for configure_file line below
            set(_dox_input ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${MODULE_DOX_INPUT})
            configure_file(${QORE_USERMODULE_DOXYGEN_TEMPLATE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.${file} @ONLY)
            file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/docs/${file}/${qm_install_subdir}/)
            add_custom_target(docs-${file}
                ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.${file}
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/html/*.html
                COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/html/search/*.html
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen for: ${file}" VERBATIM
            )
            if (TARGET qore)
                add_dependencies(docs-${file} qore)
            endif ()
            add_dependencies(docs docs-${file})
        endif (xDOXYGEN_FOUND)
        # install qm files
        install(FILES ${_mod_targets}
            DESTINATION ${QORE_USER_MODULES_DIR}/${qm_install_subdir}
            COMPONENT ${QORE_QM_SOURCE_INSTALL_COMPONENT})
    endforeach(f)
ENDMACRO (QORE_USER_MODULES)


# Make distributable (source code) tarball. Target 'make dist'
MACRO (QORE_DIST _version)
    # packaging related stuff
    string(TOLOWER ${CMAKE_PROJECT_NAME} CPACK_PACKAGE_NAME)
    SET(CPACK_PACKAGE_VERSION "${_version}")
    SET(CPACK_SOURCE_GENERATOR "TBZ2")
    SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
    SET(CPACK_IGNORE_FILES "/CVS/;/#;~$;__pycache__;[^_a-zA-Z]core\\\\.(:?!\\\\W+(h|hpp|c|cpp)\\\\b);qorusproject\\\\.json;\\\\.tar.gz$;/CMakeFiles/;CMakeCache.txt;refresh-copyright-and-license.pl;/\\\\..*;/build.*/")
    SET(CPACK_SOURCE_IGNORE_FILES ${CPACK_IGNORE_FILES})
    INCLUDE(CPack)
    # simulate autotools' "make dist"
    add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

    message(STATUS "")
    message(STATUS "Tarball creation target: make dist")
    message(STATUS "")

ENDMACRO (QORE_DIST)

# prints a summary of configuration
MACRO (QORE_CONFIG_INFO)
    MESSAGE(STATUS "")
    MESSAGE(STATUS "-----------------------------")
    MESSAGE(STATUS "System: ${CMAKE_SYSTEM}")
    MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    MESSAGE(STATUS "Prefix: ${CMAKE_INSTALL_PREFIX}")
    IF (APPLE)
        MESSAGE(STATUS "Archs: ${CMAKE_OSX_ARCHITECTURES}")
    ENDIF (APPLE)
    MESSAGE(STATUS "C Compiler: ${CMAKE_C_COMPILER} - ${CMAKE_C_COMPILER_ID} - ${CMAKE_C_COMPILER_VERSION}")
    MESSAGE(STATUS "C Flags: ${CMAKE_C_FLAGS}")
    MESSAGE(STATUS "C Flags debug: ${CMAKE_C_FLAGS_DEBUG}")
    MESSAGE(STATUS "C Flags release: ${CMAKE_C_FLAGS_RELEASE}")
    MESSAGE(STATUS "C Flags reldeb: ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    MESSAGE(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER} - ${CMAKE_CXX_COMPILER_ID} - ${CMAKE_CXX_COMPILER_VERSION}")
    MESSAGE(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
    MESSAGE(STATUS "CXX Flags debug: ${CMAKE_CXX_FLAGS_DEBUG}")
    MESSAGE(STATUS "CXX Flags release: ${CMAKE_CXX_FLAGS_RELEASE}")
    MESSAGE(STATUS "CXX Flags reldeb: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    MESSAGE(STATUS "-----------------------------")
    MESSAGE(STATUS "")
ENDMACRO (QORE_CONFIG_INFO)

#
# Find Pthreads.
# Uses FindThreads with pthreads preference to look for pthreads.
# Cmake generation will break if pthreads are not found.
# For variables and targets defined by this, see docs for FindThreads.
#
macro(QORE_FIND_PTHREADS)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
    message(STATUS "Found POSIX Threads: TRUE")
else(CMAKE_USE_PTHREADS_INIT)
    message(STATUS "Found POSIX Threads: FALSE")
    message(FATAL_ERROR "POSIX threads do not seem to be supported on this platform, aborting")
endif()
endmacro(QORE_FIND_PTHREADS)

# find pthread_setname
macro(QORE_FIND_PTHREAD_SETNAME_NP)
  message(STATUS "Looking for pthread_setname_np()")

  check_cxx_source_compiles("
#include <pthread.h>
int main(int argc, char* argv []) {
    pthread_setname_np(\"foo\");
    return 0;
}
" QORE_HAVE_PTHREAD_SETNAME_NP_1)

  if (NOT QORE_HAVE_PTHREAD_SETNAME_NP_1)
    check_cxx_source_compiles("
#include <pthread.h>
int main(int argc, char* argv []) {
    pthread_setname_np(pthread_self(), \"foo\");
    return 0;
}
" QORE_HAVE_PTHREAD_SETNAME_NP_2)

    if (NOT QORE_HAVE_PTHREAD_SETNAME_NP_2)
      check_cxx_source_compiles("
#include <pthread.h>
int main(int argc, char* argv []) {
    pthread_setname_np(pthread_self(), \"foo\", (void*)0);
    return 0;
}
" QORE_HAVE_PTHREAD_SETNAME_NP_3)

      if (NOT QORE_HAVE_PTHREAD_SETNAME_NP_3)
        check_cxx_source_compiles("
#include <pthread.h>
int main(int argc, char* argv []) {
    pthread_set_name_np(pthread_self(), \"foo\");
    return 0;
}
" QORE_HAVE_PTHREAD_SET_NAME_NP)

      endif()

    endif()

  endif()

endmacro(QORE_FIND_PTHREAD_SETNAME_NP)

# find pthread_getattr_np
macro(QORE_FIND_PTHREAD_GETATTR_NP)
  check_cxx_source_compiles("
#include <pthread.h>
int main(int argc, char* argv []) {
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    return 0;
}
" QORE_HAVE_PTHREAD_GETATTR_NP)
endmacro()

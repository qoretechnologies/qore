cmake_minimum_required(VERSION 3.14)

# NOTE: Qore should always use semantic versioning: https://semver.org/
set(VERSION_MAJOR 3)
set(VERSION_MINOR 0)
set(VERSION_SUB 0)
#set(VERSION_PATCH 0)

# set policy for interperting unquoted variables
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
# support <pkg>_ROOT in find_package
if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()
# enable RPATH in macos bins (OS requires this in any case)
if (POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()

project(qore-language)

set(QORE_FETCHCONTENT_URL_OPTIONS)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND QORE_FETCHCONTENT_URL_OPTIONS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

set(QORE_FETCHCONTENT_EXCLUDE_FROM_ALL)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    list(APPEND QORE_FETCHCONTENT_EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
endif()

# On macOS, auto-detect package manager prefixes for find_package() calls.
# MacPorts (/opt/local) is preferred; Homebrew is added after.
if (APPLE)
    if (IS_DIRECTORY /opt/local)
        list(APPEND CMAKE_PREFIX_PATH /opt/local)
    endif()
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE _brew_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _brew_result
    )
    if (_brew_result EQUAL 0 AND _brew_prefix)
        list(APPEND CMAKE_PREFIX_PATH "${_brew_prefix}")
        # ICU is keg-only in Homebrew, so add its prefix explicitly for
        # find_package(ICU) and pkg-config based discovery.
        execute_process(
            COMMAND brew --prefix icu4c
            OUTPUT_VARIABLE _brew_icu_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _brew_icu_result
        )
        if (_brew_icu_result EQUAL 0 AND _brew_icu_prefix)
            list(APPEND CMAKE_PREFIX_PATH "${_brew_icu_prefix}")
        endif()
    endif()
endif()

option(ICONV_TRANSLIT
       "Force //Translit to iconv encoding to do transliteration, if OFF it is tested for"
       OFF)

option(SINGLE_COMPILATION_UNIT
       "Build libqore as a single compilation unit for faster builds"
       ON)

option(ENABLE_ALLOC_TRACKING
       "Enable malloc/free interposer for memory leak tracking (adds overhead to all allocations; debug only)"
       OFF)

option(QORE_OPTIMIZE_DEBUG_BUILDS
       "Use debug-friendly compiler optimization for debug builds"
       ON)

option(QORE_BUILD_AOT_MODULES
       "Build AOT-compiled .qmod artifacts alongside each user module via qcc"
       ON)

option(QORE_INSTALL_DEPENDS_ON_ALL
       "Make the install target build the all target before installing"
       OFF)
if (NOT QORE_INSTALL_DEPENDS_ON_ALL)
    set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
endif()

STRING(LENGTH ${VERSION_MINOR} MINOR_LEN)
if (${MINOR_LEN} EQUAL 1)
    set(VERSION_FORMATTED_MINOR 0${VERSION_MINOR})
else()
    set(VERSION_FORMATTED_MINOR ${VERSION_MINOR})
endif()
STRING(LENGTH ${VERSION_SUB} SUB_LEN)
if (${SUB_LEN} EQUAL 1)
    set(VERSION_FORMATTED_SUB 0${VERSION_SUB})
else()
    set(VERSION_FORMATTED_SUB ${VERSION_SUB})
endif()

if(DEFINED VERSION_PATCH)
    set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB}.${VERSION_PATCH})
else()
    set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB})
    set(VERSION_PATCH 0)
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "release")
endif (NOT CMAKE_BUILD_TYPE)
string(TOLOWER ${CMAKE_BUILD_TYPE} QORE_BUILD_TYPE_LWR)
if (${QORE_BUILD_TYPE_LWR} MATCHES "debug")
    add_definitions(-DDEBUG)
    if (QORE_OPTIMIZE_DEBUG_BUILDS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")
    endif ()
else ()
    add_definitions(-DNDEBUG)
endif ()

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_BINARY_DIR}/include/)
include_directories(BEFORE ${CMAKE_BINARY_DIR})

# for add the source dir include for cmake runs that refuse to add it in some environments for an unknown reason
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_SOURCE_DIR}/include")

# NaN-boxing in QoreValue stores doubles, integers, and pointers in a single uint64_t field using
# bit-pattern manipulation. GCC/Clang strict aliasing optimizations can miscompile reads and writes
# through QoreValue pointers, causing incorrect behavior (e.g., member variable increments by 2
# instead of 1). This flag is required for correctness with NaN-boxing type-punning.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
endif()

find_package(Java)

if (DEFINED ENV{DOXYGEN_EXECUTABLE})
    set(DOXYGEN_EXECUTABLE $ENV{DOXYGEN_EXECUTABLE})
endif()

# simulate QoreConfig
set(QORE_QDX_EXECUTABLE "${CMAKE_SOURCE_DIR}/doxygen/qdx")
set(QORE_QJAR_EXECUTABLE "${CMAKE_SOURCE_DIR}/doxygen/qjar")
set(QORE_INTERPRETER "$<TARGET_FILE:qore>")
set(QORE_BINARY_MODULE_DIRS "")
file(GLOB QORE_BINARY_MODULE_SOURCE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/modules/*")
foreach(mod_dir ${QORE_BINARY_MODULE_SOURCE_DIRS})
    if (IS_DIRECTORY ${mod_dir})
        get_filename_component(mod_name ${mod_dir} NAME)
        list(APPEND QORE_BINARY_MODULE_DIRS "${CMAKE_BINARY_DIR}/modules/${mod_name}")
    endif ()
endforeach ()
if (WIN32 OR MSYS OR MINGW)
    set(QORE_MODULE_DIR_FOR_DOCS "${CMAKE_SOURCE_DIR}/qlib")
    list(APPEND QORE_MODULE_DIR_FOR_DOCS ${QORE_BINARY_MODULE_DIRS})
    string(REPLACE ";" "\\;" QORE_MODULE_DIR_FOR_DOCS "${QORE_MODULE_DIR_FOR_DOCS}")
else (WIN32 OR MSYS OR MINGW)
    set(QORE_MODULE_DIR_FOR_DOCS "${CMAKE_SOURCE_DIR}/qlib")
    list(APPEND QORE_MODULE_DIR_FOR_DOCS ${QORE_BINARY_MODULE_DIRS})
    string(REPLACE ";" ":" QORE_MODULE_DIR_FOR_DOCS "${QORE_MODULE_DIR_FOR_DOCS}")
endif (WIN32 OR MSYS OR MINGW)
set(QORE_DOC_DEFINES_LIST QORE_QDX_RUN)
if (WIN32 OR MSYS OR MINGW)
    list(APPEND QORE_DOC_DEFINES_LIST Windows)
else (WIN32 OR MSYS OR MINGW)
    list(APPEND QORE_DOC_DEFINES_LIST Unix)
endif (WIN32 OR MSYS OR MINGW)
if (${QORE_BUILD_TYPE_LWR} MATCHES "debug")
    list(APPEND QORE_DOC_DEFINES_LIST QoreDebug)
endif ()
if (DEFINED HAVE_TERMIOS_H AND HAVE_TERMIOS_H)
    list(APPEND QORE_DOC_DEFINES_LIST HAVE_TERMIOS)
endif ()
string(REPLACE ";" "," QORE_DOC_DEFINES "${QORE_DOC_DEFINES_LIST}")
if (DEFINED ENV{QORE_DOC_DEFINES})
    set(_QORE_DOC_DEFINES_ENV "$ENV{QORE_DOC_DEFINES}")
    string(REGEX REPLACE "[ \t\r\n]+" "," _QORE_DOC_DEFINES_ENV "${_QORE_DOC_DEFINES_ENV}")
    if (QORE_DOC_DEFINES AND _QORE_DOC_DEFINES_ENV)
        set(QORE_DOC_DEFINES "${QORE_DOC_DEFINES},${_QORE_DOC_DEFINES_ENV}")
    elseif (_QORE_DOC_DEFINES_ENV)
        set(QORE_DOC_DEFINES "${_QORE_DOC_DEFINES_ENV}")
    endif ()
endif ()
set(QORE_DOCS_ENV QORE_MODULE_DIR=${QORE_MODULE_DIR_FOR_DOCS} QORE_DOC_DEFINES=${QORE_DOC_DEFINES})
set(QORE_QDX_COMMAND ${QORE_INTERPRETER} ${QORE_QDX_EXECUTABLE})
set(QORE_QJAR_COMMAND ${QORE_INTERPRETER} ${QORE_QJAR_EXECUTABLE})
# simulate QoreConfig

include("${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/QoreMacrosIntern.cmake")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CheckIncludeFiles)
include(GNUInstallDirs)

set(QORE_INSTALL_COMPONENT_BOOTSTRAP qore-bootstrap)
set(QORE_INSTALL_COMPONENT_QLIB_AOT qore-qlib-aot)
set(QORE_BINARY_MODULE_INSTALL_COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
set(QORE_QM_SOURCE_INSTALL_COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
set(QORE_QMOD_INSTALL_COMPONENT ${QORE_INSTALL_COMPONENT_QLIB_AOT})

# issue #2994: ensure that the arch-specific libdir is used on Debian/Ubuntu
check_libdir_arch()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(myprefix ${CMAKE_INSTALL_PREFIX})
set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})

if (WIN32 OR MSYS OR MINGW)
    set(MODULE_DIR lib/qore-modules)
    set(MODULE_VER_DIR ${MODULE_DIR}/${VERSION})
    set(USER_MODULE_DIR share/qore-modules)
    set(USER_MODULE_VER_DIR ${USER_MODULE_DIR}/${VERSION})
    add_definitions(-DMODULES_RELATIVE_PATH)
else (WIN32 OR MSYS or MINGW)
    set(MODULE_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/qore-modules)
    set(MODULE_VER_DIR ${MODULE_DIR}/${VERSION})
    set(USER_MODULE_DIR ${CMAKE_INSTALL_FULL_DATADIR}/qore-modules/)
    set(USER_MODULE_VER_DIR ${USER_MODULE_DIR}${VERSION})
endif (WIN32 OR MSYS OR MINGW)

get_module_api_versions()

set(LIBQORE_QPP_SRC
    lib/QC_AbstractSmartLock.qpp
    lib/QC_AbstractThreadResource.qpp
    lib/QC_AutoGate.qpp
    lib/QC_AutoLock.qpp
    lib/QC_AutoReadLock.qpp
    lib/QC_AutoWriteLock.qpp
    lib/QC_Condition.qpp
    lib/QC_Counter.qpp
    lib/QC_Channel.qpp
    lib/QC_ChannelIterator.qpp
    lib/QC_Future.qpp
    lib/QC_FutureImpl.qpp
    lib/QC_Promise.qpp
    lib/QC_Semaphore.qpp
    lib/QC_AutoSemaphore.qpp
    lib/QC_WaitGroup.qpp
    lib/QC_AbstractIterator.qpp
    lib/QC_AbstractQuantifiedIterator.qpp
    lib/QC_AbstractBidirectionalIterator.qpp
    lib/QC_AbstractQuantifiedBidirectionalIterator.qpp
    lib/QC_ListIterator.qpp
    lib/QC_ListReverseIterator.qpp
    lib/QC_HashIterator.qpp
    lib/QC_HashReverseIterator.qpp
    lib/QC_HashKeyIterator.qpp
    lib/QC_HashKeyReverseIterator.qpp
    lib/QC_HashPairIterator.qpp
    lib/QC_HashPairReverseIterator.qpp
    lib/QC_ObjectIterator.qpp
    lib/QC_ObjectReverseIterator.qpp
    lib/QC_ObjectKeyIterator.qpp
    lib/QC_ObjectKeyReverseIterator.qpp
    lib/QC_ObjectPairIterator.qpp
    lib/QC_ObjectPairReverseIterator.qpp
    lib/QC_HashListIterator.qpp
    lib/QC_HashListReverseIterator.qpp
    lib/QC_ListHashIterator.qpp
    lib/QC_ListHashReverseIterator.qpp
    lib/QC_AbstractLineIterator.qpp
    lib/QC_FileLineIterator.qpp
    lib/QC_DataLineIterator.qpp
    lib/QC_InputStreamLineIterator.qpp
    lib/QC_SingleValueIterator.qpp
    lib/QC_AbstractDatasource.qpp
    lib/QC_AbstractSQLStatement.qpp
    lib/QC_ColumnarResult.qpp
    lib/QC_RangeIterator.qpp
    lib/QC_StringSplitIterator.qpp
    lib/QC_StringRegexSplitIterator.qpp
    lib/QC_RegexMatchIterator.qpp
    lib/QC_StringCharIterator.qpp
    lib/QC_Scanner.qpp
    lib/QC_Datasource.qpp
    lib/QC_DatasourcePool.qpp
    lib/QC_SqlMutationDeclaration.qpp
    lib/QC_AbstractConnectionPool.qpp
    lib/QC_Dir.qpp
    lib/QC_ReadOnlyFile.qpp
    lib/QC_File.qpp
    lib/QC_FtpClient.qpp
    lib/QC_Gate.qpp
    lib/QC_GetOpt.qpp
    lib/QC_HTTPClient.qpp
    lib/QC_Mutex.qpp
    lib/QC_Program.qpp
    lib/QC_ProgramControl.qpp
    lib/QC_DebugProgram.qpp
    lib/QC_Breakpoint.qpp
    lib/QC_Expression.qpp
    lib/QC_Regex.qpp
    lib/QC_RegexSubst.qpp
    lib/QC_RegexExtract.qpp
    lib/QC_Queue.qpp
    lib/QC_RWLock.qpp
    lib/QC_SandboxManager.qpp
    lib/QC_SQLStatement.qpp
    lib/QC_Sequence.qpp
    lib/QC_Socket.qpp
    lib/QC_TermIOS.qpp
    lib/QC_TimeZone.qpp
    lib/QC_TreeMap.qpp
    lib/QC_QoreParseOptions.qpp
    lib/QC_SSLCertificate.qpp
    lib/QC_SSLPrivateKey.qpp
    lib/QC_ThreadPool.qpp
    lib/QC_StreamBase.qpp
    lib/QC_InputStream.qpp
    lib/QC_BinaryInputStream.qpp
    lib/QC_StringInputStream.qpp
    lib/QC_FileInputStream.qpp
    lib/QC_EncodingConversionInputStream.qpp
    lib/QC_OutputStream.qpp
    lib/QC_BinaryOutputStream.qpp
    lib/QC_StringOutputStream.qpp
    lib/QC_FileOutputStream.qpp
    lib/QC_EncodingConversionOutputStream.qpp
    lib/QC_StreamPipe.qpp
    lib/QC_PipeInputStream.qpp
    lib/QC_PipeOutputStream.qpp
    lib/QC_StreamWriter.qpp
    lib/QC_StreamReader.qpp
    lib/QC_BufferedStreamReader.qpp
    lib/QC_Transform.qpp
    lib/QC_TransformInputStream.qpp
    lib/QC_TransformOutputStream.qpp
    lib/QC_StdoutOutputStream.qpp
    lib/QC_StderrOutputStream.qpp
    lib/QC_Serializable.qpp
    lib/QC_AbstractPollableIoObject.qpp
    lib/QC_AbstractPollableIoObjectBase.qpp
    lib/QC_AbstractPollOperation.qpp
    lib/QC_SocketPollOperationBase.qpp
    lib/QC_SocketPollOperation.qpp
    lib/QC_FilePollOperation.qpp
    lib/QC_DelegatingPollOperation.qpp
    lib/QC_FtpControlPollOperation.qpp
    lib/QC_FtpDataPollOperation.qpp
    lib/QC_AbstractHttpPollConnection.qpp
    lib/QC_HttpClientConnectionBase.qpp
    lib/QC_HttpClientConnectionManagerBase.qpp
    lib/QC_HttpIdlePollOperationBase.qpp
    lib/QC_HttpKeepAlivePollOperationBase.qpp
    lib/QC_HttpAcceptPollOperationBase.qpp
    lib/QC_Http2PollOperationBase.qpp
    lib/QC_HttpWebSocketPollOperationBase.qpp
    lib/QC_WebSocketClientPollOperationBase.qpp
    lib/QC_PollPipeline.qpp
    lib/QC_Http1ClientPollOperationBase.qpp
    lib/QC_Http3ClientPollOperationBase.qpp
    lib/QC_Http3ServerPollOperation.qpp
    lib/QC_HttpClientPingPollOperationBase.qpp
    lib/QC_Http2ClientPollOperationBase.qpp
    lib/QC_HttpClientConnection.qpp
    lib/QC_EventLoop.qpp
    lib/QC_EventNotifier.qpp
    lib/QC_LoggerInterfaceBase.qpp
    lib/QC_AsyncIoController.qpp
    lib/ql_misc.qpp
    lib/ql_websocket.qpp
    lib/ql_compression.qpp
    lib/ql_thread.qpp
    lib/ql_crypto.qpp
    lib/ql_lib.qpp
    lib/ql_file.qpp
    lib/ql_string.qpp
    lib/ql_time.qpp
    lib/ql_math.qpp
    lib/ql_list.qpp
    lib/ql_type.qpp
    lib/ql_pwd.qpp
    lib/ql_object.qpp
    lib/ql_env.qpp
    lib/ql_dbi.qpp
    lib/ql_context.qpp
    lib/qc_option.qpp
    lib/qc_errno.qpp
    lib/qc_qore.qpp
)

set(PSEUDO_QPP_SRC
    lib/Pseudo_QC_All.qpp
    lib/Pseudo_QC_Nothing.qpp
    lib/Pseudo_QC_Date.qpp
    lib/Pseudo_QC_Object.qpp
    lib/Pseudo_QC_Hash.qpp
    lib/Pseudo_QC_String.qpp
    lib/Pseudo_QC_Binary.qpp
    lib/Pseudo_QC_List.qpp
    lib/Pseudo_QC_Buffer.qpp
    lib/Pseudo_QC_Bool.qpp
    lib/Pseudo_QC_Char.qpp
    lib/Pseudo_QC_Int.qpp
    lib/Pseudo_QC_Float.qpp
    lib/Pseudo_QC_Number.qpp
    lib/Pseudo_QC_Closure.qpp
    lib/Pseudo_QC_Callref.qpp
)

set(QORE_BIN_FILES
    doxygen/qdx
    doxygen/qjar
    bin/qdp
    bin/qget
    bin/rest
    bin/qschema
    bin/schema-reverse
    bin/sfrest
    bin/saprest
    bin/sqlutil
    bin/qdbg
    bin/qdbg-server
    bin/qdbg-remote
    bin/qdbg-vsc-adapter
    bin/qore-i18n
    bin/qore-openapi3-gen
    bin/qore-asyncapi-gen
    bin/qore-onnx
    bin/qdiff
    bin/qfmt
    bin/qpatch
    bin/qls
    bin/qore-extract-qm-metadata
)

set(QORE_DOX_TMPL_SRC
    doxygen/lang/mainpage.dox.tmpl
    doxygen/lang/000_intro.dox.tmpl
    doxygen/lang/105_desc_overview.dox.tmpl
    doxygen/lang/110_environment_variables.dox.tmpl
    doxygen/lang/115_conditional_parsing.dox.tmpl
    doxygen/lang/120_modules.dox.tmpl
    doxygen/lang/125_include_files.dox.tmpl
    doxygen/lang/130_identifiers.dox.tmpl
    doxygen/lang/135_comments.dox.tmpl
    doxygen/lang/140_variables.dox.tmpl
    doxygen/lang/145_basic_data_types.dox.tmpl
    doxygen/lang/147_fast_text_processing.dox.tmpl
    doxygen/lang/150_container_data_types.dox.tmpl
    doxygen/lang/152_code_data_types.dox.tmpl
    doxygen/lang/155_data_type_declarations.dox.tmpl
    doxygen/lang/157_lvalue_references.dox.tmpl
    doxygen/lang/160_overloading.dox.tmpl
    doxygen/lang/165_time_zones.dox.tmpl
    doxygen/lang/170_character_encoding.dox.tmpl
    doxygen/lang/175_expressions.dox.tmpl
    doxygen/lang/180_operators.dox.tmpl
    doxygen/lang/185_qore_regex.dox.tmpl
    doxygen/lang/190_date_time_arithmetic.dox.tmpl
    doxygen/lang/195_statements.dox.tmpl
    doxygen/lang/200_functions.dox.tmpl
    doxygen/lang/202_code_flags.dox.tmpl
    doxygen/lang/205_namespaces.dox.tmpl
    doxygen/lang/210_constants.dox.tmpl
    doxygen/lang/215_classes.dox.tmpl
    doxygen/lang/217_hashdecl.dox.tmpl
    doxygen/lang/218_enums.dox.tmpl
    doxygen/lang/220_threading.dox.tmpl
    doxygen/lang/222_sandboxing.dox.tmpl
    doxygen/lang/223_execution_modes.dox.tmpl
    doxygen/lang/224_plugin_types.dox.tmpl
    doxygen/lang/225_exception_handling.dox.tmpl
    doxygen/lang/226_ai_model_deployment.dox.tmpl
    doxygen/lang/230_signal_handling.dox.tmpl
    doxygen/lang/235_event_handling.dox.tmpl
    doxygen/lang/240_command_line_processing.dox.tmpl
    doxygen/lang/245_parse_directives.dox.tmpl
    doxygen/lang/250_warnings.dox.tmpl
    doxygen/lang/255_keywords.dox.tmpl
    doxygen/lang/900_release_notes.dox.tmpl
    doxygen/lang/Doxyfile.tmpl
)

# qore interpreter
set(QORE_CPP_SRC command-line.cpp qore-main.cpp)
#set(QR_CPP_SRC command-line.cpp qr-main.cpp)

# find needed libraries
qore_find_pthreads()
find_package(ICONV REQUIRED)
# Allow BISON_EXECUTABLE to be set from environment variable
if(NOT BISON_EXECUTABLE AND DEFINED ENV{BISON_EXECUTABLE})
    set(BISON_EXECUTABLE $ENV{BISON_EXECUTABLE} CACHE FILEPATH "Path to bison executable")
endif()
find_package(BISON REQUIRED)
find_package(FLEX 2.5.35 REQUIRED)
# Allow OPENSSL_ROOT_DIR to be set from environment variable (e.g. /etc/profile.d/quic.sh)
if(NOT OPENSSL_ROOT_DIR AND DEFINED ENV{OPENSSL_ROOT_DIR})
    set(OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR})
endif()
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(MPFR REQUIRED)
find_package(BZip2 REQUIRED)

# LLVM configuration for JIT compilation (required)
# On macOS, LLVM is typically not in the default search path (MacPorts installs
# to /opt/local/libexec/llvm-XX/, Homebrew to /opt/homebrew/opt/llvm/).
# Use llvm-config --cmakedir to find the active version (respects port select / brew link).
if(APPLE AND NOT DEFINED LLVM_DIR)
    find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config
        PATHS /opt/local/bin /opt/homebrew/bin /usr/local/bin
        NO_DEFAULT_PATH)
    if(LLVM_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${LLVM_CONFIG_EXECUTABLE} --cmakedir
            OUTPUT_VARIABLE LLVM_CMAKE_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(LLVM_CMAKE_DIR)
            set(LLVM_DIR "${LLVM_CMAKE_DIR}" CACHE PATH "LLVM cmake directory")
            message(STATUS "Auto-detected LLVM cmake dir via llvm-config: ${LLVM_DIR}")
        endif()
    endif()
endif()
if (DEFINED CMAKE_DISABLE_FIND_PACKAGE_LibEdit)
    set(_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT_DEFINED TRUE)
    set(_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT "${CMAKE_DISABLE_FIND_PACKAGE_LibEdit}")
else()
    set(_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT_DEFINED FALSE)
endif()
set(CMAKE_DISABLE_FIND_PACKAGE_LibEdit ON CACHE BOOL "Disable optional LibEdit discovery while loading LLVMConfig" FORCE)
find_package(LLVM REQUIRED CONFIG)
if (_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT_DEFINED)
    set(CMAKE_DISABLE_FIND_PACKAGE_LibEdit "${_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT}" CACHE BOOL "" FORCE)
else()
    unset(CMAKE_DISABLE_FIND_PACKAGE_LibEdit CACHE)
    unset(CMAKE_DISABLE_FIND_PACKAGE_LibEdit)
endif()
unset(_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT_DEFINED)
unset(_QORE_SAVE_CMAKE_DISABLE_FIND_PACKAGE_LIBEDIT)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_INCLUDE_DIRS=${LLVM_INCLUDE_DIRS}")

# llvm-objcopy is used to embed the relink-surviving qore_aot_pcloc section into AOT
# objects on Mach-O (GNU objcopy corrupts Mach-O; LLVM's objcopy handles it correctly).
# Prefer the tool shipped with the LLVM we build against; fall back to PATH.
find_program(QORE_LLVM_OBJCOPY NAMES llvm-objcopy HINTS ${LLVM_TOOLS_BINARY_DIR})
if (QORE_LLVM_OBJCOPY)
    message(STATUS "Found llvm-objcopy: ${QORE_LLVM_OBJCOPY}")
else()
    message(STATUS "llvm-objcopy not found; Mach-O AOT lazy-location sections will be unavailable")
endif()

# Homebrew LLVM loads zstd before Arrow, but can leave the zstd export set
# without the aggregate target that a later Arrow find_dependency(zstd) expects.
if (TARGET zstd::libzstd_shared AND NOT TARGET zstd::libzstd)
    add_library(zstd::libzstd INTERFACE IMPORTED)
    set_property(TARGET zstd::libzstd PROPERTY
        INTERFACE_LINK_LIBRARIES zstd::libzstd_shared)
elseif (TARGET zstd::libzstd_static AND NOT TARGET zstd::libzstd)
    add_library(zstd::libzstd INTERFACE IMPORTED)
    set_property(TARGET zstd::libzstd PROPERTY
        INTERFACE_LINK_LIBRARIES zstd::libzstd_static)
endif()

# LLVM libraries - find_package(LLVM CONFIG) sets LLVM_LIBRARY_DIRS
# but not LLVM_LIBRARIES, so we need to find the main LLVM library ourselves.
# On Debian/Ubuntu, prefer the multiarch runtime library symlink
# (/usr/lib/<multiarch>/libLLVM-<major>.so) over the development package path
# (/usr/lib/llvm-<major>/lib/libLLVM.so) to avoid leaking the development
# directory into installed RUNPATHs and confusing dpkg-shlibdeps.
string(REGEX MATCH "^[0-9]+" QORE_LLVM_MAJOR_VERSION "${LLVM_PACKAGE_VERSION}")
# find_library() below caches QORE_LLVM_SYSTEM_LIBRARY / LLVM_LIBRARIES and will
# not re-search once they are set.  When LLVM_DIR is repointed at a different
# LLVM in an existing build tree (e.g. switching llvm-20 -> llvm-21), the cached
# library would stay pinned to the previous version, silently mixing the new
# headers with the old runtime library (an ABI mismatch).  Invalidate the cached
# results whenever the resolved LLVM (cmake dir + version) differs from what they
# were last resolved for, so find_library re-runs.
set(_qore_llvm_lib_fingerprint "${LLVM_DIR}|${LLVM_PACKAGE_VERSION}")
if(DEFINED QORE_LLVM_LIBRARY_FINGERPRINT AND
        NOT QORE_LLVM_LIBRARY_FINGERPRINT STREQUAL "${_qore_llvm_lib_fingerprint}")
    unset(QORE_LLVM_SYSTEM_LIBRARY CACHE)
    unset(LLVM_LIBRARIES CACHE)
endif()
set(QORE_LLVM_LIBRARY_FINGERPRINT "${_qore_llvm_lib_fingerprint}"
    CACHE INTERNAL "LLVM dir+version the cached LLVM library was resolved for")
set(QORE_LLVM_LIBRARY_NAMES)
if(QORE_LLVM_MAJOR_VERSION)
    list(APPEND QORE_LLVM_LIBRARY_NAMES LLVM-${QORE_LLVM_MAJOR_VERSION})
endif()
list(APPEND QORE_LLVM_LIBRARY_NAMES LLVM)
if(UNIX AND NOT APPLE)
    set(QORE_LLVM_SYSTEM_LIBRARY_PATHS)
    if(CMAKE_LIBRARY_ARCHITECTURE)
        list(APPEND QORE_LLVM_SYSTEM_LIBRARY_PATHS "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()
    list(APPEND QORE_LLVM_SYSTEM_LIBRARY_PATHS "/usr/lib" "/usr/lib64")
    find_library(QORE_LLVM_SYSTEM_LIBRARY NAMES ${QORE_LLVM_LIBRARY_NAMES}
        PATHS ${QORE_LLVM_SYSTEM_LIBRARY_PATHS} NO_DEFAULT_PATH)
    if(QORE_LLVM_SYSTEM_LIBRARY AND LLVM_DIR MATCHES "^/usr/lib/llvm-[0-9]+/cmake$")
        set(LLVM_LIBRARIES "${QORE_LLVM_SYSTEM_LIBRARY}" CACHE FILEPATH "LLVM library" FORCE)
        message(STATUS "Found LLVM library: ${LLVM_LIBRARIES}")
    endif()
endif()
if(NOT LLVM_LIBRARIES AND LLVM_LIBRARY_DIRS)
    # Try to find the unified LLVM library in the provided directory
    find_library(LLVM_LIBRARIES NAMES ${QORE_LLVM_LIBRARY_NAMES} HINTS ${LLVM_LIBRARY_DIRS} NO_DEFAULT_PATH)
    if(LLVM_LIBRARIES)
        message(STATUS "Found LLVM library: ${LLVM_LIBRARIES}")
    else()
        message(WARNING "Could not find LLVM library in ${LLVM_LIBRARY_DIRS}, searching standard paths...")
        find_library(LLVM_LIBRARIES NAMES ${QORE_LLVM_LIBRARY_NAMES})
        if(LLVM_LIBRARIES)
            message(STATUS "Found LLVM library (standard path): ${LLVM_LIBRARIES}")
        else()
            message(FATAL_ERROR "Could not find LLVM library")
        endif()
    endif()
endif()

set(LLVM_LIBS ${LLVM_LIBRARIES})
message(STATUS "LLVM_LIBS=${LLVM_LIBS}")
message(STATUS "LLVM library directory: ${LLVM_LIBRARY_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

# ICU is required so the bundled i18n module is always built and available.
pkg_check_modules(ICU QUIET icu-uc icu-i18n)
if(ICU_FOUND)
    if(ICU_VERSION)
        message(STATUS "Found ICU: ${ICU_VERSION}")
    else()
        message(STATUS "Found ICU")
    endif()
else()
    find_package(ICU COMPONENTS uc i18n REQUIRED)
endif()

# c-ares is required for asynchronous DNS resolution in socket I/O (>= 1.19.1).
# Prefer an adequate system install; fall back to building the latest stable
# release from source via FetchContent (statically linked) when the system
# package is missing or older than the minimum.
#
# Minimum 1.19.1: c-ares <= 1.19.0 (e.g. Debian 12 ships 1.18.1) has an
# ares_getaddrinfo() bug that rejects the numeric service "0" (ephemeral
# port, valid per getaddrinfo(3)) with ARES_ESERVICE, which 1.18.1's
# ares_strerror() additionally renders as the opaque "unknown"; this breaks
# every listener that binds to an ephemeral port. Fixed in 1.19.1 (verified
# empirically: 1.18.1 and 1.19.0 fail, 1.19.1+ succeed).
#
# IMPORTANT: consume the pkg-config IMPORTED_TARGET (PkgConfig::CARES_PKG),
# NOT the bare ${CARES_PKG_LIBRARIES} name, so the linker resolves the exact
# library pkg-config reports (full path + include dirs) rather than a
# different libcares earlier in its default search path.
set(CARES_VERSION "1.34.6")  # latest stable; FetchContent fallback pin
set(CARES_FOUND_SYSTEM FALSE)
pkg_check_modules(CARES_PKG QUIET IMPORTED_TARGET libcares>=1.19.1)
if (CARES_PKG_FOUND)
    message(STATUS "Found system c-ares: ${CARES_PKG_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS PkgConfig::CARES_PKG)
    set(CARES_FOUND_SYSTEM TRUE)
else()
    # Try to find c-ares without pkg-config, applying the same version gate.
    # find_library returns a full path (not a bare name).
    find_path(CARES_INCLUDE_DIR ares.h)
    find_library(CARES_LIBRARY NAMES cares)
    if (CARES_INCLUDE_DIR AND CARES_LIBRARY
            AND EXISTS "${CARES_INCLUDE_DIR}/ares_version.h")
        file(STRINGS "${CARES_INCLUDE_DIR}/ares_version.h" _cares_ver_str
             REGEX "^#define ARES_VERSION_STR ")
        if (_cares_ver_str MATCHES "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
            set(_cares_ver "${CMAKE_MATCH_1}")
            if (_cares_ver VERSION_LESS "1.19.1")
                message(STATUS "system c-ares ${_cares_ver} too old (need >= 1.19.1 for ares_getaddrinfo service \"0\" fix); will build from source")
            else()
                message(STATUS "Found system c-ares: ${CARES_LIBRARY} (${_cares_ver})")
                list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${CARES_INCLUDE_DIR})
                list(APPEND LIBQORE_OPTIONAL_LIBS ${CARES_LIBRARY})
                set(CARES_FOUND_SYSTEM TRUE)
            endif()
        endif()
    endif()
endif()

if (NOT CARES_FOUND_SYSTEM)
    # Build the latest stable c-ares from source via FetchContent.
    message(STATUS "system c-ares not found or too old; building from source (c-ares ${CARES_VERSION})")
    include(FetchContent)

    # Save cache variables we override for the subproject build
    set(_SAVE_CARES_BUILD_TESTING ${BUILD_TESTING})
    set(_SAVE_CARES_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(_SAVE_CARES_STATIC ${CARES_STATIC})
    set(_SAVE_CARES_SHARED ${CARES_SHARED})
    set(_SAVE_CARES_INSTALL ${CARES_INSTALL})
    set(_SAVE_CARES_STATIC_PIC ${CARES_STATIC_PIC})
    set(_SAVE_CARES_BUILD_TOOLS ${CARES_BUILD_TOOLS})
    set(_SAVE_CARES_PIC ${CMAKE_POSITION_INDEPENDENT_CODE})

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CARES_STATIC ON CACHE BOOL "" FORCE)
    set(CARES_SHARED OFF CACHE BOOL "" FORCE)
    set(CARES_INSTALL OFF CACHE BOOL "" FORCE)
    set(CARES_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    # Static lib linked into libqore.so needs -fPIC
    set(CARES_STATIC_PIC ON CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    FetchContent_Declare(cares
        URL "https://github.com/c-ares/c-ares/releases/download/v${CARES_VERSION}/c-ares-${CARES_VERSION}.tar.gz"
        URL_HASH SHA256=912dd7cc3b3e8a79c52fd7fb9c0f4ecf0aaa73e45efda880266a2d6e26b84ef5
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(cares)

    # Restore cache variables
    if (DEFINED _SAVE_CARES_BUILD_TESTING)
        set(BUILD_TESTING ${_SAVE_CARES_BUILD_TESTING} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_TESTING CACHE)
    endif()
    if (DEFINED _SAVE_CARES_BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${_SAVE_CARES_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    if (DEFINED _SAVE_CARES_STATIC)
        set(CARES_STATIC ${_SAVE_CARES_STATIC} CACHE BOOL "" FORCE)
    else()
        unset(CARES_STATIC CACHE)
    endif()
    if (DEFINED _SAVE_CARES_SHARED)
        set(CARES_SHARED ${_SAVE_CARES_SHARED} CACHE BOOL "" FORCE)
    else()
        unset(CARES_SHARED CACHE)
    endif()
    if (DEFINED _SAVE_CARES_INSTALL)
        set(CARES_INSTALL ${_SAVE_CARES_INSTALL} CACHE BOOL "" FORCE)
    else()
        unset(CARES_INSTALL CACHE)
    endif()
    if (DEFINED _SAVE_CARES_STATIC_PIC)
        set(CARES_STATIC_PIC ${_SAVE_CARES_STATIC_PIC} CACHE BOOL "" FORCE)
    else()
        unset(CARES_STATIC_PIC CACHE)
    endif()
    if (DEFINED _SAVE_CARES_BUILD_TOOLS)
        set(CARES_BUILD_TOOLS ${_SAVE_CARES_BUILD_TOOLS} CACHE BOOL "" FORCE)
    else()
        unset(CARES_BUILD_TOOLS CACHE)
    endif()
    if (DEFINED _SAVE_CARES_PIC)
        set(CMAKE_POSITION_INDEPENDENT_CODE ${_SAVE_CARES_PIC} CACHE BOOL "" FORCE)
    else()
        unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
    endif()

    # The c-ares::cares alias resolves to the static target (CARES_SHARED=OFF)
    # and carries its own INTERFACE include directories.
    list(APPEND LIBQORE_OPTIONAL_LIBS c-ares::cares)
endif()
set(HAVE_CARES TRUE)

# Kerberos 5 / GSSAPI support is required.  Prefer pkg-config where it is
# available, then fall back to krb5-config and finally direct header/library
# lookup for platforms without pkg-config metadata.
set(QORE_KRB5_FOUND FALSE)
pkg_check_modules(KRB5 QUIET krb5)
pkg_check_modules(GSSAPI QUIET krb5-gssapi)
if (KRB5_FOUND AND GSSAPI_FOUND)
    set(QORE_KRB5_FOUND TRUE)
    set(QORE_KRB5_INCLUDE_DIRS ${KRB5_INCLUDE_DIRS} ${GSSAPI_INCLUDE_DIRS})
    set(QORE_KRB5_LIBRARY_DIRS ${KRB5_LIBRARY_DIRS} ${GSSAPI_LIBRARY_DIRS})
    set(QORE_KRB5_LINK_LIBRARIES ${GSSAPI_LDFLAGS} ${KRB5_LDFLAGS})
    set(QORE_KRB5_VERSION "${KRB5_VERSION}")
else()
    find_program(KRB5_CONFIG_EXECUTABLE NAMES krb5-config)
    if (KRB5_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${KRB5_CONFIG_EXECUTABLE} --cflags
            OUTPUT_VARIABLE _krb5_config_cflags
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _krb5_config_cflags_result
        )
        execute_process(
            COMMAND ${KRB5_CONFIG_EXECUTABLE} --libs gssapi
            OUTPUT_VARIABLE _krb5_config_libs
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _krb5_config_libs_result
        )
        if (_krb5_config_cflags_result EQUAL 0 AND _krb5_config_libs_result EQUAL 0)
            separate_arguments(_krb5_config_cflags_list UNIX_COMMAND "${_krb5_config_cflags}")
            separate_arguments(_krb5_config_libs_list UNIX_COMMAND "${_krb5_config_libs}")
            foreach(_krb5_config_flag ${_krb5_config_cflags_list})
                if (_krb5_config_flag MATCHES "^-I(.+)")
                    list(APPEND QORE_KRB5_INCLUDE_DIRS "${CMAKE_MATCH_1}")
                else()
                    list(APPEND QORE_KRB5_CFLAGS "${_krb5_config_flag}")
                endif()
            endforeach()
            set(QORE_KRB5_LINK_LIBRARIES ${_krb5_config_libs_list})
            set(QORE_KRB5_FOUND TRUE)
        endif()
    endif()
endif()
if (NOT QORE_KRB5_FOUND)
    find_path(QORE_KRB5_INCLUDE_DIR krb5.h)
    find_path(QORE_GSSAPI_INCLUDE_DIR gssapi/gssapi.h)
    find_library(QORE_KRB5_LIBRARY NAMES krb5 Kerberos)
    find_library(QORE_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi GSS)
    if (QORE_KRB5_INCLUDE_DIR AND QORE_GSSAPI_INCLUDE_DIR
            AND QORE_KRB5_LIBRARY AND QORE_GSSAPI_LIBRARY)
        set(QORE_KRB5_INCLUDE_DIRS ${QORE_KRB5_INCLUDE_DIR} ${QORE_GSSAPI_INCLUDE_DIR})
        set(QORE_KRB5_LINK_LIBRARIES ${QORE_GSSAPI_LIBRARY} ${QORE_KRB5_LIBRARY})
        set(QORE_KRB5_FOUND TRUE)
    endif()
endif()
if (QORE_KRB5_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES QORE_KRB5_INCLUDE_DIRS)
endif()
if (QORE_KRB5_LIBRARY_DIRS)
    list(REMOVE_DUPLICATES QORE_KRB5_LIBRARY_DIRS)
endif()
if (QORE_KRB5_LINK_LIBRARIES)
    list(REMOVE_DUPLICATES QORE_KRB5_LINK_LIBRARIES)
endif()
if (NOT QORE_KRB5_FOUND)
    message(FATAL_ERROR
        "Kerberos 5 / GSSAPI development files are required to build Qore. "
        "Install krb5-devel (RHEL/Fedora), libkrb5-dev (Debian/Ubuntu), "
        "krb5-dev (Alpine), or krb5 from MacPorts/Homebrew on macOS.")
endif()

set(CMAKE_REQUIRED_INCLUDES ${QORE_KRB5_INCLUDE_DIRS})
string(REPLACE ";" " " QORE_KRB5_CFLAGS_STRING "${QORE_KRB5_CFLAGS}")
set(CMAKE_REQUIRED_FLAGS ${QORE_KRB5_CFLAGS_STRING})
include(CheckIncludeFile)
check_include_file(krb5.h QORE_HAVE_KRB5_H)
check_include_file(gssapi/gssapi.h QORE_HAVE_GSSAPI_H)
check_include_file(gssapi/gssapi_ext.h QORE_HAVE_GSSAPI_EXT_H)
check_include_file(gssapi/gssapi_krb5.h QORE_HAVE_GSSAPI_KRB5_H)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_FLAGS)
if (NOT QORE_HAVE_KRB5_H OR NOT QORE_HAVE_GSSAPI_H
        OR NOT QORE_HAVE_GSSAPI_EXT_H OR NOT QORE_HAVE_GSSAPI_KRB5_H)
    message(FATAL_ERROR
        "Kerberos 5 / GSSAPI headers are incomplete; krb5.h, "
        "gssapi/gssapi.h, gssapi/gssapi_ext.h, and "
        "gssapi/gssapi_krb5.h are required")
endif()

message(STATUS "Found Kerberos 5 / GSSAPI: ${QORE_KRB5_LINK_LIBRARIES}")

# Required compression libraries
# Brotli (required)
pkg_check_modules(BROTLI_ENC QUIET libbrotlienc)
pkg_check_modules(BROTLI_DEC QUIET libbrotlidec)
if(BROTLI_ENC_FOUND AND BROTLI_DEC_FOUND)
    message(STATUS "Found Brotli: ${BROTLI_ENC_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS ${BROTLI_ENC_LIBRARIES} ${BROTLI_DEC_LIBRARIES})
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${BROTLI_ENC_INCLUDE_DIRS})
else()
    # Try to find Brotli without pkg-config
    find_path(BROTLI_INCLUDE_DIR brotli/encode.h)
    find_library(BROTLI_ENC_LIBRARY NAMES brotlienc)
    find_library(BROTLI_DEC_LIBRARY NAMES brotlidec)
    if(BROTLI_INCLUDE_DIR AND BROTLI_ENC_LIBRARY AND BROTLI_DEC_LIBRARY)
        message(STATUS "Found Brotli: ${BROTLI_ENC_LIBRARY}")
        list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${BROTLI_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${BROTLI_ENC_LIBRARY} ${BROTLI_DEC_LIBRARY})
    else()
        message(FATAL_ERROR "Brotli is required for compression support. Please install libbrotli-dev (Debian/Ubuntu) or brotli-devel (Fedora/RHEL)")
    endif()
endif()
# Always defined since brotli is now required
set(HAVE_BROTLI TRUE)

# Zstandard (required)
pkg_check_modules(ZSTD QUIET libzstd)
if(ZSTD_FOUND)
    # pkg-config reports bare library names (e.g. "zstd") and keeps the search
    # path in ZSTD_LIBRARY_DIRS.  Resolve to an absolute path: targets that link
    # zstd on their own (qcc) and the AOT static link recipe carry no -L of their
    # own, so a bare -lzstd only resolves when the library happens to live on the
    # default linker search path (it does not under /opt/local or /opt/homebrew).
    # search the pkg-config dir first and exclusively, so the library matches the
    # headers pkg-config reported when several prefixes are installed
    # (MacPorts + Homebrew); fall back to the default search path otherwise
    if(ZSTD_LIBRARY_DIRS)
        find_library(ZSTD_LIBRARY NAMES ${ZSTD_LIBRARIES} zstd
            PATHS ${ZSTD_LIBRARY_DIRS} NO_DEFAULT_PATH)
    endif()
    find_library(ZSTD_LIBRARY NAMES ${ZSTD_LIBRARIES} zstd)
    if(ZSTD_LIBRARY)
        message(STATUS "Found Zstandard: ${ZSTD_LIBRARY} (${ZSTD_VERSION})")
        list(APPEND LIBQORE_OPTIONAL_LIBS ${ZSTD_LIBRARY})
    else()
        message(STATUS "Found Zstandard: ${ZSTD_VERSION}")
        list(APPEND LIBQORE_OPTIONAL_LIBS ${ZSTD_LIBRARIES})
        # keep the bare names resolvable for every target defined below
        link_directories(${ZSTD_LIBRARY_DIRS})
    endif()
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${ZSTD_INCLUDE_DIRS})
else()
    # Try to find Zstd without pkg-config
    find_path(ZSTD_INCLUDE_DIR zstd.h)
    find_library(ZSTD_LIBRARY NAMES zstd)
    if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
        message(STATUS "Found Zstandard: ${ZSTD_LIBRARY}")
        list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${ZSTD_LIBRARY})
    else()
        message(FATAL_ERROR "Zstandard is required for compression support. Please install libzstd-dev (Debian/Ubuntu) or libzstd-devel (Fedora/RHEL)")
    endif()
endif()
# canonical zstd link spec for targets that link zstd directly (qcc); prefers the
# absolute path so no -L from an unrelated package is needed on the link line
if(ZSTD_LIBRARY)
    set(QORE_ZSTD_LIBS ${ZSTD_LIBRARY})
else()
    set(QORE_ZSTD_LIBS ${ZSTD_LIBRARIES})
endif()
# Always defined since zstd is now required
set(HAVE_ZSTD TRUE)

# LZ4 (required)
pkg_check_modules(LZ4 QUIET liblz4)
if(LZ4_FOUND)
    message(STATUS "Found LZ4: ${LZ4_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS ${LZ4_LIBRARIES})
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${LZ4_INCLUDE_DIRS})
else()
    # Try to find LZ4 without pkg-config
    find_path(LZ4_INCLUDE_DIR lz4.h)
    find_library(LZ4_LIBRARY NAMES lz4)
    if(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
        message(STATUS "Found LZ4: ${LZ4_LIBRARY}")
        list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${LZ4_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${LZ4_LIBRARY})
    else()
        message(FATAL_ERROR "LZ4 is required for compression support. Please install liblz4-dev (Debian/Ubuntu) or lz4-devel (Fedora/RHEL)")
    endif()
endif()
# Always defined since lz4 is now required
set(HAVE_LZ4 TRUE)

# musl's allocator (mallocng) releases freed memory to the kernel almost immediately, so allocation-heavy
# workloads - above all LLVM, which every AOT compilation runs - spend most of their time in mmap/munmap/madvise
# and page faults instead of in user code; an AOT batch that takes 12s with glibc takes 60s on musl, 165s of it
# in the kernel.  jemalloc restores parity (11s, 0.6s system time) at the same peak RSS, so every musl build of
# libqore links it in statically.
#
# it has to be linked into libqore itself and not just added as a DT_NEEDED dependency: musl resolves symbols
# breadth-first, so libc is reached before any second-level dependency of libqore and keeps ownership of malloc.
# libqore precedes libc in the DT_NEEDED list of every binary that links it, so when libqore defines malloc, that
# definition is the one the whole process uses.
OPTION(QORE_DISABLE_JEMALLOC "do not link jemalloc into libqore on musl targets" OFF)
set(QORE_MUSL FALSE)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
        OUTPUT_VARIABLE QORE_TARGET_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (QORE_TARGET_TRIPLE MATCHES "musl")
        set(QORE_MUSL TRUE)
    endif()
endif()

set(QORE_JEMALLOC_LIBRARY)
if (QORE_MUSL AND NOT QORE_DISABLE_JEMALLOC)
    # jemalloc is built from source rather than taken from the distribution, because it has to be built with
    # --disable-initial-exec-tls.  Distribution packages compile jemalloc's thread-specific data (je_tsd_tls) with
    # the initial-exec TLS model; linking that archive into libqore.so propagates an R_X86_64_TPOFF64 relocation
    # into the library, and musl's dynamic linker refuses initial-exec TLS relocations in any object loaded with
    # dlopen() ("initial-exec TLS resolves to dynamic definition").  libqore *is* dlopen()ed - that is how the
    # Python module loads it, so a single such relocation makes "import qoreloader" fail outright and takes every
    # embedded-Python user of Qore with it.  glibc absorbs this in its static TLS surplus; musl has no equivalent,
    # so the model has to be ours to choose and not the distribution's.
    #
    # No --with-jemalloc-prefix: libqore must export the allocator under the standard names (see the whole-archive
    # link below), so malloc/free/realloc/... have to stay unprefixed.
    #
    # --disable-cxx, for the same dlopen() reason: jemalloc's C++ integration defines operator new/delete, and
    # those are the one part of the allocator that libqore can win when it is dlopen()ed instead of linked.  libc
    # defines malloc/free and is always loaded before a dlopen()ed libqore, so musl keeps ownership of them; but
    # no libc defines the C++ operators, so they resolve to the first object that does - the jemalloc inside
    # libqore.  jemalloc's operators call je_malloc directly rather than going through the malloc symbol, so the
    # process ends up allocating through jemalloc and freeing through musl, and the first free() of a
    # new()ed pointer faults in musl's get_meta().  Without the operators, new/delete come from libstdc++, which
    # is a plain wrapper around malloc/free and therefore always follows whichever allocator won: jemalloc when
    # libqore is linked (the qore binary keeps the performance win intact), musl when it is dlopen()ed.  Either
    # way the process has exactly one allocator, which is the only property that matters here.
    include(ExternalProject)
    set(QORE_JEMALLOC_PREFIX ${CMAKE_BINARY_DIR}/jemalloc)
    set(QORE_JEMALLOC_STATIC_LIBRARY ${QORE_JEMALLOC_PREFIX}/lib/libjemalloc_pic.a)
    # The source location may be overridden with a local path (e.g. a tarball vendored into a CI image) to avoid a
    # network fetch at build time.  It must be jemalloc 5.3.0: the pinned SHA-256 below is verified whether the
    # source is downloaded or taken from a local path, so an override cannot substitute a different tree.
    set(QORE_JEMALLOC_URL "https://github.com/jemalloc/jemalloc/releases/download/5.3.0/jemalloc-5.3.0.tar.bz2"
        CACHE STRING "URL or local path to the jemalloc 5.3.0 source tarball linked into libqore on musl targets")
    ExternalProject_Add(jemalloc_ext
        URL ${QORE_JEMALLOC_URL}
        URL_HASH SHA256=2db82d1e7119df3e71b7640219b6dfe84789bc0537983c3b7ac4f7189aecfeaa
        PREFIX ${QORE_JEMALLOC_PREFIX}
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
            --prefix=${QORE_JEMALLOC_PREFIX}
            --disable-initial-exec-tls
            --disable-cxx
            --disable-shared
            --enable-static
            "CC=${CMAKE_C_COMPILER}"
        BUILD_COMMAND ${CMAKE_COMMAND} -E env $(MAKE) build_lib_static
        INSTALL_COMMAND ${CMAKE_COMMAND} -E env $(MAKE) install_lib_static install_include
        BUILD_BYPRODUCTS ${QORE_JEMALLOC_STATIC_LIBRARY}
    )
    message(STATUS "Building jemalloc from source for musl target (--disable-initial-exec-tls --disable-cxx): "
        "${QORE_JEMALLOC_STATIC_LIBRARY}")
    # the PIC archive is the only one that can be linked into a shared library
    set(QORE_JEMALLOC_LIBRARY ${QORE_JEMALLOC_STATIC_LIBRARY})
endif()

if (NOT (WIN32 OR MINGW OR MSYS))
    find_package(Backtrace)
    if (Backtrace_FOUND)
        list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${Backtrace_INCLUDE_DIRS})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${Backtrace_LIBRARIES})
    endif (Backtrace_FOUND)
endif (NOT (WIN32 OR MINGW OR MSYS))

# io_uring - async file I/O (Linux only, optional)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(URING QUIET liburing>=2.0)
    if(URING_FOUND)
        message(STATUS "Found liburing: ${URING_VERSION}")
        list(APPEND LIBQORE_OPTIONAL_LIBS ${URING_LIBRARIES})
        list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${URING_INCLUDE_DIRS})
        set(HAVE_IO_URING TRUE)
    else()
        # Try to find liburing without pkg-config
        find_path(URING_INCLUDE_DIR liburing.h)
        find_library(URING_LIBRARY NAMES uring)
        if(URING_INCLUDE_DIR AND URING_LIBRARY)
            # Verify the library has the APIs we need (io_uring_prep_cancel64
            # was added in liburing 2.0)
            include(CheckSymbolExists)
            set(CMAKE_REQUIRED_INCLUDES ${URING_INCLUDE_DIR})
            set(CMAKE_REQUIRED_LIBRARIES ${URING_LIBRARY})
            check_symbol_exists(io_uring_prep_cancel64 "liburing.h" HAVE_URING_CANCEL64)
            unset(CMAKE_REQUIRED_INCLUDES)
            unset(CMAKE_REQUIRED_LIBRARIES)
            if(HAVE_URING_CANCEL64)
                message(STATUS "Found liburing: ${URING_LIBRARY}")
                list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${URING_INCLUDE_DIR})
                list(APPEND LIBQORE_OPTIONAL_LIBS ${URING_LIBRARY})
                set(HAVE_IO_URING TRUE)
            else()
                message(STATUS "liburing found but too old (missing io_uring_prep_cancel64, need >= 2.0)")
            endif()
        endif()
    endif()
    if(HAVE_IO_URING)
        message(STATUS "io_uring support enabled")
    else()
        message(STATUS "io_uring support disabled (liburing >= 2.0 not found)")
    endif()
endif()

# HTTP/2 support via nghttp2 (required, >= 1.68.1).
# Prefer an adequate system install; fall back to building the latest stable
# release from source via FetchContent (statically linked) when the system
# package is missing or older than the minimum.
#
# Minimum 1.68.1: v1.68.1 fixes CVE-2026-27135 (GHSA-6933-cjhr-5qg6), a DoS
# via assertion failure in nghttp2_session_terminate_session[2] when extension
# frames (ALTSVC, PRIORITY_UPDATE) trigger a follow-on FRAME_SIZE_ERROR.
# Encoded as NGHTTP2_VERSION_NUM 0x014401 (1.68.1).
#
# IMPORTANT: consume the pkg-config IMPORTED_TARGET (PkgConfig::NGHTTP2_PKG),
# NOT the bare ${NGHTTP2_PKG_LIBRARIES} name. The imported target carries the
# full resolved library path plus include dirs, so the linker cannot silently
# pick a different/older libnghttp2 earlier in its default search path - which
# previously produced "undefined reference to nghttp2_session_mem_send2"
# (Http2Session.cpp selects the API by the compile-time NGHTTP2_VERSION_NUM,
# so a header/library skew only surfaced at link time).
set(NGHTTP2_VERSION "1.69.0")  # latest stable; FetchContent fallback pin
set(NGHTTP2_FOUND_SYSTEM FALSE)
pkg_check_modules(NGHTTP2_PKG QUIET IMPORTED_TARGET libnghttp2>=1.68.1)
if (NGHTTP2_PKG_FOUND)
    message(STATUS "Found system nghttp2: ${NGHTTP2_PKG_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS PkgConfig::NGHTTP2_PKG)
    set(NGHTTP2_FOUND_SYSTEM TRUE)
else()
    # Try to find nghttp2 without pkg-config, applying the same version gate.
    # find_library returns a full path (not a bare name), so this path is not
    # affected by the linker-search-order issue described above.
    find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h)
    find_library(NGHTTP2_LIBRARY NAMES nghttp2)
    if (NGHTTP2_INCLUDE_DIR AND NGHTTP2_LIBRARY
            AND EXISTS "${NGHTTP2_INCLUDE_DIR}/nghttp2/nghttp2ver.h")
        file(STRINGS "${NGHTTP2_INCLUDE_DIR}/nghttp2/nghttp2ver.h" _nghttp2_ver
             REGEX "^#define NGHTTP2_VERSION_NUM 0x")
        if (_nghttp2_ver MATCHES "0x([0-9a-fA-F]+)")
            math(EXPR _nghttp2_ver_num "0x${CMAKE_MATCH_1}" OUTPUT_FORMAT DECIMAL)
            # 0x014401 = 82945 decimal (v1.68.1)
            if (_nghttp2_ver_num LESS 82945)
                message(STATUS "system nghttp2 too old (need >= 1.68.1 for CVE-2026-27135); will build from source")
            else()
                message(STATUS "Found system nghttp2: ${NGHTTP2_LIBRARY}")
                list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
                list(APPEND LIBQORE_OPTIONAL_LIBS ${NGHTTP2_LIBRARY})
                set(NGHTTP2_FOUND_SYSTEM TRUE)
            endif()
        endif()
    endif()
endif()

if (NOT NGHTTP2_FOUND_SYSTEM)
    # Build the latest stable nghttp2 from source via FetchContent.
    # nghttp2 unconditionally creates an add_custom_target(check) like ngtcp2/
    # nghttp3, so we patch it out the same way before add_subdirectory.
    message(STATUS "system nghttp2 not found or too old; building from source (nghttp2 ${NGHTTP2_VERSION})")
    include(FetchContent)

    # Save cache variables we override for the subproject build
    set(_SAVE_NG2_BUILD_TESTING ${BUILD_TESTING})
    set(_SAVE_NG2_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(_SAVE_NG2_BUILD_STATIC_LIBS ${BUILD_STATIC_LIBS})
    set(_SAVE_NG2_ENABLE_LIB_ONLY ${ENABLE_LIB_ONLY})
    set(_SAVE_NG2_ENABLE_DOC ${ENABLE_DOC})
    set(_SAVE_NG2_PIC ${CMAKE_POSITION_INDEPENDENT_CODE})
    set(_QORE_NGHTTP2_DISABLE_FIND_PACKAGE_DEFINED)
    set(_QORE_NGHTTP2_DISABLED_FIND_PACKAGES Jansson Libevent Jemalloc)
    foreach(_qore_pkg IN LISTS _QORE_NGHTTP2_DISABLED_FIND_PACKAGES)
        set(_qore_var "CMAKE_DISABLE_FIND_PACKAGE_${_qore_pkg}")
        if (DEFINED ${_qore_var})
            list(APPEND _QORE_NGHTTP2_DISABLE_FIND_PACKAGE_DEFINED ${_qore_pkg})
            set("_QORE_NGHTTP2_SAVE_${_qore_pkg}" "${${_qore_var}}")
        endif()
        set(${_qore_var} ON CACHE BOOL "Disable optional ${_qore_pkg} discovery for Qore's nghttp2 lib-only fallback" FORCE)
    endforeach()

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE)
    # nghttp2's doc/CMakeLists.txt declares sphinx targets named after the output format,
    # one of which is `json` -- which collides with our builtin json module target (CMake
    # target names are global).  We do not build nghttp2's docs, so turn the subdirectory
    # off entirely rather than patching its sources.
    set(ENABLE_DOC OFF CACHE BOOL "" FORCE)
    # Static lib linked into libqore.so needs -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    FetchContent_Declare(nghttp2
        URL "https://github.com/nghttp2/nghttp2/releases/download/v${NGHTTP2_VERSION}/nghttp2-${NGHTTP2_VERSION}.tar.xz"
        URL_HASH SHA256=1fb324b6ec2c56f6bde0658f4139ffd8209fa9e77ce98fd7a5f63af8d0e508ad
        ${QORE_FETCHCONTENT_URL_OPTIONS}
        # Patch out the unconditional 'check' target to avoid duplicate-target
        # errors when ngtcp2/nghttp3 are also fetched.
        PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/PatchFetchContentCheckTarget.cmake
        ${QORE_FETCHCONTENT_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(nghttp2)

    # Restore cache variables
    if (DEFINED _SAVE_NG2_BUILD_TESTING)
        set(BUILD_TESTING ${_SAVE_NG2_BUILD_TESTING} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_TESTING CACHE)
    endif()
    if (DEFINED _SAVE_NG2_BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${_SAVE_NG2_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    if (DEFINED _SAVE_NG2_BUILD_STATIC_LIBS)
        set(BUILD_STATIC_LIBS ${_SAVE_NG2_BUILD_STATIC_LIBS} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_STATIC_LIBS CACHE)
    endif()
    if (DEFINED _SAVE_NG2_ENABLE_LIB_ONLY)
        set(ENABLE_LIB_ONLY ${_SAVE_NG2_ENABLE_LIB_ONLY} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_LIB_ONLY CACHE)
    endif()
    if (DEFINED _SAVE_NG2_ENABLE_DOC)
        set(ENABLE_DOC ${_SAVE_NG2_ENABLE_DOC} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_DOC CACHE)
    endif()
    if (DEFINED _SAVE_NG2_PIC)
        set(CMAKE_POSITION_INDEPENDENT_CODE ${_SAVE_NG2_PIC} CACHE BOOL "" FORCE)
    else()
        unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
    endif()
    foreach(_qore_pkg IN LISTS _QORE_NGHTTP2_DISABLED_FIND_PACKAGES)
        set(_qore_var "CMAKE_DISABLE_FIND_PACKAGE_${_qore_pkg}")
        list(FIND _QORE_NGHTTP2_DISABLE_FIND_PACKAGE_DEFINED ${_qore_pkg} _qore_was_defined)
        if (_qore_was_defined EQUAL -1)
            unset(${_qore_var} CACHE)
            unset(${_qore_var})
        else()
            set(${_qore_var} "${_QORE_NGHTTP2_SAVE_${_qore_pkg}}" CACHE BOOL "" FORCE)
            unset("_QORE_NGHTTP2_SAVE_${_qore_pkg}")
        endif()
    endforeach()
    unset(_QORE_NGHTTP2_DISABLED_FIND_PACKAGES)
    unset(_QORE_NGHTTP2_DISABLE_FIND_PACKAGE_DEFINED)
    unset(_qore_pkg)
    unset(_qore_var)
    unset(_qore_was_defined)

    # nghttp2 generates nghttp2ver.h in the build tree and ships public headers
    # under lib/includes/nghttp2/.
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS
        "${nghttp2_SOURCE_DIR}/lib/includes"
        "${nghttp2_BINARY_DIR}/lib/includes"
    )
    # Link against the static target produced when BUILD_STATIC_LIBS=ON.
    list(APPEND LIBQORE_OPTIONAL_LIBS nghttp2_static)
endif()

# QUIC/HTTP/3 support via ngtcp2 + nghttp3
# Try system packages first (pkg-config, then cmake find); fall back to
# building from source via FetchContent when not installed.
set(NGTCP2_VERSION "1.23.0")
set(NGHTTP3_VERSION "1.16.0")

set(QUIC_FOUND_SYSTEM FALSE)
# Consume the pkg-config IMPORTED_TARGETs (PkgConfig::*), NOT the bare
# ${*_PKG_LIBRARIES} names: the imported targets carry full resolved library
# paths plus include dirs, so the linker cannot silently pick a different/
# older libngtcp2/libnghttp3 earlier in its default search path.
pkg_check_modules(NGTCP2_PKG QUIET IMPORTED_TARGET libngtcp2)
pkg_check_modules(NGTCP2_CRYPTO_OSSL_PKG QUIET IMPORTED_TARGET libngtcp2_crypto_ossl)
pkg_check_modules(NGHTTP3_PKG QUIET IMPORTED_TARGET libnghttp3>=1.14.0)
if (NGTCP2_PKG_FOUND AND NGTCP2_CRYPTO_OSSL_PKG_FOUND AND NGHTTP3_PKG_FOUND)
    message(STATUS "Found system ngtcp2: ${NGTCP2_PKG_VERSION}")
    message(STATUS "Found system ngtcp2_crypto_ossl: ${NGTCP2_CRYPTO_OSSL_PKG_VERSION}")
    message(STATUS "Found system nghttp3: ${NGHTTP3_PKG_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS PkgConfig::NGTCP2_PKG PkgConfig::NGTCP2_CRYPTO_OSSL_PKG PkgConfig::NGHTTP3_PKG)
    set(QUIC_FOUND_SYSTEM TRUE)
else()
    # Try to find ngtcp2/nghttp3 without pkg-config
    find_path(NGTCP2_INCLUDE_DIR ngtcp2/ngtcp2.h)
    find_library(NGTCP2_LIBRARY NAMES ngtcp2)
    find_library(NGTCP2_CRYPTO_OSSL_LIBRARY NAMES ngtcp2_crypto_ossl)
    find_path(NGHTTP3_INCLUDE_DIR nghttp3/nghttp3.h)
    find_library(NGHTTP3_LIBRARY NAMES nghttp3)
    if (NGTCP2_INCLUDE_DIR AND NGTCP2_LIBRARY AND NGTCP2_CRYPTO_OSSL_LIBRARY AND NGHTTP3_INCLUDE_DIR AND NGHTTP3_LIBRARY
            AND EXISTS "${NGHTTP3_INCLUDE_DIR}/nghttp3/version.h")
        # Verify nghttp3 >= 1.14.0 (0x010e00): v1.13.x has a bug where the
        # client-side SETTINGS decoder skips enable_connect_protocol
        file(STRINGS "${NGHTTP3_INCLUDE_DIR}/nghttp3/version.h" _nghttp3_ver
             REGEX "^#define NGHTTP3_VERSION_NUM 0x")
        if (_nghttp3_ver MATCHES "0x([0-9a-fA-F]+)")
            math(EXPR _nghttp3_ver_num "0x${CMAKE_MATCH_1}" OUTPUT_FORMAT DECIMAL)
            # 0x010e00 = 69120 decimal (v1.14.0)
            if (_nghttp3_ver_num LESS 69120)
                message(STATUS "nghttp3 version too old (need >= 1.14.0); will build from source")
            else()
                message(STATUS "Found ngtcp2: ${NGTCP2_LIBRARY}")
                message(STATUS "Found ngtcp2_crypto_ossl: ${NGTCP2_CRYPTO_OSSL_LIBRARY}")
                message(STATUS "Found nghttp3: ${NGHTTP3_LIBRARY}")
                list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${NGTCP2_INCLUDE_DIR} ${NGHTTP3_INCLUDE_DIR})
                list(APPEND LIBQORE_OPTIONAL_LIBS ${NGTCP2_LIBRARY} ${NGTCP2_CRYPTO_OSSL_LIBRARY} ${NGHTTP3_LIBRARY})
                set(QUIC_FOUND_SYSTEM TRUE)
            endif()
        endif()
    endif()
endif()

if (NOT QUIC_FOUND_SYSTEM)
    # Build ngtcp2 and nghttp3 from source via FetchContent.
    # We use FetchContent_Populate + manual add_subdirectory because both
    # projects unconditionally create an add_custom_target(check) which
    # causes a fatal CMake duplicate-target error when both are present.
    # Patching the downloaded sources removes the conflicting target.
    message(STATUS "ngtcp2/nghttp3 not found on system; building from source (ngtcp2 ${NGTCP2_VERSION}, nghttp3 ${NGHTTP3_VERSION})")
    include(FetchContent)

    # Save cache variables that we override for subproject builds
    set(_SAVE_BUILD_TESTING ${BUILD_TESTING})
    set(_SAVE_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    set(_SAVE_ENABLE_LIB_ONLY ${ENABLE_LIB_ONLY})
    set(_SAVE_ENABLE_SHARED_LIB ${ENABLE_SHARED_LIB})
    set(_SAVE_ENABLE_STATIC_LIB ${ENABLE_STATIC_LIB})
    set(_SAVE_ENABLE_OPENSSL ${ENABLE_OPENSSL})
    set(_SAVE_ENABLE_GNUTLS ${ENABLE_GNUTLS})
    set(_SAVE_CMAKE_POSITION_INDEPENDENT_CODE ${CMAKE_POSITION_INDEPENDENT_CODE})

    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE)
    # Force static libraries for nghttp3/ngtcp2 to avoid runtime conflicts
    # with older system-installed shared libraries
    set(ENABLE_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(ENABLE_STATIC_LIB ON CACHE BOOL "" FORCE)
    # Static libraries linked into libqore.so need -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

    # --- nghttp3 (must come before ngtcp2) ---
    # Patch out the unconditional 'check' target that conflicts in multi-FetchContent builds
    FetchContent_Declare(nghttp3
        URL "https://github.com/ngtcp2/nghttp3/releases/download/v${NGHTTP3_VERSION}/nghttp3-${NGHTTP3_VERSION}.tar.xz"
        URL_HASH SHA256=776f59a99905c9a348846807b2e5ac9bb3485fc0f8c0250ba803018d5238a16e
        ${QORE_FETCHCONTENT_URL_OPTIONS}
        PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/PatchFetchContentCheckTarget.cmake
        ${QORE_FETCHCONTENT_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(nghttp3)

    # --- ngtcp2 (with OpenSSL crypto backend) ---
    set(ENABLE_OPENSSL ON CACHE BOOL "" FORCE)
    set(ENABLE_GNUTLS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ngtcp2
        URL "https://github.com/ngtcp2/ngtcp2/releases/download/v${NGTCP2_VERSION}/ngtcp2-${NGTCP2_VERSION}.tar.xz"
        URL_HASH SHA256=59d5b4211e96970f2d3d5e6876f73dce03414800ba04aa56835b132fce8de730
        ${QORE_FETCHCONTENT_URL_OPTIONS}
        PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/PatchFetchContentCheckTarget.cmake
        ${QORE_FETCHCONTENT_EXCLUDE_FROM_ALL}
    )
    FetchContent_MakeAvailable(ngtcp2)

    # Restore cache variables
    if (DEFINED _SAVE_BUILD_TESTING)
        set(BUILD_TESTING ${_SAVE_BUILD_TESTING} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_TESTING CACHE)
    endif()
    if (DEFINED _SAVE_BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${_SAVE_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    if (DEFINED _SAVE_ENABLE_LIB_ONLY)
        set(ENABLE_LIB_ONLY ${_SAVE_ENABLE_LIB_ONLY} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_LIB_ONLY CACHE)
    endif()
    if (DEFINED _SAVE_ENABLE_SHARED_LIB)
        set(ENABLE_SHARED_LIB ${_SAVE_ENABLE_SHARED_LIB} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_SHARED_LIB CACHE)
    endif()
    if (DEFINED _SAVE_ENABLE_STATIC_LIB)
        set(ENABLE_STATIC_LIB ${_SAVE_ENABLE_STATIC_LIB} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_STATIC_LIB CACHE)
    endif()
    if (DEFINED _SAVE_ENABLE_OPENSSL)
        set(ENABLE_OPENSSL ${_SAVE_ENABLE_OPENSSL} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_OPENSSL CACHE)
    endif()
    if (DEFINED _SAVE_ENABLE_GNUTLS)
        set(ENABLE_GNUTLS ${_SAVE_ENABLE_GNUTLS} CACHE BOOL "" FORCE)
    else()
        unset(ENABLE_GNUTLS CACHE)
    endif()
    if (DEFINED _SAVE_CMAKE_POSITION_INDEPENDENT_CODE)
        set(CMAKE_POSITION_INDEPENDENT_CODE ${_SAVE_CMAKE_POSITION_INDEPENDENT_CODE} CACHE BOOL "" FORCE)
    else()
        unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
    endif()

    # Add FetchContent include directories explicitly.
    # Target-based include propagation via INTERFACE_INCLUDE_DIRECTORIES can fail
    # when cmake targets are mixed into the QORE_LIBS list variable, so we add
    # the source/build include paths directly.
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS
        "${nghttp3_SOURCE_DIR}/lib/includes"
        "${nghttp3_BINARY_DIR}/lib/includes"
        "${ngtcp2_SOURCE_DIR}/lib/includes"
        "${ngtcp2_BINARY_DIR}/lib/includes"
        "${ngtcp2_SOURCE_DIR}/crypto/includes"
        "${ngtcp2_BINARY_DIR}/crypto/includes"
    )

    # Link against the FetchContent-built static library targets.
    # nghttp3/ngtcp2 use "_static" suffix for static targets when
    # ENABLE_STATIC_LIB=ON and ENABLE_SHARED_LIB=OFF.
    list(APPEND LIBQORE_OPTIONAL_LIBS ngtcp2_static ngtcp2_crypto_ossl_static nghttp3_static)
endif()

if (WIN32 OR MINGW OR MSYS)
    SET(CMAKE_DL_LIBS dl)
    SET(MPFR_LIBRARIES mpfr gmp)
    SET(OPENSSL_LIBRARIES ssl crypto)
    SET(WINSOCKET_LIBRARIES ws2_32)
    SET(NO_SIGNAL_HANDLING ON)
endif (WIN32 OR MINGW OR MSYS)

create_git_revision()

# check for C++20 (default member initializers for bit-fields require C++20)
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
    set(HAVE_CXX20 true)
    set(HAVE_CXX17 true)
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. Please use a different C++ compiler.")
endif()

# check if we need libatomic
check_cxx_atomic()

# other checks
qore_os_checks()
qore_iconv_translit_check()
qore_openssl_checks()
qore_mpfr_checks()

qore_check_headers_cxx(arpa/inet.h cxxabi.h dlfcn.h fcntl.h getopt.h glob.h grp.h iconv.h inttypes.h linux/if_packet.h memory.h netdb.h
    netinet/in.h netinet/tcp.h poll.h pwd.h stdbool.h stddef.h stdint.h stdlib.h string.h strings.h sys/select.h
    sys/socket.h sys/socket.h sys/stat.h sys/statvfs.h sys/time.h sys/types.h sys/un.h sys/wait.h termios.h umem.h
    unistd.h vfork.h winsock2.h ws2tcpip.h openssl/engine.h
)

check_cxx_source_compiles("#include <sys/stat.h>
int main() {
    struct stat value {};
    return value.st_atim.tv_nsec + value.st_mtim.tv_nsec + value.st_ctim.tv_nsec;
}"
HAVE_STRUCT_STAT_ST_TIM)
check_cxx_source_compiles("#include <sys/stat.h>
int main() {
    struct stat value {};
    return value.st_atimespec.tv_nsec + value.st_mtimespec.tv_nsec
        + value.st_ctimespec.tv_nsec;
}"
HAVE_STRUCT_STAT_ST_TIMESPEC)

# net/if_dl.h is not self-contained
include(CheckIncludeFiles)
check_include_files("sys/types.h;sys/socket.h;net/if_dl.h" HAVE_NET_IF_DL_H)

qore_search_libs(LIBQORE_LIBS setsockopt socket)
qore_search_libs(LIBQORE_LIBS gethostbyname nsl)
qore_search_libs(LIBQORE_LIBS clock_gettime rt)

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LIBQORE_LIBS})
qore_check_funcs(
    access alarm atoll bzero chown clock_gettime doprnt exp2 floor fork fsync getaddrinfo getegid geteuid
    getgid getgrgid_r getgrnam_r getgroups gethostbyaddr gethostbyname gethostname getifaddrs getnameinfo getppid
    getpwnam_r getpwuid_r getrlimit getsockopt gettimeofday getuid glob gmtime_r inet_ntop inet_pton isblank kill lchown
    localtime_r link_ntoa lstat memmem memmove memset mkfifo mkfifo nanosleep poll
    pthread_attr_getstacksize
    pthread_get_name_np pthread_get_stacksize_np pthread_getname_np putenv random
    readlink realloc realpath regcomp round select setegid setegid setenv
    seteuid seteuid setgid setgroups setsid setsockopt setuid setuid sleep socket
    strcasecmp strcasestr strchr strdup strerror strncasecmp strspn strstr
    strtoll strtol symlink system tbbmalloc timegm unsetenv usleep vfork vprintf
)
qore_func_strerror_r()
qore_gethost_checks()
qore_find_pthread_setname_np()
qore_find_pthread_getattr_np()
unset(CMAKE_REQUIRED_LIBRARIES)

qore_other_checks()
qore_dlcpp_check()
qore_stl_hash_map(include/qore/hash_map_include.h)
qore_stl_slist(include/qore/slist_include.h)
qore_cpu_checks()
qore_assembler_files()

# issue #2897 make build reproducible by omitting the hostname if possible
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    execute_process(COMMAND uname -smpo OUTPUT_VARIABLE QORE_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    execute_process(COMMAND uname -spirv OUTPUT_VARIABLE QORE_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(COMMAND uname -smr OUTPUT_VARIABLE QORE_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    execute_process(COMMAND uname -a OUTPUT_VARIABLE QORE_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
   set(TARGET_BITS 64)
else()
   set(TARGET_BITS 32)
endif()

if(NOT DEFINED NO_SIGNAL_HANDLING)
   set(HAVE_SIGNAL_HANDLING true)
endif()

# prepare scanner and parser targets
# The separate CHECK_GRAMMAR_CONFLICTS target validates the one documented
# reduce/reduce conflict, so do not duplicate that expected diagnostic here.
if(BISON_VERSION VERSION_GREATER_EQUAL 3.0)
    bison_target(qoreparser lib/parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        COMPILE_FLAGS "-Wno-deprecated -Wno-conflicts-rr")
else()
    bison_target(qoreparser lib/parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
endif()
flex_target(qorescanner lib/scanner.lpp ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp COMPILE_FLAGS --nowarn)
add_flex_bison_dependency(qorescanner qoreparser)

# Enforce the parser grammar conflict budget (0 shift/reduce, 1 reduce/reduce).  Re-runs whenever
# parser.ypp changes and fails the build if ambiguity is (re)introduced — see
# cmake/check-grammar-conflicts.sh and the conflict comment in lib/parser.ypp.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.grammar-conflicts-ok
    COMMAND ${CMAKE_COMMAND} -E env sh
            ${CMAKE_SOURCE_DIR}/cmake/check-grammar-conflicts.sh
            ${CMAKE_SOURCE_DIR}/lib/parser.ypp
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/.grammar-conflicts-ok
    DEPENDS ${CMAKE_SOURCE_DIR}/lib/parser.ypp
            ${CMAKE_SOURCE_DIR}/cmake/check-grammar-conflicts.sh
    COMMENT "Checking parser grammar conflict budget"
    VERBATIM
)
add_custom_target(CHECK_GRAMMAR_CONFLICTS
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/.grammar-conflicts-ok)

# qpp
if(NOT HAVE_GETOPT_H)
set(QPP_GETOPT_LONG_SRC lib/getopt_long.cpp)
endif()
add_executable(qpp
    lib/qpp.cpp
    ${QPP_GETOPT_LONG_SRC}
)
target_compile_definitions(qpp PUBLIC HAVE_UNIX_CONFIG_H)
# just fake qpp for macro
set(QORE_QPP_EXECUTABLE ${CMAKE_BINARY_DIR}/qpp)
target_link_libraries(qpp ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})

# malformed documentation tables are silently emitted as literal text, so they must fail the build
# in this repo; see design/doc-tables.md
option(QORE_DOX_TABLE_STRICT "treat malformed documentation tables as errors" ON)

# libqore generators

# Extend LIBQORE_QPP_SRC with optional sources BEFORE the wrap + QPP_GENERATED_FILES
# target are created, so the generated .cpp files are part of the build graph.
# Allocation tracking adds ql_alloc_tracking.qpp when -DENABLE_ALLOC_TRACKING=ON.
# (The corresponding AllocTracking.cpp and QORE_HAVE_ALLOC_TRACKING define are set
# later — see the matching if-block in this file.)
if (ENABLE_ALLOC_TRACKING AND Backtrace_FOUND)
    set(LIBQORE_QPP_SRC ${LIBQORE_QPP_SRC} lib/ql_alloc_tracking.qpp)
endif ()

qore_wrap_qpp_value(LIBQORE_QPP_CPP_SRC DOXLIST QPP_DOX_SRC METALIST QPP_META_SRC ${LIBQORE_QPP_SRC})
qore_wrap_qpp_value(PSEUDO_QPP_CPP_SRC DOXLIST QPP_DOX_SRC METALIST QPP_META_SRC ${PSEUDO_QPP_SRC})

add_custom_target(QPP_GENERATED_FILES DEPENDS ${LIBQORE_QPP_CPP_SRC} ${PSEUDO_QPP_CPP_SRC})

# install qpp metadata files into per-module subdirectory
install(FILES ${QPP_META_SRC}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/qore/metadata/core
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
add_custom_target(BISON_GENERATED_FILES DEPENDS ${BISON_qoreparser_OUTPUTS})
add_custom_target(FLEX_GENERATED_FILES DEPENDS ${FLEX_qorescanner_OUTPUTS})

set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(LIBQORE_CPP_SRC
    ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp
    lib/BarewordNode.cpp
    lib/SelfVarrefNode.cpp
    lib/StaticClassVarRefNode.cpp
    lib/lvalue_ref.cpp
    lib/ReferenceNode.cpp
    lib/BackquoteNode.cpp
    lib/ContextrefNode.cpp
    lib/ComplexContextrefNode.cpp
    lib/ContextRowNode.cpp
    lib/ClassRefNode.cpp
    lib/ScopedRefNode.cpp
    lib/AbstractQoreNode.cpp
    lib/QoreStringNode.cpp
    lib/DateTimeNode.cpp
    lib/qore_date_private.cpp
    lib/QoreHashNode.cpp
    lib/QoreBufferNode.cpp
    lib/QoreColumnarResult.cpp
    lib/BinaryNode.cpp
    lib/QoreNumberNode.cpp
    lib/QoreNullNode.cpp
    lib/QoreNothingNode.cpp
    lib/VarRefNode.cpp
    lib/FunctionCallNode.cpp
    lib/NewComplexTypeNode.cpp
    lib/QoreParseHashNode.cpp
    lib/QoreClosureParseNode.cpp
    lib/QoreClosureNode.cpp
    lib/QoreImplicitArgumentNode.cpp
    lib/QoreImplicitElementNode.cpp
    lib/Function.cpp
    lib/GlobalVariableList.cpp
    lib/FunctionList.cpp
    lib/AbstractStatement.cpp
    lib/OnBlockExitStatement.cpp
    lib/ExpressionStatement.cpp
    lib/ReturnStatement.cpp
    lib/StatementBlock.cpp
    lib/ContextStatement.cpp
    lib/SummarizeStatement.cpp
    lib/IfStatement.cpp
    lib/WhileStatement.cpp
    lib/DoWhileStatement.cpp
    lib/ForStatement.cpp
    lib/ForEachStatement.cpp
    lib/TryStatement.cpp
    lib/RethrowStatement.cpp
    lib/ThrowStatement.cpp
    lib/AssertStatement.cpp
    lib/DebugStatement.cpp
    lib/SwitchStatement.cpp
    lib/Variable.cpp
    lib/support.cpp
    lib/QoreType.cpp
    lib/ModuleManager.cpp
    lib/QorePluginRegistry.cpp
    lib/QoreException.cpp
    lib/ExceptionSink.cpp
    lib/QoreStandardException.cpp
    lib/QoreXSinkException.cpp
    lib/QoreClass.cpp
    lib/TypedHashDecl.cpp
    lib/QoreEnumDecl.cpp
    lib/QoreReflection.cpp
    lib/Context.cpp
    lib/FindNode.cpp
    lib/charset.cpp
    lib/unicode-charmaps.cpp
    lib/ThreadClosureVariableStack.cpp
    lib/ThreadLocalVariableData.cpp
    lib/WeakReferenceNode.cpp
    lib/WeakListReferenceNode.cpp
    lib/WeakHashReferenceNode.cpp
    lib/QoreProgram.cpp
    lib/QoreProgramHelper.cpp
    lib/QoreNamespace.cpp
    lib/QoreNet.cpp
    lib/QoreURL.cpp
    lib/QoreFile.cpp
    lib/QoreDir.cpp
    lib/QoreSocket.cpp
    lib/SocketSyncPoll.cpp
    lib/QoreEventLoop.cpp
    lib/QoreEventNotifier.cpp
    lib/QoreIoUring.cpp
    lib/QoreLoggerBridge.cpp
    lib/QoreAsyncIoLogger.cpp
    lib/AsyncIoControllerPriv.cpp
    lib/QoreHttpClientConnectionBase.cpp
    lib/QoreHttp1ClientConnection.cpp
    lib/QoreHttp2ClientConnection.cpp
    lib/QoreHttp3ClientConnection.cpp
    lib/QoreHttpClientConnectionManagerBase.cpp
    lib/NegotiatingConnectionPollOp.cpp
    lib/QoreSandboxManager.cpp
    lib/DateTime.cpp
    lib/QoreLib.cpp
    lib/qore_git_revision.cpp
    lib/QoreTimeZoneManager.cpp
    lib/QoreString.cpp
    lib/QoreObject.cpp
    lib/RSet.cpp
    lib/RSection.cpp
    lib/QoreParseListNode.cpp
    lib/QoreListNode.cpp
    lib/qore-main.cpp
    lib/QoreGetOpt.cpp
    lib/QoreFtpClient.cpp
    lib/DBI.cpp
    lib/ConstantList.cpp
    lib/QoreClassList.cpp
    lib/HashDeclList.cpp
    lib/EnumList.cpp
    lib/RuntimeConfig.cpp
    lib/thread.cpp
    lib/AbstractThreadResource.cpp
    lib/ThreadResourceList.cpp
    lib/VRMutex.cpp
    lib/VLock.cpp
    lib/QoreRWLock.cpp
    lib/AbstractSmartLock.cpp
    lib/SmartMutex.cpp
    lib/Datasource.cpp
    lib/SqlMutationContext.cpp
    lib/DatasourcePool.cpp
    lib/ConnectionPool.cpp
    lib/ManagedDatasource.cpp
    lib/SQLStatement.cpp
    lib/QoreSQLStatement.cpp
    lib/ExecArgList.cpp
    lib/CallReferenceNode.cpp
    lib/NamedScope.cpp
    lib/RWLock.cpp
    lib/QoreSSLBase.cpp
    lib/QoreSSLCertificate.cpp
    lib/QoreSSLPrivateKey.cpp
    lib/QoreSocketObject.cpp
    lib/QoreCondition.cpp
    lib/QoreModuleCppApi.cpp
    lib/QoreQueue.cpp
    lib/QoreQueueHelper.cpp
    lib/QoreRegex.cpp
    lib/QoreRegexBase.cpp
    lib/QoreRegexSubst.cpp
    lib/QoreTransliteration.cpp
    lib/Sequence.cpp
    lib/QoreReferenceCounter.cpp
    lib/QoreHTTPClient.cpp
    lib/QoreHttpClientObject.cpp
    lib/ParseOptionMap.cpp
    lib/QoreParseOptions.cpp
    lib/SystemEnvironment.cpp
    lib/QoreCounter.cpp
    lib/QoreFuture.cpp
    lib/ReferenceArgumentHelper.cpp
    lib/ReferenceHelper.cpp
    lib/QoreTypeInfo.cpp
    lib/QoreTypeSpecMatchRegistry.cpp
    lib/QoreIRExprRegistry.cpp
    lib/JITRuntime.cpp
    lib/QoreIRInterpreter.cpp
    lib/QoreIRIntrinsic.cpp
    lib/QoreIRBuilder.cpp
    lib/QoreIRAnalysis.cpp
    lib/QoreIRLowering.cpp
    lib/QoreIRToLLVM.cpp
    lib/QoreIRVerifier.cpp
    lib/QoreAOT.cpp
    lib/QoreAOTRuntime.cpp
    lib/QoreAOTBinary.cpp
    lib/QoreAOTInstRegistry.cpp
    lib/QoreAOTExprRegistry.cpp
    lib/QoreAOTExprSlotRegistry.cpp
    lib/QoreAOTExprNodeRegistry.cpp
    lib/QoreIRPrinter.cpp
    lib/QoreJIT.cpp
    lib/QoreParseContext.cpp
    lib/QoreDeleteOperatorNode.cpp
    lib/QoreRemoveOperatorNode.cpp
    lib/QoreSpliceOperatorNode.cpp
    lib/QoreExtractOperatorNode.cpp
    lib/QoreCastOperatorNode.cpp
    lib/QoreUnaryMinusOperatorNode.cpp
    lib/QoreUnaryPlusOperatorNode.cpp
    lib/QoreLogicalNotOperatorNode.cpp
    lib/QoreDotEvalOperatorNode.cpp
    lib/QoreLogicalEqualsOperatorNode.cpp
    lib/QoreLogicalAbsoluteEqualsOperatorNode.cpp
    lib/QoreModuloOperatorNode.cpp
    lib/QoreBinaryAndOperatorNode.cpp
    lib/QoreBinaryNotOperatorNode.cpp
    lib/QoreBinaryOrOperatorNode.cpp
    lib/QoreBinaryXorOperatorNode.cpp
    lib/QoreShiftLeftOperatorNode.cpp
    lib/QoreShiftRightOperatorNode.cpp
    lib/QoreExistsOperatorNode.cpp
    lib/QoreElementsOperatorNode.cpp
    lib/QoreInstanceOfOperatorNode.cpp
    lib/QoreHashObjectDereferenceOperatorNode.cpp
    lib/QoreRegexMatchOperatorNode.cpp
    lib/QoreRegexNMatchOperatorNode.cpp
    lib/QoreRegexExtractOperatorNode.cpp
    lib/QoreRegexSubstOperatorNode.cpp
    lib/QoreTransliterationOperatorNode.cpp
    lib/QoreAssignmentOperatorNode.cpp
    lib/QoreListAssignmentOperatorNode.cpp
    lib/QorePlusEqualsOperatorNode.cpp
    lib/QoreIntPlusEqualsOperatorNode.cpp
    lib/QoreMinusEqualsOperatorNode.cpp
    lib/QoreIntMinusEqualsOperatorNode.cpp
    lib/QoreOrEqualsOperatorNode.cpp
    lib/QoreAndEqualsOperatorNode.cpp
    lib/QoreModuloEqualsOperatorNode.cpp
    lib/QoreMultiplyEqualsOperatorNode.cpp
    lib/QoreDivideEqualsOperatorNode.cpp
    lib/QoreXorEqualsOperatorNode.cpp
    lib/QoreShiftLeftEqualsOperatorNode.cpp
    lib/QoreShiftRightEqualsOperatorNode.cpp
    lib/QorePostIncrementOperatorNode.cpp
    lib/QoreIntPostIncrementOperatorNode.cpp
    lib/QorePostDecrementOperatorNode.cpp
    lib/QoreIntPostDecrementOperatorNode.cpp
    lib/QorePreIncrementOperatorNode.cpp
    lib/QoreIntPreIncrementOperatorNode.cpp
    lib/QorePreDecrementOperatorNode.cpp
    lib/QoreIntPreDecrementOperatorNode.cpp
    lib/QoreLogicalLessThanOperatorNode.cpp
    lib/QoreLogicalGreaterThanOperatorNode.cpp
    lib/QoreLogicalLessThanOrEqualsOperatorNode.cpp
    lib/QoreLogicalGreaterThanOrEqualsOperatorNode.cpp
    lib/QoreDivisionOperatorNode.cpp
    lib/QoreMapOperatorNode.cpp
    lib/QoreMapSelectOperatorNode.cpp
    lib/QoreHashMapOperatorNode.cpp
    lib/QoreHashMapSelectOperatorNode.cpp
    lib/QoreFoldlOperatorNode.cpp
    lib/QoreIterateOperatorNode.cpp
    lib/QoreStreamingOperatorNode.cpp
    lib/QoreNullCoalescingOperatorNode.cpp
    lib/QoreValueCoalescingOperatorNode.cpp
    lib/QoreChompOperatorNode.cpp
    lib/QoreTrimOperatorNode.cpp
    lib/QoreSquareBracketsOperatorNode.cpp
    lib/QoreShiftOperatorNode.cpp
    lib/QoreUnshiftOperatorNode.cpp
    lib/QorePopOperatorNode.cpp
    lib/QorePushOperatorNode.cpp
    lib/QoreLogicalAndOperatorNode.cpp
    lib/QoreLogicalOrOperatorNode.cpp
    lib/QoreLogicalComparisonOperatorNode.cpp
    lib/QorePlusOperatorNode.cpp
    lib/QoreMinusOperatorNode.cpp
    lib/QoreMultiplicationOperatorNode.cpp
    lib/QoreBackgroundOperatorNode.cpp
    lib/QoreValue.cpp
    lib/QoreBigIntNode.cpp
    lib/QoreBigFloatNode.cpp
    lib/StreamPipe.cpp
    lib/CompressionTransforms.cpp
    lib/EncryptionTransforms.cpp
    lib/Transform.cpp
    lib/QorePseudoMethods.cpp
    lib/xxhash.cpp
    lib/FunctionalOperator.cpp
    lib/FunctionalOperatorInterface.cpp
    lib/QoreQuestionMarkOperatorNode.cpp
    lib/QoreKeysOperatorNode.cpp
    lib/QoreSelectOperatorNode.cpp
    lib/QoreRangeOperatorNode.cpp
    lib/QoreSquareBracketsRangeOperatorNode.cpp
    lib/QoreSerializable.cpp
    lib/UnicodeCharacterIterator.cpp
    lib/QoreEllipsesNode.cpp
    lib/QoreRegexInterface.cpp
    lib/QoreThreadLocalStorage.cpp
)

if (HAVE_SIGNAL_HANDLING)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/QoreSignal.cpp)
endif ()
if (NOT HAVE_GLOB)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/glob.cpp)
endif ()
if (NOT HAVE_INET_NTOP)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/inet_ntop.cpp)
endif ()
if (NOT HAVE_INET_PTON)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/inet_pton.cpp)
endif ()
if (${QORE_BUILD_TYPE_LWR} MATCHES "debug")
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/ql_debug.cpp)
endif ()
if (HAVE_TERMIOS_H)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/QoreTermIOS.cpp)
endif ()
# Non-blocking I/O support for streams (used by HTTP/2 streaming to avoid buffering large responses in memory)
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/InputStream.cpp
    lib/OutputStream.cpp
)

# Allocation tracking (malloc/free interposer for leak detection)
# Must be a separate compilation unit (not in SCU) because it defines global malloc/free symbols
# Only built when -DENABLE_ALLOC_TRACKING=ON (adds overhead to every malloc/free call)
# NOTE: LIBQORE_QPP_SRC is extended with lib/ql_alloc_tracking.qpp earlier in this file
# (just before the main qore_wrap_qpp_value() call), so that the QPP generator produces
# ql_alloc_tracking.cpp as part of the QPP_GENERATED_FILES target.  Without that hoist,
# the SCU compile fails at include time because no build rule exists for the file.
if (ENABLE_ALLOC_TRACKING AND Backtrace_FOUND)
    set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC} lib/AllocTracking.cpp)
    add_definitions(-DQORE_HAVE_ALLOC_TRACKING)
endif ()

# HTTP/2 support is always enabled (nghttp2 is required)
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/Http2Session.cpp
)

# QUIC/HTTP/3 support via ngtcp2 + nghttp3
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/QuicSession.cpp
    lib/QuicSessionTicketCache.cpp
    lib/QuicPollOperations.cpp
)

# WebSocket frame codec (RFC 6455)
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/WebSocketFrameCodec.cpp
    lib/WebSocketStreamFrameState.cpp
)

# get list of header files
file(GLOB LIBQORE_SOURCE_HEADERS "include/qore/*.h" "include/qore/intern/*.h")
set(LIBQORE_SOURCE_HEADERS ${LIBQORE_SOURCE_HEADERS} include/qore/qlist include/qore/safe_dslist include/qore/vector_map include/qore/vector_set)

# prepare libqore target
if (SINGLE_COMPILATION_UNIT)
    # Generate single-compilation-unit.cpp in the build directory to ensure
    # generated parser.cpp and scanner.cpp from the build directory are used
    # instead of any stale files from autotools builds in lib/
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(_scu_content
"// Auto-generated wrapper - ensures build directory files are used
#include \"${CMAKE_CURRENT_BINARY_DIR}/parser.cpp\"
#include \"${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp\"
#undef ECHO
#include \"${CMAKE_SOURCE_DIR}/lib/BarewordNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SelfVarrefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/StaticClassVarRefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReferenceNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/lvalue_ref.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/BackquoteNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ContextrefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ComplexContextrefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ContextRowNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ClassRefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ScopedRefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AbstractQoreNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreStringNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DateTimeNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/qore_date_private.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHashNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBufferNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreColumnarResult.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/BinaryNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNumberNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNullNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNothingNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/VarRefNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/FunctionCallNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/NewComplexTypeNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreParseHashNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreParseListNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreClosureParseNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreClosureNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreImplicitArgumentNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreImplicitElementNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Function.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/GlobalVariableList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/FunctionList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AbstractStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/OnBlockExitStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ExpressionStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReturnStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/StatementBlock.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ContextStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SummarizeStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/IfStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/WhileStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DoWhileStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ForStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ForEachStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/TryStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ThrowStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AssertStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DebugStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RethrowStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SwitchStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Variable.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/WeakReferenceNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/WeakHashReferenceNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/WeakListReferenceNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/support.cpp\"
#ifdef HAVE_SIGNAL_HANDLING
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSignal.cpp\"
#endif
#include \"${CMAKE_SOURCE_DIR}/lib/QoreType.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ModuleManager.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePluginRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreException.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ExceptionSink.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreStandardException.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreXSinkException.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreClass.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/TypedHashDecl.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEnumDecl.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreReflection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Context.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/FindNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/charset.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/unicode-charmaps.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ThreadClosureVariableStack.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ThreadLocalVariableData.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreProgram.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreProgramHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNamespace.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNet.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreURL.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreFile.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreDir.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSocket.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SocketSyncPoll.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEventLoop.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEventNotifier.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIoUring.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLoggerBridge.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAsyncIoLogger.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AsyncIoControllerPriv.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttpClientConnectionBase.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttp1ClientConnection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttp2ClientConnection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttp3ClientConnection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttpClientConnectionManagerBase.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/NegotiatingConnectionPollOp.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSandboxManager.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DateTime.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLib.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTimeZoneManager.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreString.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreObject.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RSet.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RSection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreListNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreGetOpt.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreFtpClient.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DBI.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ConstantList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreClassList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/HashDeclList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/EnumList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RuntimeConfig.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/thread.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AbstractThreadResource.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ThreadResourceList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/VRMutex.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/VLock.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRWLock.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/AbstractSmartLock.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SmartMutex.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Datasource.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SqlMutationContext.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DatasourcePool.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ConnectionPool.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ManagedDatasource.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SQLStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSQLStatement.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ExecArgList.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/CallReferenceNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/NamedScope.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RWLock.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSSLBase.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSSLCertificate.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSSLPrivateKey.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSocketObject.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreCondition.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreModuleCppApi.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreQueue.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreQueueHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegex.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexBase.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexSubst.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTransliteration.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Sequence.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreReferenceCounter.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHTTPClient.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHttpClientObject.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ParseOptionMap.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreParseOptions.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/SystemEnvironment.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreCounter.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreFuture.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReferenceArgumentHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReferenceHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTypeInfo.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTypeSpecMatchRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRExprRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreDeleteOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRemoveOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSpliceOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreExtractOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreCastOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreKeysOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreUnaryMinusOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreUnaryPlusOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalNotOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreDotEvalOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalAbsoluteEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreModuloOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBinaryAndOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBinaryOrOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBinaryXorOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBinaryNotOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreShiftLeftOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreShiftRightOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreExistsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreElementsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreInstanceOfOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreQuestionMarkOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHashObjectDereferenceOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexMatchOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexNMatchOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexExtractOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexSubstOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTransliterationOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAssignmentOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreListAssignmentOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePlusEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntPlusEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMinusEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntMinusEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreOrEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAndEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreModuloEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMultiplyEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreDivideEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreXorEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreShiftLeftEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreShiftRightEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePostIncrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntPostIncrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePostDecrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntPostDecrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePreIncrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntPreIncrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePreDecrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIntPreDecrementOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalLessThanOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalGreaterThanOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalLessThanOrEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalGreaterThanOrEqualsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreDivisionOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMapOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMapSelectOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHashMapOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreHashMapSelectOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSelectOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreFoldlOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIterateOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreStreamingOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreNullCoalescingOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreValueCoalescingOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreChompOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTrimOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSquareBracketsOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreShiftOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreUnshiftOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePopOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePushOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalAndOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalOrOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLogicalComparisonOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QorePlusOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMinusOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreMultiplicationOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBackgroundOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRangeOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSquareBracketsRangeOperatorNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreValue.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBigIntNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreBigFloatNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/FunctionalOperatorInterface.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/FunctionalOperator.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/StreamPipe.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/CompressionTransforms.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/EncryptionTransforms.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/Transform.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSerializable.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/UnicodeCharacterIterator.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEllipsesNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreRegexInterface.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreThreadLocalStorage.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_thread.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_time.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_lib.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_math.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_type.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_env.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_string.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_pwd.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_misc.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_websocket.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_list.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_crypto.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_object.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_file.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_compression.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_dbi.cpp\"
#include \"${CMAKE_BINARY_DIR}/ql_context.cpp\"
#include \"${CMAKE_BINARY_DIR}/qc_option.cpp\"
#include \"${CMAKE_BINARY_DIR}/qc_errno.cpp\"
#include \"${CMAKE_BINARY_DIR}/qc_qore.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Socket.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ProgramControl.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Program.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_DebugProgram.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Breakpoint.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Expression.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Regex.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RegexSubst.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RegexExtract.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ReadOnlyFile.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_File.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Dir.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_GetOpt.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FtpClient.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractQuantifiedIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractBidirectionalIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractQuantifiedBidirectionalIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ListIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ListReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashKeyIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashKeyReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashPairIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashPairReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashListIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HashListReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ListHashIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ListHashReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectKeyIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectKeyReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectPairIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ObjectPairReverseIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractLineIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FileLineIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_DataLineIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_InputStreamLineIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SingleValueIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RangeIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StringSplitIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StringRegexSplitIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RegexMatchIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StringCharIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Scanner.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ThreadPool.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractDatasource.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractSQLStatement.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ColumnarResult.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Datasource.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_DatasourcePool.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SqlMutationDeclaration.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractConnectionPool.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SQLStatement.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Queue.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Mutex.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Condition.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RWLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SandboxManager.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Gate.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Sequence.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Channel.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_ChannelIterator.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Counter.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Future.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FutureImpl.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Promise.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Semaphore.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AutoSemaphore.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_WaitGroup.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SSLCertificate.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SSLPrivateKey.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HTTPClient.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AutoLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AutoGate.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AutoReadLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AutoWriteLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_TermIOS.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractSmartLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_TimeZone.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_TreeMap.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_QoreParseOptions.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractThreadResource.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StreamBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_InputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_BinaryInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StringInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FileInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_EncodingConversionInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_EncodingConversionOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_OutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_BinaryOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StringOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FileOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StreamPipe.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_PipeInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_PipeOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StreamWriter.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StreamReader.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_BufferedStreamReader.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Transform.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_TransformInputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_TransformOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StdoutOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_StderrOutputStream.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Serializable.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractPollableIoObject.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractPollableIoObjectBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SocketPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SocketPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FilePollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_DelegatingPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FtpControlPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_FtpDataPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractHttpPollConnection.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpClientConnectionBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpClientConnectionManagerBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpIdlePollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpKeepAlivePollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpAcceptPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Http2PollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpWebSocketPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_WebSocketClientPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_PollPipeline.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Http1ClientPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Http3ClientPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Http3ServerPollOperation.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Http2ClientPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpClientPingPollOperationBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_HttpClientConnection.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_EventNotifier.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_EventLoop.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_LoggerInterfaceBase.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AsyncIoController.cpp\"

#include \"${CMAKE_SOURCE_DIR}/lib/QorePseudoMethods.cpp\"

#include \"${CMAKE_SOURCE_DIR}/lib/xxhash.cpp\"

#ifndef HAVE_GLOB
#include \"${CMAKE_SOURCE_DIR}/lib/glob.cpp\"
#endif
#ifndef HAVE_INET_NTOP
#include \"${CMAKE_SOURCE_DIR}/lib/inet_ntop.cpp\"
#endif
#ifndef HAVE_INET_PTON
#include \"${CMAKE_SOURCE_DIR}/lib/inet_pton.cpp\"
#endif
#ifdef DEBUG
#include \"${CMAKE_SOURCE_DIR}/lib/ql_debug.cpp\"
#endif
// Non-blocking I/O support for streams
#include \"${CMAKE_SOURCE_DIR}/lib/InputStream.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/OutputStream.cpp\"
// HTTP/2 support is always enabled (nghttp2 is required)
#include \"${CMAKE_SOURCE_DIR}/lib/Http2Session.cpp\"
// QUIC/HTTP/3 support via ngtcp2 + nghttp3
#include \"${CMAKE_SOURCE_DIR}/lib/QuicSession.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QuicSessionTicketCache.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QuicPollOperations.cpp\"
// JIT/IR/AOT compilation support
#include \"${CMAKE_SOURCE_DIR}/lib/QoreJIT.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/JITRuntime.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRInterpreter.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRIntrinsic.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRBuilder.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRAnalysis.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreParseContext.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRLowering.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRToLLVM.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRVerifier.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOT.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTRuntime.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTBinary.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTInstRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTExprRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTExprSlotRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreAOTExprNodeRegistry.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreIRPrinter.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTermIOS.cpp\"
// WebSocket frame codec (RFC 6455)
#include \"${CMAKE_SOURCE_DIR}/lib/WebSocketFrameCodec.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/WebSocketStreamFrameState.cpp\"
")
    # Conditionally add alloc tracking QPP to the SCU (the QPP-generated C++ is safe for SCU;
    # only AllocTracking.cpp with its global malloc/free symbols must be compiled separately)
    if (ENABLE_ALLOC_TRACKING AND Backtrace_FOUND)
        string(APPEND _scu_content
            "#include \"${CMAKE_BINARY_DIR}/ql_alloc_tracking.cpp\"\n")
    endif ()

    # Emit the SCU wrapper mtime-stably: write the assembled content to a
    # staging file, then configure_file ... COPYONLY copies it to the real
    # path only when the content actually differs.  A plain file(WRITE)
    # rewrites the file (and bumps its mtime) on every cmake run, which
    # forces a full libqore recompile + relink on every reconfigure.  On
    # macOS APFS/Tart that spurious relink runs concurrently (make -j) with
    # the qcc/qore/binary-module links that consume libqore, and ld fails
    # with "can't write output file" (job 175756).  Keeping the mtime stable
    # means a reconfigure (e.g. the post-module-install AOT re-probe) does
    # not relink libqore at all.
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp.in
        "${_scu_content}")
    configure_file(
        ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp
        COPYONLY)
    unset(_scu_content)
    # Files that must be compiled separately from the SCU:
    # - qore-main.cpp: includes platform dynamic-loader headers such as
    #   <link.h> that define ELF macros conflicting with LLVM headers in the SCU
    # - qore_git_revision.cpp: so git hash changes only recompile this tiny file
    # - AllocTracking.cpp: defines global malloc/free symbols
    set(LIBQORE_EXTRA_SRC lib/qore-main.cpp lib/qore_git_revision.cpp)
    if (ENABLE_ALLOC_TRACKING AND Backtrace_FOUND)
        list(APPEND LIBQORE_EXTRA_SRC lib/AllocTracking.cpp)
    endif ()

    # Make the single-compilation-unit object explicitly depend on every file it
    # #includes (hand-written lib/*.cpp, the bison/flex parser.cpp/scanner.cpp,
    # and the qpp-generated build/*.cpp).  CMake's compiler-depfile tracking does
    # cover these, but only once the SCU has been compiled at least once to
    # populate the dependency database: in a fresh or just-reconfigured build
    # tree (or when the .o predates the dependency capture) a change to a
    # #included .cpp/.qpp can be silently missed for one build cycle, producing a
    # stale libqore.  Deriving the dependency list from the generated SCU's own
    # #include lines keeps it exactly in sync with what is actually compiled and
    # forces a recompile whenever any member changes (or a .qpp -> .cpp is
    # regenerated by qpp).
    file(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp
        _scu_include_lines REGEX "^#include \"")
    set(_scu_object_deps "")
    foreach (_scu_line ${_scu_include_lines})
        string(REGEX REPLACE "^#include \"([^\"]+)\".*$" "\\1" _scu_inc "${_scu_line}")
        if (_scu_inc AND NOT _scu_inc STREQUAL _scu_line)
            list(APPEND _scu_object_deps "${_scu_inc}")
        endif ()
    endforeach ()
    list(REMOVE_DUPLICATES _scu_object_deps)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp
        PROPERTIES OBJECT_DEPENDS "${_scu_object_deps}")

    add_library(libqore SHARED ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp ${LIBQORE_EXTRA_SRC} ${LIBQORE_SOURCE_HEADERS})
else (SINGLE_COMPILATION_UNIT)
    add_library(libqore SHARED ${LIBQORE_QPP_CPP_SRC} ${LIBQORE_CPP_SRC} ${LIBQORE_ASM_SRC} ${LIBQORE_SOURCE_HEADERS})
endif (SINGLE_COMPILATION_UNIT)

add_dependencies(QPP_GENERATED_FILES qpp)
add_dependencies(libqore QPP_GENERATED_FILES BISON_GENERATED_FILES FLEX_GENERATED_FILES UpdateGitRevision CHECK_GRAMMAR_CONFLICTS)

target_compile_definitions(libqore PUBLIC _QORE_LIB_INTERN HAVE_UNIX_CONFIG_H)
if (APPLE)
    # Required to expose IPV6_PKTINFO and struct in6_pktinfo in <netinet/in.h>
    # on macOS/Darwin.  Must be a global define so it takes effect before the
    # first inclusion of <netinet/in.h> (critical for single-compilation-unit).
    target_compile_definitions(libqore PRIVATE __APPLE_USE_RFC_3542)
endif()
target_compile_definitions(libqore PRIVATE MODULE_DIR="${MODULE_DIR}")
target_compile_definitions(libqore PRIVATE MODULE_VER_DIR="${MODULE_VER_DIR}")
target_compile_definitions(libqore PRIVATE USER_MODULE_DIR="${USER_MODULE_DIR}")
target_compile_definitions(libqore PRIVATE USER_MODULE_VER_DIR="${USER_MODULE_VER_DIR}")
target_compile_definitions(libqore PRIVATE QORE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
if (QORE_LLVM_OBJCOPY)
    target_compile_definitions(libqore PRIVATE QORE_LLVM_OBJCOPY="${QORE_LLVM_OBJCOPY}")
endif()
option(ENABLE_ASYNC_IO_TRACE "Enable async I/O controller trace instrumentation" OFF)
if (ENABLE_ASYNC_IO_TRACE)
    target_compile_definitions(libqore PRIVATE DEBUG_ASYNC_IO)
    message(STATUS "Async I/O trace instrumentation enabled (set QORE_ASYNC_IO_TRACE=1 at runtime)")
endif ()
if (WIN32 OR MINGW OR MSYS)
    target_compile_definitions(libqore PUBLIC BUILDING_DLL)
endif (WIN32 OR MINGW OR MSYS)

# Include CMAKE_BINARY_DIR first so QPP-generated files take precedence over autotools-generated ones
target_include_directories(libqore PUBLIC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/include ${BZIP2_INCLUDE_DIR} ${ICONV_INCLUDE_DIR} ${MPFR_INCLUDE_DIRS} ${LIBQORE_OPTIONAL_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${PCRE2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})

set(QORE_LIBS ${BZIP2_LIBRARIES} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${ICONV_LIBRARY} ${LIBQORE_LIBS} ${LIBQORE_OPTIONAL_LIBS} ${MPFR_LIBRARIES} ${OPENSSL_LIBRARIES} ${PCRE2_LDFLAGS} ${ZLIB_LIBRARIES})
string(REPLACE ";" " " CMAKE_LDFLAGS "${QORE_LIBS}")
string(STRIP "${CMAKE_LDFLAGS}" CMAKE_LDFLAGS)
string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
target_link_libraries(libqore PRIVATE ${QORE_LIBS})

# link jemalloc ahead of everything else and with --whole-archive, so that libqore exports the complete allocator
# API (malloc/free/realloc/posix_memalign/...) and not just the members that satisfy its own undefined symbols;
# a partially-exported allocator would let some allocations reach musl's malloc while their frees reach
# jemalloc's.  The C++ operators are deliberately not part of that set (--disable-cxx above): exporting them is
# what splits the allocator when libqore is dlopen()ed rather than linked
if (QORE_JEMALLOC_LIBRARY)
    add_dependencies(libqore jemalloc_ext)
    set_property(TARGET libqore APPEND_STRING PROPERTY LINK_FLAGS
        " -Wl,--whole-archive ${QORE_JEMALLOC_LIBRARY} -Wl,--no-whole-archive")
endif()

# Link LLVM libraries by explicit library path when available so installed
# binaries do not inherit LLVM development directories in their RUNPATH.
# Otherwise fall back to the imported LLVM target.
# Use PUBLIC so LLVM propagates to consumers (e.g., qore executable)
if(LLVM_LIBS)
    target_link_libraries(libqore PUBLIC ${LLVM_LIBS})
elseif(TARGET LLVM::LLVM)
    target_link_libraries(libqore PUBLIC LLVM::LLVM)
endif()

# Generate the AOT link configuration (aot-link.conf) so an installed qore can
# reproduce the build-time link recipe when AOT-compiling .qmod modules and
# (with --static) standalone native executables, without access to the CMake
# build tree.  The reader lives in lib/QoreAOT.cpp (loadAOTLinkConfig()).
#
# macOS note: CMAKE_CXX_COMPILER is the Xcode toolchain clang++ invoked by full
# path, which (unlike the /usr/bin shim) does NOT auto-inject an SDK sysroot, so
# libc++ is not found at link time without -isysroot.  We therefore pass it, but
# point at the UNVERSIONED .../SDKs/MacOSX.sdk symlink rather than the concrete
# versioned CMAKE_OSX_SYSROOT, so the conf does not go stale (and break every AOT
# link) when the SDK is later upgraded or removed.
#
# Convert a CMake library list (absolute paths, -l/-L flags, plain names, or
# imported targets) to space-separated linker flags.
function(qore_libs_to_flags OUTPUT_VAR)
    set(_flags)
    foreach(_lib ${ARGN})
        if(_lib MATCHES "^(-|/)")
            # already a flag (-lpthread, -L/path) or an absolute library path
            list(APPEND _flags "${_lib}")
        elseif(TARGET ${_lib})
            # imported target -> resolve to its library file, else -l<name>
            get_target_property(_loc ${_lib} IMPORTED_LOCATION)
            if(NOT _loc)
                get_target_property(_loc ${_lib} IMPORTED_LOCATION_RELEASE)
            endif()
            if(_loc)
                list(APPEND _flags "${_loc}")
            else()
                list(APPEND _flags "-l${_lib}")
            endif()
        elseif(_lib)
            list(APPEND _flags "-l${_lib}")
        endif()
    endforeach()
    string(REPLACE ";" " " _flags_str "${_flags}")
    string(STRIP "${_flags_str}" _flags_str)
    set(${OUTPUT_VAR} "${_flags_str}" PARENT_SCOPE)
endfunction()

# Dynamic link (.qmod): minimal extra libs beyond -lqore; libqore pulls in its
# own transitive dependencies at load time.
qore_libs_to_flags(_QORE_AOT_DYN_LIBS ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

# Static link (--static executable): all of libqore's transitive dependencies,
# since there is no shared libqore to pull them in.  Include the LLVM library
# directory as a search path when known (LLVM libs may live off the default path).
set(_QORE_AOT_LLVM_LDIR)
if(LLVM_LIBRARY_DIRS)
    set(_QORE_AOT_LLVM_LDIR "-L${LLVM_LIBRARY_DIRS}")
endif()
qore_libs_to_flags(_QORE_AOT_STATIC_LIBS ${QORE_LIBS} ${_QORE_AOT_LLVM_LDIR} ${LLVM_LIBS})

# On macOS, supply the SDK sysroot (unversioned symlink, see note above) so the
# full-path toolchain clang++ can find libc++/system libraries when linking AOT
# output.  Prepended to both link recipes; harmless/empty on non-Apple platforms.
set(_QORE_AOT_SYSROOT)
if(APPLE AND CMAKE_OSX_SYSROOT)
    get_filename_component(_qore_sdk_dir "${CMAKE_OSX_SYSROOT}" DIRECTORY)
    if(_qore_sdk_dir AND EXISTS "${_qore_sdk_dir}/MacOSX.sdk")
        set(_QORE_AOT_SYSROOT "-isysroot ${_qore_sdk_dir}/MacOSX.sdk")
    else()
        set(_QORE_AOT_SYSROOT "-isysroot ${CMAKE_OSX_SYSROOT}")
    endif()
    string(STRIP "${_QORE_AOT_SYSROOT} ${_QORE_AOT_DYN_LIBS}" _QORE_AOT_DYN_LIBS)
    string(STRIP "${_QORE_AOT_SYSROOT} ${_QORE_AOT_STATIC_LIBS}" _QORE_AOT_STATIC_LIBS)
endif()

file(WRITE "${CMAKE_BINARY_DIR}/aot-link.conf"
"# Qore AOT link configuration - generated by CMake, do not edit\n"
"cxx=${CMAKE_CXX_COMPILER}\n"
"libdir=${CMAKE_INSTALL_FULL_LIBDIR}\n"
"dynamic_libs=${_QORE_AOT_DYN_LIBS}\n"
"static_libs=${_QORE_AOT_STATIC_LIBS}\n"
)
install(FILES "${CMAKE_BINARY_DIR}/aot-link.conf"
        DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qore")

if (WIN32 OR MINGW OR MSYS)
    target_link_libraries(libqore ${WINSOCKET_LIBRARIES})
endif (WIN32 OR MINGW OR MSYS)

set_target_properties(libqore PROPERTIES OUTPUT_NAME qore)
if (APPLE)
    set_target_properties(libqore PROPERTIES VERSION 20)
    set_target_properties(libqore PROPERTIES SOVERSION 20)
    # Keep libqore's install name consistent between build and install to avoid dyld loading duplicates.
    set_target_properties(libqore PROPERTIES INSTALL_NAME_DIR "@rpath")
else (APPLE)
    set_target_properties(libqore PROPERTIES VERSION 20.0.0)
    set_target_properties(libqore PROPERTIES SOVERSION 20)
endif (APPLE)

if (ADDRESS_SANITIZER)
    target_compile_options(libqore PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(libqore PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

# prepare qore target
add_executable(qore ${QORE_CPP_SRC})
target_link_libraries(qore libqore)
#add_executable(qr ${QR_CPP_SRC})
#target_link_libraries(qr libqore)

if (SINGLE_COMPILATION_UNIT)
    set(QORE_IR_PHASE0_SMOKE_RUNTIME_SRC ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp ${LIBQORE_EXTRA_SRC})
else (SINGLE_COMPILATION_UNIT)
    set(QORE_IR_PHASE0_SMOKE_RUNTIME_SRC ${LIBQORE_QPP_CPP_SRC} ${LIBQORE_CPP_SRC} ${LIBQORE_ASM_SRC})
endif (SINGLE_COMPILATION_UNIT)

add_executable(qore-ir-phase0-smoke EXCLUDE_FROM_ALL
    examples/test/ir/phase0_metadata_smoke.cpp
    ${QORE_IR_PHASE0_SMOKE_RUNTIME_SRC})
add_dependencies(qore-ir-phase0-smoke QPP_GENERATED_FILES BISON_GENERATED_FILES FLEX_GENERATED_FILES UpdateGitRevision)
target_compile_definitions(qore-ir-phase0-smoke PRIVATE
    _QORE_LIB_INTERN
    HAVE_UNIX_CONFIG_H
    MODULE_DIR="${MODULE_DIR}"
    MODULE_VER_DIR="${MODULE_VER_DIR}"
    USER_MODULE_DIR="${USER_MODULE_DIR}"
    USER_MODULE_VER_DIR="${USER_MODULE_VER_DIR}"
    QORE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
if (APPLE)
    target_compile_definitions(qore-ir-phase0-smoke PRIVATE __APPLE_USE_RFC_3542)
endif()
target_include_directories(qore-ir-phase0-smoke PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${BZIP2_INCLUDE_DIR}
    ${ICONV_INCLUDE_DIR}
    ${MPFR_INCLUDE_DIRS}
    ${LIBQORE_OPTIONAL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${PCRE2_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
target_link_libraries(qore-ir-phase0-smoke PRIVATE ${QORE_LIBS})
if(LLVM_LIBS)
    target_link_libraries(qore-ir-phase0-smoke PRIVATE ${LLVM_LIBS})
elseif(TARGET LLVM::LLVM)
    target_link_libraries(qore-ir-phase0-smoke PRIVATE LLVM::LLVM)
endif()
if (WIN32 OR MINGW OR MSYS)
    target_compile_definitions(qore-ir-phase0-smoke PRIVATE BUILDING_DLL)
    target_link_libraries(qore-ir-phase0-smoke PRIVATE ${WINSOCKET_LIBRARIES})
endif (WIN32 OR MINGW OR MSYS)
if (ADDRESS_SANITIZER)
    target_compile_options(qore-ir-phase0-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(qore-ir-phase0-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

# JSON C++ API smoke test.  Unlike the IR smoke targets above this deliberately links
# only libqore and includes only installed public headers -- it resolves the json module's
# QoreJsonApi struct through q_get_module_cpp_api() exactly as a builtin binary module
# (ex: avro) consumes it, with no Qore-language code involved.  Run it against the build
# tree with:
#   QORE_MODULE_DIR=build/modules/json LD_LIBRARY_PATH=build build/qore-json-cpp-api-smoke
add_executable(qore-json-cpp-api-smoke EXCLUDE_FROM_ALL
    examples/test/json/json_cpp_api.cpp)
add_dependencies(qore-json-cpp-api-smoke libqore json)
target_include_directories(qore-json-cpp-api-smoke PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(qore-json-cpp-api-smoke PRIVATE libqore)

add_executable(qore-ir-plugin-registry-smoke EXCLUDE_FROM_ALL
    examples/test/ir/plugin_registry_smoke.cpp
    ${QORE_IR_PHASE0_SMOKE_RUNTIME_SRC})
add_dependencies(qore-ir-plugin-registry-smoke QPP_GENERATED_FILES BISON_GENERATED_FILES FLEX_GENERATED_FILES UpdateGitRevision)
target_compile_definitions(qore-ir-plugin-registry-smoke PRIVATE
    _QORE_LIB_INTERN
    HAVE_UNIX_CONFIG_H
    MODULE_DIR="${MODULE_DIR}"
    MODULE_VER_DIR="${MODULE_VER_DIR}"
    USER_MODULE_DIR="${USER_MODULE_DIR}"
    USER_MODULE_VER_DIR="${USER_MODULE_VER_DIR}"
    QORE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")
if (APPLE)
    target_compile_definitions(qore-ir-plugin-registry-smoke PRIVATE __APPLE_USE_RFC_3542)
endif()
target_include_directories(qore-ir-plugin-registry-smoke PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${BZIP2_INCLUDE_DIR}
    ${ICONV_INCLUDE_DIR}
    ${MPFR_INCLUDE_DIRS}
    ${LIBQORE_OPTIONAL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${PCRE2_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
target_link_libraries(qore-ir-plugin-registry-smoke PRIVATE ${QORE_LIBS})
if(LLVM_LIBS)
    target_link_libraries(qore-ir-plugin-registry-smoke PRIVATE ${LLVM_LIBS})
elseif(TARGET LLVM::LLVM)
    target_link_libraries(qore-ir-plugin-registry-smoke PRIVATE LLVM::LLVM)
endif()
if (WIN32 OR MINGW OR MSYS)
    target_compile_definitions(qore-ir-plugin-registry-smoke PRIVATE BUILDING_DLL)
    target_link_libraries(qore-ir-plugin-registry-smoke PRIVATE ${WINSOCKET_LIBRARIES})
endif (WIN32 OR MINGW OR MSYS)
if (ADDRESS_SANITIZER)
    target_compile_options(qore-ir-plugin-registry-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(qore-ir-plugin-registry-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

add_executable(qore-arrow-c-data-smoke EXCLUDE_FROM_ALL
    examples/test/ir/arrow_c_data_smoke.cpp)
target_link_libraries(qore-arrow-c-data-smoke PRIVATE libqore)
target_include_directories(qore-arrow-c-data-smoke PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include)
if (WIN32 OR MINGW OR MSYS)
    target_compile_definitions(qore-arrow-c-data-smoke PRIVATE BUILDING_DLL)
    target_link_libraries(qore-arrow-c-data-smoke PRIVATE ${WINSOCKET_LIBRARIES})
endif (WIN32 OR MINGW OR MSYS)
if (ADDRESS_SANITIZER)
    target_compile_options(qore-arrow-c-data-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(qore-arrow-c-data-smoke PRIVATE -fno-omit-frame-pointer -fsanitize=address)
endif()

add_subdirectory(examples/plugins/sample-buffer EXCLUDE_FROM_ALL)

# qcc - Qore Code Compiler (AOT)
add_executable(qcc qcc-main.cpp)
target_include_directories(qcc PRIVATE ${ZSTD_INCLUDE_DIRS} ${ZSTD_INCLUDE_DIR})
target_link_libraries(qcc libqore ${ZLIB_LIBRARIES} ${QORE_ZSTD_LIBS} ${CMAKE_DL_LIBS})
target_compile_definitions(qcc PRIVATE
    QORE_QCC_SOURCE_INCLUDE_DIR="${CMAKE_SOURCE_DIR}/include"
    QORE_QCC_BUILD_INCLUDE_DIR="${CMAKE_BINARY_DIR}/include"
    QORE_QCC_INSTALL_INCLUDE_DIR="${CMAKE_INSTALL_FULL_INCLUDEDIR}"
    QORE_QCC_BUILD_LIBDIR="${CMAKE_BINARY_DIR}"
    QORE_QCC_INSTALL_LIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}")
set_target_properties(qcc PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
set_target_properties(qcc PROPERTIES BUILD_RPATH ${CMAKE_BINARY_DIR})

# qcc format-version fingerprint.  Purpose: decouple qmod rebuilds from
# libqore.so / qcc binary mtime.  Without this, any libqore source
# change relinks libqore → relinks qcc → all 238 qmods rebuild even
# when the change has nothing to do with the AOT binary format
# (e.g. unrelated runtime-only fixes).
#
# The fingerprint tracks only files and build settings that actually affect
# qmod output:
# the wire-format headers, the writer, the compile pipeline, the IR
# opcode/lowering headers, signature/local metadata construction, and
# qcc itself.  QoreAOTRuntime.cpp is included because it owns slot-map
# metadata deserialization and registration; stale qmods can otherwise
# survive a runtime rebuild and fail with unsupported AOT slot metadata.
#
# If a file affecting qmod output is missed here, it would cause
# silently stale qmods.  The feature-flag machinery
# (QORE_AOT_SUPPORTED_FEATURES) catches wire-format drift at load
# time, but behavioral drift (e.g. slot-map layout changes) wouldn't
# be caught — err on the side of including rather than excluding.
# Keep the handler .cpp files explicit: they are #include'd by registry
# .cpp files, but this custom command does not inherit compiler depfile
# scanning from the libqore/qcc compilation rules.
set(_qcc_format_sources
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreAOT.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreAOTBinary.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreAOTExprRegistry.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreAOTExprSlotRegistry.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreAOTInstRegistry.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIR.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRAnalysis.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRIntrinsic.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRBuilder.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRExprRegistry.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRLowering.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreIRToLLVM.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreOpcodeRegistry.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/QoreCastOperatorNode.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/Function.h
    ${CMAKE_SOURCE_DIR}/include/qore/intern/LocalVar.h
    ${CMAKE_SOURCE_DIR}/lib/Function.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOT.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTBinary.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTExprHandlers.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTExprRegistry.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTExprSlotHandlers.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTExprSlotRegistry.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTInstRegistry.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreAOTRuntime.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreCastOperatorNode.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRBuilder.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRAnalysis.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRIntrinsic.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRExprHandlers.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRExprRegistry.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRLowering.cpp
    ${CMAKE_SOURCE_DIR}/lib/QoreIRToLLVM.cpp
    ${CMAKE_SOURCE_DIR}/qcc-main.cpp
)

# Build settings can alter generated code without changing any source file.
# Keep them in a separate input so the fingerprint also changes when a new
# qcc/libqore build has different code-generation settings.
set(_qcc_format_config ${CMAKE_BINARY_DIR}/qcc-format-config.txt)
set(_qcc_format_config_content "format=1\n")
foreach(_qcc_format_config_var
        CMAKE_BUILD_TYPE
        CMAKE_CXX_COMPILER_ID
        CMAKE_CXX_COMPILER_VERSION
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_SYSTEM_NAME
        CMAKE_SYSTEM_PROCESSOR
        CMAKE_SIZEOF_VOID_P
        LLVM_PACKAGE_VERSION
        QORE_LLVM_MAJOR_VERSION)
    string(APPEND _qcc_format_config_content
        "${_qcc_format_config_var}=${${_qcc_format_config_var}}\n")
endforeach()
if (EXISTS "${_qcc_format_config}")
    file(READ "${_qcc_format_config}" _qcc_format_config_old)
else()
    set(_qcc_format_config_old "")
endif()
if (NOT _qcc_format_config_old STREQUAL _qcc_format_config_content)
    file(WRITE "${_qcc_format_config}" "${_qcc_format_config_content}")
endif()

# Rebuild qmods when this source set itself changes.  Otherwise an existing
# build tree can miss a newly-added dependency if the file is older than the
# current fingerprint.
set(_qcc_format_sources_manifest ${CMAKE_BINARY_DIR}/qcc-format-sources.txt)
set(_qcc_format_sources_manifest_content "")
foreach(_qcc_format_source IN LISTS _qcc_format_sources)
    string(APPEND _qcc_format_sources_manifest_content "${_qcc_format_source}\n")
endforeach()
string(APPEND _qcc_format_sources_manifest_content "${_qcc_format_config}\n")
if (EXISTS "${_qcc_format_sources_manifest}")
    file(READ "${_qcc_format_sources_manifest}" _qcc_format_sources_manifest_old)
else()
    set(_qcc_format_sources_manifest_old "")
endif()
if (NOT _qcc_format_sources_manifest_old STREQUAL _qcc_format_sources_manifest_content)
    file(WRITE "${_qcc_format_sources_manifest}" "${_qcc_format_sources_manifest_content}")
endif()

set(QCC_FORMAT_STAMP ${CMAKE_BINARY_DIR}/qcc-format.stamp)
add_custom_command(
    OUTPUT ${QCC_FORMAT_STAMP}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_LIST=${_qcc_format_sources_manifest}
        -DOUTPUT=${QCC_FORMAT_STAMP}
        -DSUCCESS_STAMP=${QCC_FORMAT_STAMP}
        -P ${CMAKE_SOURCE_DIR}/cmake/QoreWriteContentDigest.cmake
    DEPENDS
        ${_qcc_format_sources}
        ${_qcc_format_config}
        ${_qcc_format_sources_manifest}
        ${CMAKE_SOURCE_DIR}/cmake/QoreWriteContentDigest.cmake
    COMMENT "Updating qcc format-version fingerprint"
    VERBATIM
)
add_custom_target(qcc-format-version DEPENDS ${QCC_FORMAT_STAMP})
add_dependencies(qcc qcc-format-version)

set(QCC_FORMAT_SOURCE_QMOD_CLEAN_STAMP ${CMAKE_BINARY_DIR}/qcc-format-source-qmod-clean.stamp)
add_custom_command(
    OUTPUT ${QCC_FORMAT_SOURCE_QMOD_CLEAN_STAMP}
    COMMAND ${CMAKE_COMMAND}
        -DQORE_SOURCE_QLIB_DIR=${CMAKE_SOURCE_DIR}/qlib
        -P ${CMAKE_SOURCE_DIR}/cmake/RemoveSourceQmodSymlinks.cmake
    COMMAND ${CMAKE_COMMAND} -E touch ${QCC_FORMAT_SOURCE_QMOD_CLEAN_STAMP}
    DEPENDS ${QCC_FORMAT_STAMP} ${CMAKE_SOURCE_DIR}/cmake/RemoveSourceQmodSymlinks.cmake
    COMMENT "Removing stale source qmod symlinks after qcc format changes"
    VERBATIM
)
add_custom_target(qcc-format-source-qmod-clean DEPENDS ${QCC_FORMAT_SOURCE_QMOD_CLEAN_STAMP})

# Ensure the installed qore binary can resolve @rpath/libqore.20.dylib.
set_target_properties(qore PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
set_target_properties(qore PROPERTIES BUILD_RPATH ${CMAKE_BINARY_DIR})

# configuration files
message(STATUS "Generating unix-config.h")
configure_file(${CMAKE_SOURCE_DIR}/cmake/unix-config.h.cmake
               ${CMAKE_BINARY_DIR}/include/qore/intern/unix-config.h)
message(STATUS "Generating qore-version.h")
configure_file(${CMAKE_SOURCE_DIR}/cmake/qore-version.h.cmake
               ${CMAKE_BINARY_DIR}/include/qore/qore-version.h)
message(STATUS "Generating QoreConfigVersion.cmake")
configure_file(${CMAKE_SOURCE_DIR}/cmake/QoreConfigVersion.cmake.in
               ${CMAKE_BINARY_DIR}/cmake/QoreConfigVersion.cmake @ONLY)
message(STATUS "Generating QoreConfig.cmake")
configure_file(${CMAKE_SOURCE_DIR}/cmake/QoreConfig.cmake.in
               ${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake @ONLY)
message(STATUS "Generating qore.pc")
configure_file(${CMAKE_SOURCE_DIR}/qore.pc.in
               ${CMAKE_BINARY_DIR}/qore.pc @ONLY)

# prepare dist target
qore_dist(${VERSION})

# prepare installation targets
install(PROGRAMS ${QORE_BIN_FILES}
        DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})

install(TARGETS qore qcc qpp libqore
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
            COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP}
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
            COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
            COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
install(FILES ${QCC_FORMAT_STAMP}
        DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
#install(TARGETS qore qpp qr libqore
#        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
#        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
install(FILES ${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake ${CMAKE_BINARY_DIR}/cmake/QoreConfigVersion.cmake cmake/QoreMacros.cmake cmake/QoreWriteContentDigest.cmake cmake/QoreWriteSourceContentDigests.cmake
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Qore
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
install(PROGRAMS tools/qore-qo-incremental tools/qore-qo-source-order
    tools/qore-qo-source-parse-defines tools/qore-qo-batch-bootstrap
    tools/qore-qo-incremental-plan tools/qore-qo-prune
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Qore
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
install(FILES ${CMAKE_BINARY_DIR}/qore.pc
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
install(DIRECTORY include/qore
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP}
        PATTERN intern EXCLUDE
        PATTERN minitest.hpp EXCLUDE
        PATTERN macros-none.h EXCLUDE
        PATTERN hash_map_include.h EXCLUDE
        PATTERN slist_include.h EXCLUDE
        PATTERN qore-version.h EXCLUDE)
install(FILES ${CMAKE_BINARY_DIR}/include/qore/qore-version.h
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/qore
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
file(GLOB QORE_MAN_PAGES man/*.1)
install(FILES ${QORE_MAN_PAGES}
        DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})
install(FILES
            doxygen/Doxyfile.in
            doxygen/qore-logo-55x200.png
            doxygen/qore-logo-55x151-white.png
            doxygen/dox_qore.css
            doxygen/DataProvider-Full.svg
            doxygen/elastic-logo.svg
            doxygen/Haltian-EmpathicBuilding.svg
            doxygen/SqlUtil-Full.svg
            doxygen/Qore-Q.ico
            doxygen/header_template.html
            doxygen/footer_template.html
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/qore
        COMPONENT ${QORE_INSTALL_COMPONENT_BOOTSTRAP})

# prepare documentation targets
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # documentation
    configure_file(${CMAKE_SOURCE_DIR}/doxygen/footer_template.html ${CMAKE_BINARY_DIR}/doxygen/footer_template.html @ONLY)
    add_custom_target(docs COMMENT "Generating API documentation")

    # doc generators
    qore_wrap_dox(QORE_DOX_SRC ${QORE_DOX_TMPL_SRC})
    add_custom_target(QORE_DOX_FILES DEPENDS ${QORE_DOX_SRC})
    add_dependencies(QORE_DOX_FILES qpp)

    # get space-separated string from list of generated files
    string(REPLACE ";" " " QORE_DOX_SRC_STRING "${QORE_DOX_SRC}")

    # get space-separated string from list of generated files
    string(REPLACE ";" " " QPP_DOX_SRC_STRING "${QPP_DOX_SRC}")

    add_dependencies(libqore QORE_DOX_FILES)

    # library
    message(STATUS "Preparing Doxyfile for library")
    configure_file(${CMAKE_SOURCE_DIR}/doxygen/lib/Doxyfile.in ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lib @ONLY)
    add_custom_target(docs-lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/library/html
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lib
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/library/html/*.html
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/library/html/search/*.html
        COMMENT "Generating API documentation with Doxygen for: lib"
        VERBATIM
    )
    add_dependencies(docs-lib qore)
    add_dependencies(docs docs-lib)

    # lang - two-pass build:
    # 1. docs-lang: builds with empty TAGFILES, generates qore.tag for modules to use
    # 2. docs-lang-final: rebuilds after modules are built, with module tagfiles for cross-referencing
    message(STATUS "Preparing Doxyfile for lang (initial pass, without module tags)")
    set(QORE_LANG_DOC_TAGFILES "" CACHE INTERNAL "Module tag files for lang docs cross-referencing")
    set(QORE_LANG_DOC_PREDEFINED "" CACHE INTERNAL "Preprocessor definitions for lang docs (initial pass)")
    set(QORE_LANG_DOC_SECTIONS "" CACHE INTERNAL "Enabled sections for lang docs (initial pass)")
    configure_file(${CMAKE_SOURCE_DIR}/doxygen/lang/Doxyfile.in ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.initial @ONLY)
    # Suppress doc error warnings in initial pass (no module TAGFILES available yet)
    file(APPEND ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.initial "\n# Suppress warnings for initial pass (no module TAGFILES available)\nWARN_IF_DOC_ERROR = NO\n")
    add_custom_target(docs-lang
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/lang/html
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.initial
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/*.js
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.js
        DEPENDS QORE_DOX_FILES QPP_GENERATED_FILES
        COMMENT "Generating API documentation with Doxygen for: lang (initial pass)"
        VERBATIM
    )
    add_dependencies(docs-lang qore)
    # docs will depend on docs-lang-final, not docs-lang (added after modules are defined)
endif (DOXYGEN_FOUND)

add_custom_target(install-docs
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/docs/ ${CMAKE_INSTALL_PREFIX}/share/doc/qore
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/docs
    COMMENT "Installing API documentation to: ${CMAKE_INSTALL_PREFIX}/share/doc/qore"
    VERBATIM
)
if (DOXYGEN_FOUND)
    add_dependencies(install-docs docs)
endif (DOXYGEN_FOUND)
# end of documentation

# test modules for the module C++ API mechanism; built but never installed
add_subdirectory(examples/test/module-cpp-api)

# astparser module
add_subdirectory(modules/astparser)
# avro module
add_subdirectory(modules/avro)
# test module consuming the avro C++ API; built but never installed
add_subdirectory(examples/test/modules/avro/cpp-api)
# reflection module
add_subdirectory(modules/reflection)
# logger module
add_subdirectory(modules/logger_bin)
# linenoise module
add_subdirectory(modules/linenoise)

# mock DBI driver for the test suite; not installed
add_subdirectory(modules/dbi_test)
# i18n module
add_subdirectory(modules/i18n)
# json module
add_subdirectory(modules/json)
# krb5 module
add_subdirectory(modules/krb5)
# mongodb module (optional, requires libmongoc)
# Try pkg-config first (more reliable, avoids broken cmake configs on some distros)
# mongo-c-driver 2.x renamed pkg-config files: libmongoc-1.0 -> mongoc2
set(MONGOC_FOUND FALSE)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(MONGOC QUIET mongoc2)
    if (NOT MONGOC_FOUND)
        pkg_check_modules(MONGOC QUIET libmongoc-1.0)
    endif()
endif()
if (NOT MONGOC_FOUND)
    # Fall back to cmake config (may fail on distros with broken static lib references)
    # mongo-c-driver 2.x: package "mongoc", 1.x: package "mongoc-1.0"
    find_package(mongoc QUIET)
    if (mongoc_FOUND)
        set(MONGOC_FOUND TRUE)
    else()
        find_package(mongoc-1.0 QUIET)
        if (mongoc-1.0_FOUND)
            set(MONGOC_FOUND TRUE)
        endif()
    endif()
endif()
if (MONGOC_FOUND)
    add_subdirectory(modules/mongodb)
else()
    message(STATUS "libmongoc not found - mongodb module will not be built")
endif()
# ml module (requires Eigen3 >= 3.4; also accepts Eigen 5.x)
# Try with version constraint first (works for Eigen 3.4.x), then without (for Eigen 5.x
# whose Eigen3ConfigVersion.cmake uses range-based checking that rejects bare "3.4")
find_package(Eigen3 3.4 QUIET NO_MODULE)
if (NOT Eigen3_FOUND)
    find_package(Eigen3 REQUIRED NO_MODULE)
    if (Eigen3_FOUND AND Eigen3_VERSION VERSION_LESS "3.4")
        message(FATAL_ERROR "Eigen3 >= 3.4 is required, found ${Eigen3_VERSION}")
    endif()
endif()
add_subdirectory(modules/ml)
add_subdirectory(modules/tokenizer)
add_subdirectory(modules/ocr)
add_subdirectory(modules/dataframe)
add_subdirectory(modules/sshutil)
# protobuf module (requires libprotobuf)
# Try MODULE mode first (CMake's FindProtobuf) — avoids broken CONFIG files
# (e.g., MacPorts protobuf-targets-macports.cmake referencing missing upb targets)
find_package(Protobuf MODULE QUIET)
if (NOT Protobuf_FOUND)
    find_package(Protobuf CONFIG REQUIRED)
endif()
add_subdirectory(modules/protobuf)

if (UNIX)
    add_custom_target(plugin-type-verify-dataframe
        COMMAND ${CMAKE_COMMAND} -E env
            "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
            "$<TARGET_FILE:qore-ir-plugin-registry-smoke>"
        COMMAND "${CMAKE_SOURCE_DIR}/tools/plugin-type-verify.sh"
            --build-dir "${CMAKE_BINARY_DIR}"
            --qore-bin "$<TARGET_FILE:qore>"
            --module dataframe
            --qtest "${CMAKE_SOURCE_DIR}/modules/dataframe/test/dataframe.qtest"
            --exec-modes ast,ir,jit,tiered
            --skip-internal-smoke
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS qore qore-ir-plugin-registry-smoke reflection dataframe
        COMMENT "Verifying plugin type support with dataframe"
        VERBATIM
    )
    add_custom_target(plugin-type-verify DEPENDS plugin-type-verify-dataframe)
endif()

# HACK: we need to simulate QORE_USER_MODULES_DIR as the versioned one
#       for qore core qlib modules.
# NOTE: anything below will be installed into versioned dirs!
set(QORE_USER_MODULES_DIR ${USER_MODULE_VER_DIR})
set(QORE_AOT_MODULES_DIR ${MODULE_VER_DIR})
# modules must go after docs target - it's depending on it

# qlib modules + their documentation
# ---
# modules here are divided to groups in the order of dependence
# and then sorted alphabetically inside the groups

# metadata extraction for qlib modules (uses build-tree qore + astparser)
# QM metadata extraction needs the json binary module (via QoreApiMetadata), which is built
# from this tree -- see modules/json.  It is listed in QORE_QM_METADATA_MODULE_DIRS below so
# the build-tree qore can load it, and in QORE_AOT_BINARY_MODULE_TARGETS so it is always built
# before any metadata scan runs.  Metadata extraction is therefore unconditional; it no longer
# depends on a compatible qore+json pair already being installed on the build host.
set(QORE_EXECUTABLE ${CMAKE_BINARY_DIR}/qore)
set(QORE_QM_METADATA_EXECUTABLE ${CMAKE_SOURCE_DIR}/bin/qore-extract-qm-metadata)
set(QORE_QM_METADATA_SUBDIR core-qlib)
set(QORE_QM_METADATA_MODULE_DIRS
    ${CMAKE_BINARY_DIR}/modules/astparser
    ${CMAKE_BINARY_DIR}/modules/avro
    ${CMAKE_BINARY_DIR}/modules/json
    ${CMAKE_BINARY_DIR}/modules/reflection
    ${CMAKE_BINARY_DIR}/modules/logger_bin
    ${CMAKE_BINARY_DIR}/modules/linenoise
    ${CMAKE_BINARY_DIR}/modules/krb5
    ${CMAKE_BINARY_DIR}/modules/mongodb
    ${CMAKE_BINARY_DIR}/modules/ml
    ${CMAKE_BINARY_DIR}/modules/tokenizer
    ${CMAKE_BINARY_DIR}/modules/ocr
    ${CMAKE_BINARY_DIR}/modules/dataframe
    ${CMAKE_BINARY_DIR}/modules/protobuf)
string(REPLACE ";" ":" QORE_QM_METADATA_MODULE_DIRS_STR "${QORE_QM_METADATA_MODULE_DIRS}")
set(QORE_BUILD_EXTERNAL_MODULE_DIRS
    ${CMAKE_INSTALL_FULL_DATADIR}/qore-modules
    ${CMAKE_INSTALL_FULL_LIBDIR}/qore-modules
    /usr/local/share/qore-modules
    /usr/local/lib/qore-modules
    /usr/share/qore-modules
    /usr/lib/qore-modules)
if(CMAKE_LIBRARY_ARCHITECTURE)
    list(APPEND QORE_BUILD_EXTERNAL_MODULE_DIRS
        /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qore-modules
        /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/qore-modules)
endif()
if(APPLE)
    list(APPEND QORE_BUILD_EXTERNAL_MODULE_DIRS
        /opt/local/share/qore-modules
        /opt/local/lib/qore-modules
        /opt/homebrew/share/qore-modules
        /opt/homebrew/lib/qore-modules)
endif()
list(REMOVE_DUPLICATES QORE_BUILD_EXTERNAL_MODULE_DIRS)
string(REPLACE ";" ":" QORE_BUILD_EXTERNAL_MODULE_DIRS_STR "${QORE_BUILD_EXTERNAL_MODULE_DIRS}")
set(QORE_BUILD_MODULE_DIRS_STR
    "${QORE_QM_METADATA_MODULE_DIRS_STR}:${CMAKE_SOURCE_DIR}/qlib:${QORE_BUILD_EXTERNAL_MODULE_DIRS_STR}")
set(QORE_QM_METADATA_ENV
    "QORE_MODULE_DIR=${QORE_BUILD_MODULE_DIRS_STR}"
    "QORE_INCLUDE_DIR="
    "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}")
set(QORE_AOT_BINARY_MODULE_TARGETS)
foreach(_qore_build_module astparser avro json reflection logger_bin linenoise krb5 mongodb ml tokenizer ocr dataframe protobuf)
    if(TARGET ${_qore_build_module})
        list(APPEND QORE_AOT_BINARY_MODULE_TARGETS ${_qore_build_module})
    endif()
endforeach()
set(QORE_QM_METADATA_DEPENDS qore ${QORE_AOT_BINARY_MODULE_TARGETS})

# QM metadata extraction is unconditional: json is built from this tree and is guaranteed to
# exist before any metadata scan runs (QORE_QM_METADATA_DEPENDS above).  This replaces a probe
# that ran an *installed* qore against an *installed* json and compared its module API against
# this source tree, silently disabling metadata for the whole build on any mismatch -- a
# bootstrap cycle that made a from-scratch build unable to produce metadata at all.
set(QORE_METADATA_DIR ${CMAKE_INSTALL_DATADIR}/qore/metadata)

# An installed qore is still used below to probe *external* binary modules whose availability
# changes qlib AOT output; that probe degrades gracefully when qore is not installed yet.
find_program(_qore_exe NAMES qore HINTS ${CMAKE_INSTALL_FULL_BINDIR})

# Probe external binary modules whose availability at AOT-compile time
# changes qlib qmod output (via %try-module / %ifdef No<Name> branches in
# the qlib sources).  Record the probe results in a stamp file; the AOT
# rules in QORE_USER_MODULE_AOT_RULES depend on this stamp so re-running
# cmake after installing or removing one of these modules forces a
# re-AOT, instead of leaving stale qmods on disk with the wrong
# %ifdef branch baked in.
#
# `configure_file ... COPYONLY` only touches the destination when the
# content changes -- so qmods don't rebuild on every cmake run, only
# when the probe set actually changes.
set(_qore_aot_probe_modules yaml xml uuid msgpack process imagemagick sysconf oracle)
set(_qore_aot_probe_content "")
if(_qore_exe)
    foreach(_pm ${_qore_aot_probe_modules})
        execute_process(
            COMMAND ${_qore_exe} -l ${_pm} -ne
                "printf(\"%d.%d\", get_module_hash().${_pm}.api_major, get_module_hash().${_pm}.api_minor);"
            RESULT_VARIABLE _pm_rc
            OUTPUT_VARIABLE _pm_api
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_pm_rc EQUAL 0 AND NOT "${_pm_api}" STREQUAL "")
            string(APPEND _qore_aot_probe_content "${_pm}=${_pm_api}\n")
        else()
            string(APPEND _qore_aot_probe_content "${_pm}=missing\n")
        endif()
    endforeach()
    unset(_pm_rc)
    unset(_pm_api)
else()
    foreach(_pm ${_qore_aot_probe_modules})
        string(APPEND _qore_aot_probe_content "${_pm}=no-qore-yet\n")
    endforeach()
endif()
set(QORE_AOT_PROBE_STAMP ${CMAKE_BINARY_DIR}/qore-aot-probe.stamp)
file(WRITE ${CMAKE_BINARY_DIR}/qore-aot-probe.stamp.in "${_qore_aot_probe_content}")
configure_file(
    ${CMAKE_BINARY_DIR}/qore-aot-probe.stamp.in
    ${QORE_AOT_PROBE_STAMP}
    COPYONLY)
unset(_qore_aot_probe_modules)
unset(_qore_aot_probe_content)
unset(_qore_exe)

# base modules
qore_user_module("qlib/DatasourceProvider.qm")
qore_user_module("qlib/DebugUtil.qm")
qore_user_module("qlib/FsUtil.qm")
qore_user_module("qlib/HttpStreamClient.qm")
qore_user_module("qlib/Qdx.qm")
qore_user_module("qlib/Qorize.qm")
qore_user_module("qlib/QoreHistory")
qore_user_module("qlib/QoreRepl")
qore_user_module("qlib/Util.qm")
qore_user_module("qlib/WebSocketUtil.qm")
qore_user_module("qlib/MapperUtil.qm")
qore_user_module("qlib/QoreApiMetadata")
qore_user_module("qlib/QoreCodeCompletion")
qore_user_module("qlib/QoreCodeFormat")
qore_user_module("qlib/QLS")

# base 2 modules
qore_user_module("qlib/FileLocationHandler")
qore_user_module("qlib/Diff.qm")
qore_user_module("qlib/Logger.qm")
qore_user_module("qlib/LoggerSemanticSearch")
qore_user_module("qlib/ProviderIndexUtil")
qore_user_module("qlib/Krb5Util")
qore_user_module("qlib/Mime.qm")
qore_user_module("qlib/QUnit.qm")
qore_user_module("qlib/TextWrap.qm")
qore_user_module("qlib/EdifactUtil")
qore_user_module("qlib/Hl7v2Util" "hl7v2-logo.svg")
qore_user_module("qlib/MllpUtil")

# base 3 modules
qore_user_module("qlib/DataProvider" "../../doxygen/DataProvider-Full.svg;Qore-Q-square.svg")
qore_user_module("qlib/AvroUtil.qm")
qore_user_module("qlib/DataProviderML")
qore_user_module("qlib/DataProviderDataFrame")
qore_user_module("qlib/QoreModelRegistry")
qore_user_module("qlib/QoreOnnxExport")
qore_user_module("qlib/QoreOnnxTools")
qore_user_module("qlib/QoreMLUtils")
qore_user_module("qlib/QoreTokenizerUtils")
qore_user_module("qlib/QoreCrypto")
qore_user_module("qlib/QoreLlmTypes.qm")
qore_user_module("qlib/QoreLlmUtils")
qore_user_module("qlib/QoreStructuredExtraction")
qore_user_module("qlib/QoreGuardrails")
qore_user_module("qlib/QoreOcrUtils")
qore_user_module("qlib/QoreVisionExtraction")
qore_user_module("qlib/QoreAgentStrategies")
qore_user_module("qlib/QoreRagTypes.qm")
qore_user_module("qlib/QoreRagUtils")
qore_user_module("qlib/QoreAsyncJobUtils.qm")
qore_user_module("qlib/QoreSpeechUtils.qm")
qore_user_module("qlib/QoreMediaUtils.qm")
qore_user_module("qlib/DpqlWebSocket")
qore_user_module("qlib/DebugProgramControl.qm")
qore_user_module("qlib/HttpUtil.qm")
qore_user_module("qlib/LineCoverage.qm")
qore_user_module("qlib/HttpServerUtil.qm")
qore_user_module("qlib/MailMessage.qm")

# base 4 modules
qore_user_module("qlib/AsyncSocketIo")
qore_user_module("qlib/FilePoller.qm")
qore_user_module("qlib/FileDataProvider" "file-logo-white.svg;file-logo-black.svg")
qore_user_module("qlib/FtpClientDataProvider" "ftp-logo-white.svg;ftp-logo-black.svg")
qore_user_module("qlib/HttpClientDataProvider")
qore_user_module("qlib/ServerSentEventHandler")
qore_user_module("qlib/SqlUtil" "../../doxygen/SqlUtil-Full.svg")
qore_user_module("qlib/ConnectionProvider")
qore_user_module("qlib/CsvUtil")
qore_user_module("qlib/FixedLengthUtil")
qore_user_module("qlib/Mapper.qm")
qore_user_module("qlib/FtpPollerUtil.qm")
qore_user_module("qlib/BulkSqlUtil")
qore_user_module("qlib/Schema")
qore_user_module("qlib/TelnetClient.qm")
qore_user_module("qlib/TerminalFormat")
qore_user_module("qlib/DebugCmdLine.qm")
qore_user_module("qlib/DiscordWebSocketClient.qm")
qore_user_module("qlib/HttpServer.qm")
qore_user_module("qlib/HttpServerAsyncIo")
qore_user_module("qlib/FtpClientIo")
qore_user_module("qlib/HttpClientIo")
qore_user_module("qlib/HttpClientStreamingIo")
qore_user_module("qlib/ImapClient")
qore_user_module("qlib/Pop3Client.qm")
qore_user_module("qlib/RestSchemaValidator.qm")
qore_user_module("qlib/SewioWebSocketClient.qm")
qore_user_module("qlib/SmtpClient.qm")
qore_user_module("qlib/WebSocketHandler.qm")
qore_user_module("qlib/WebUtil.qm")

# base 5 modules
qore_user_module("qlib/ProviderIndex")
qore_user_module("qlib/ServerSentEventClient")
qore_user_module("qlib/MssqlSqlUtilBase.qm")
qore_user_module("qlib/MysqlSqlUtil.qm")
qore_user_module("qlib/OracleSqlUtilBase.qm")
qore_user_module("qlib/PgsqlSqlUtilBase.qm")
qore_user_module("qlib/Sqlite3SqlUtil.qm")
qore_user_module("qlib/XdbcFirebirdSqlUtilBase.qm")
qore_user_module("qlib/WebSocketClient.qm")
qore_user_module("qlib/TableMapper.qm")
qore_user_module("qlib/DbDataProvider")
qore_user_module("qlib/FtpPoller.qm")
qore_user_module("qlib/SchemaReverse.qm")
qore_user_module("qlib/DebugHandler.qm")
qore_user_module("qlib/DebugLinenoiseCmdLine.qm")
qore_user_module("qlib/AsyncApi.qm")
qore_user_module("qlib/OpenApi3.qm")
qore_user_module("qlib/RestHandler.qm")
qore_user_module("qlib/Swagger.qm")
qore_user_module("qlib/VscDebugAdapter.qm")
qore_user_module("qlib/ImapClientDataProvider" "imap-logo-white.svg")
qore_user_module("qlib/Pop3ClientDataProvider" "pop3-logo-white.svg")

# base 6 modules
qore_user_module("qlib/FreetdsSqlUtil.qm")
qore_user_module("qlib/JdbcMicrosoftSqlUtil.qm")
qore_user_module("qlib/OracleSqlUtil.qm")
qore_user_module("qlib/JdbcOracleSqlUtil.qm")
qore_user_module("qlib/PgsqlSqlUtil.qm")
qore_user_module("qlib/JdbcPostgresqlSqlUtil.qm")
qore_user_module("qlib/JdbcFirebirdSqlUtil.qm")
qore_user_module("qlib/OdbcFirebirdSqlUtil.qm")
qore_user_module("qlib/RestClient.qm")
qore_user_module("qlib/RestClientIo.qm")


# base 7 modules
qore_user_module("qlib/SalesforceRestClient.qm")
qore_user_module("qlib/SewioRestClient.qm")
qore_user_module("qlib/ZeyosRestClient.qm")
qore_user_module("qlib/BigCommerceRestClient.qm")
qore_user_module("qlib/BillwerkRestClient.qm")
qore_user_module("qlib/BitlyRestClient.qm")
qore_user_module("qlib/CircleRestClient.qm")
qore_user_module("qlib/InsightlyRestClient.qm")
qore_user_module("qlib/SmartSheetRestClient.qm")
qore_user_module("qlib/GorgiasRestClient.qm")
qore_user_module("qlib/Sap4HanaRestClient.qm")
qore_user_module("qlib/CdsRestClient.qm")
qore_user_module("qlib/ChargebeeRestClient.qm")
qore_user_module("qlib/CloseRestClient.qm")
qore_user_module("qlib/DiscordRestClient.qm")
qore_user_module("qlib/EasyPostRestClient.qm")
qore_user_module("qlib/ElevenLabsRestClient.qm")
qore_user_module("qlib/FirecrawlRestClient.qm")
qore_user_module("qlib/FreshBooksRestClient.qm")
qore_user_module("qlib/FreshsalesRestClient.qm")
qore_user_module("qlib/GeminiRestClient.qm")
qore_user_module("qlib/GraphQL")
qore_user_module("qlib/GraphQLClient.qm")
qore_user_module("qlib/GraphQLDataProvider")
qore_user_module("qlib/GraphQLHandler")
qore_user_module("qlib/LinearRestClient.qm")
qore_user_module("qlib/WaveRestClient.qm")
qore_user_module("qlib/WooCommerceRestClient.qm")
qore_user_module("qlib/GoogleRestClient.qm")
qore_user_module("qlib/HueRestClient.qm")
qore_user_module("qlib/JotformRestClient.qm")
qore_user_module("qlib/MailgunRestClient.qm")
qore_user_module("qlib/MewsRestClient.qm")
qore_user_module("qlib/NetSuiteRestClient.qm")
qore_user_module("qlib/AftershipRestClient.qm")
qore_user_module("qlib/BaseLinkerRestClient.qm")
qore_user_module("qlib/AnthropicRestClient.qm")
qore_user_module("qlib/VoyageRestClient.qm")
qore_user_module("qlib/SGLangRestClient.qm")
qore_user_module("qlib/BgeM3RestClient.qm")
qore_user_module("qlib/TallyRestClient.qm")
qore_user_module("qlib/OneDriveRestClient.qm")
qore_user_module("qlib/GrokRestClient.qm")
qore_user_module("qlib/HuggingFaceRestClient.qm")
qore_user_module("qlib/OpenAiRestClient.qm")
qore_user_module("qlib/PerplexityRestClient.qm")
qore_user_module("qlib/SegmentRestClient.qm")
qore_user_module("qlib/ServiceNowRestClient.qm")
qore_user_module("qlib/LemlistRestClient.qm")
qore_user_module("qlib/ShipStationRestClient.qm")
qore_user_module("qlib/SendCloudRestClient.qm")
qore_user_module("qlib/SystemeIoRestClient.qm")
qore_user_module("qlib/CustomerIoRestClient.qm")
qore_user_module("qlib/InstantlyRestClient.qm")
qore_user_module("qlib/ShippoRestClient.qm")
qore_user_module("qlib/DeelRestClient.qm")
qore_user_module("qlib/GoHighLevelRestClient.qm")
qore_user_module("qlib/PdfCoRestClient.qm")
qore_user_module("qlib/ApolloRestClient.qm")
qore_user_module("qlib/KeapRestClient.qm")
qore_user_module("qlib/KitRestClient.qm")
qore_user_module("qlib/ConstantContactRestClient.qm")
qore_user_module("qlib/SquareRestClient.qm")
qore_user_module("qlib/SquarespaceRestClient.qm")
qore_user_module("qlib/TableauRestClient.qm")
qore_user_module("qlib/Cin7CoreRestClient.qm")
qore_user_module("qlib/ClickFunnelsRestClient.qm")
qore_user_module("qlib/UnbounceRestClient.qm")
qore_user_module("qlib/UnleashedRestClient.qm")
qore_user_module("qlib/KatanaRestClient.qm")
qore_user_module("qlib/ZohoBooksRestClient.qm")
qore_user_module("qlib/ZohoInventoryRestClient.qm")
qore_user_module("qlib/ZohoInvoiceRestClient.qm")
qore_user_module("qlib/AwsRestClient.qm")
qore_user_module("qlib/AwsEventStream.qm")
qore_user_module("qlib/AzureAuth.qm")
qore_user_module("qlib/AzureOpenAiRestClient.qm")
qore_user_module("qlib/AzureOpenAiDataProvider" "azure-logo.svg")
qore_user_module("qlib/BedrockRestClient.qm")
qore_user_module("qlib/BedrockDataProvider" "bedrock-logo.svg")
qore_user_module("qlib/VertexAiRestClient.qm")
qore_user_module("qlib/VertexAiDataProvider" "vertex-ai-logo.svg")
qore_user_module("qlib/DeepSeekRestClient.qm")
qore_user_module("qlib/DeepSeekDataProvider" "deepseek-logo.svg")
qore_user_module("qlib/DeepgramRestClient.qm")
qore_user_module("qlib/DeepgramDataProvider" "deepgram-logo.svg")
qore_user_module("qlib/AssemblyAiRestClient.qm")
qore_user_module("qlib/AssemblyAiDataProvider" "assemblyai-logo.svg")
qore_user_module("qlib/AwsTextractRestClient.qm")
qore_user_module("qlib/AwsTextractDataProvider" "aws-textract-logo.svg")
qore_user_module("qlib/GoogleDocumentAiRestClient.qm")
qore_user_module("qlib/GoogleDocumentAiDataProvider" "google-document-ai-logo.svg")
qore_user_module("qlib/AzureDocumentIntelligenceRestClient.qm")
qore_user_module("qlib/AzureDocumentIntelligenceDataProvider" "azure-document-intelligence-logo.svg")
qore_user_module("qlib/StabilityAiRestClient.qm")
qore_user_module("qlib/StabilityAiDataProvider" "stability-ai-logo.svg")
qore_user_module("qlib/BflRestClient.qm")
qore_user_module("qlib/BflDataProvider" "bfl-logo.svg")
qore_user_module("qlib/GroqRestClient.qm")
qore_user_module("qlib/GroqDataProvider" "groq-logo.svg")
qore_user_module("qlib/TogetherRestClient.qm")
qore_user_module("qlib/TogetherDataProvider" "together-ai-logo.svg")
qore_user_module("qlib/FireworksRestClient.qm")
qore_user_module("qlib/FireworksDataProvider" "fireworks-ai-logo.svg")
qore_user_module("qlib/OllamaRestClient.qm")
qore_user_module("qlib/OllamaDataProvider" "ollama-logo.svg")
qore_user_module("qlib/JinaRestClient.qm")
qore_user_module("qlib/JinaDataProvider" "jina-logo.svg")
qore_user_module("qlib/CohereRestClient.qm")
qore_user_module("qlib/CohereDataProvider" "cohere-logo.svg")
qore_user_module("qlib/MistralRestClient.qm")
qore_user_module("qlib/MistralDataProvider" "mistral-ai-logo.svg")
qore_user_module("qlib/ChromaRestClient.qm")
qore_user_module("qlib/ChromaDataProvider" "chroma-logo.svg")
qore_user_module("qlib/PineconeRestClient.qm")
qore_user_module("qlib/PineconeDataProvider" "pinecone-logo.svg")
qore_user_module("qlib/WeaviateRestClient.qm")
qore_user_module("qlib/WeaviateDataProvider" "weaviate-logo.svg")
qore_user_module("qlib/MilvusRestClient.qm")
qore_user_module("qlib/MilvusDataProvider" "milvus-logo.svg")
qore_user_module("qlib/TavilyRestClient.qm")
qore_user_module("qlib/TavilyDataProvider" "tavily-logo.svg")
qore_user_module("qlib/ExaRestClient.qm")
qore_user_module("qlib/ExaDataProvider" "exa-logo.svg")
qore_user_module("qlib/LlamaParseRestClient.qm")
qore_user_module("qlib/LlamaParseDataProvider" "llamaparse-logo.svg")
qore_user_module("qlib/UnstructuredRestClient.qm")
qore_user_module("qlib/UnstructuredDataProvider" "unstructured-logo.svg")
qore_user_module("qlib/GcpAuth.qm")
qore_user_module("qlib/GeminiBodyShape.qm")
qore_user_module("qlib/SwaggerDataProvider")
qore_user_module("qlib/OpenApi3DataProvider")
qore_user_module("qlib/AsyncApiDataProvider")
qore_user_module("qlib/RestSchemaDataProvider")
qore_user_module("qlib/BigCommerceDataProvider" "bigcommerce-logo.svg")
qore_user_module("qlib/BitlyDataProvider" "bitly-logo.svg")
qore_user_module("qlib/CircleDataProvider" "circle-logo.svg")
qore_user_module("qlib/InsightlyDataProvider" "insightly-logo.svg")
qore_user_module("qlib/SmartSheetDataProvider" "smartsheet-logo.svg")
qore_user_module("qlib/GorgiasDataProvider" "gorgias-logo.svg")
qore_user_module("qlib/CdsRestDataProvider" "dynamics-logo.svg;business-central-logo.svg")
qore_user_module("qlib/ServiceNowRestDataProvider" "servicenow-logo-white.svg;servicenow-logo-black.svg")
qore_user_module("qlib/RestClientDataProvider" "webhook-logo.svg")
qore_user_module("qlib/SearchIndexDataProvider")
qore_user_module("qlib/ElasticSearchDataProvider" "elastic-logo.svg")
qore_user_module("qlib/OpenSearchDataProvider" "opensearch-logo.svg")
qore_user_module("qlib/QdrantDataProvider" "qdrant-logo.svg")
qore_user_module("qlib/EmpathicBuildingDataProvider" "Haltian-EmpathicBuilding.svg")
qore_user_module("qlib/GeneratorDataProvider" "generator-logo.svg")
qore_user_module("qlib/MemcachedDataProvider" "memcached-logo.svg")
qore_user_module("qlib/MongoDbDataProvider" "mongodb-logo.svg")
qore_user_module("qlib/RedisDataProvider" "redis-logo.svg")

# base 8 modules
qore_user_module("qlib/ElasticSearchLoggerAppender.qm")
qore_user_module("qlib/OpenSearchLoggerAppender" "opensearch-logo.svg")
qore_user_module("qlib/SalesforceRestDataProvider" "salesforce-logo.svg")
qore_user_module("qlib/AwsRestClientDataProvider")
qore_user_module("qlib/AwsS3DataProvider" "amazon-s3-logo.svg")
qore_user_module("qlib/GoogleDataProvider")
qore_user_module("qlib/DiscordDataProvider" "discord-logo.svg")
qore_user_module("qlib/EasyPostDataProvider" "easypost-logo.svg")
qore_user_module("qlib/ElevenLabsDataProvider" "elevenlabs-logo.svg")
qore_user_module("qlib/FirecrawlDataProvider" "firecrawl-logo.svg")
qore_user_module("qlib/FreshBooksDataProvider" "freshbooks-logo.svg")
qore_user_module("qlib/FreshsalesDataProvider" "freshsales-logo.svg")
qore_user_module("qlib/GeminiDataProvider" "gemini-logo.svg")
qore_user_module("qlib/LinearDataProvider" "linear-logo.svg")
qore_user_module("qlib/WaveDataProvider" "wave-logo.svg")
qore_user_module("qlib/WooCommerceDataProvider" "woocommerce-logo.svg")
qore_user_module("qlib/JotformDataProvider" "jotform-logo.svg")
qore_user_module("qlib/MailgunDataProvider" "mailgun-logo.svg")
qore_user_module("qlib/MewsRestDataProvider" "../../doxygen/mews-logo-white.svg")
qore_user_module("qlib/AftershipDataProvider" "aftership-logo.svg")
qore_user_module("qlib/BaseLinkerDataProvider" "baselinker-logo.svg")
qore_user_module("qlib/AnthropicDataProvider" "anthropic-logo.svg")
qore_user_module("qlib/VoyageDataProvider" "voyage-logo.svg")
qore_user_module("qlib/SGLangDataProvider" "sglang-logo.svg")
qore_user_module("qlib/BgeM3FastApiDataProvider" "bge-m3-logo.svg")
qore_user_module("qlib/TallyDataProvider" "tally-logo.svg")
qore_user_module("qlib/OneDriveDataProvider" "onedrive-logo.svg")
qore_user_module("qlib/GrokDataProvider" "grok-logo.svg")
qore_user_module("qlib/HuggingFaceDataProvider" "huggingface-logo.svg")
qore_user_module("qlib/OpenAiDataProvider" "openai-logo.svg")
qore_user_module("qlib/PerplexityDataProvider" "perplexity-logo.svg")
qore_user_module("qlib/SegmentDataProvider" "segment-logo.svg")

# base 9 modules
qore_user_module("qlib/GoogleCalendarDataProvider" "google-calendar-logo.svg")
qore_user_module("qlib/GoogleDriveDataProvider" "google-drive-logo.svg")
qore_user_module("qlib/GmailDataProvider" "gmail-logo.svg")
qore_user_module("qlib/ZohoBooksDataProvider" "zohobooks-logo.svg")
qore_user_module("qlib/ZohoInventoryDataProvider" "zohoinventory-logo.svg")
qore_user_module("qlib/ZohoInvoiceDataProvider" "zohoinvoice-logo.svg")
qore_user_module("qlib/LemlistDataProvider" "lemlist-logo.svg")
qore_user_module("qlib/ShipStationDataProvider" "shipstation-logo.svg")
qore_user_module("qlib/SendCloudDataProvider" "sendcloud-logo.svg")
qore_user_module("qlib/SystemeIoDataProvider" "systemeio-logo.svg")
qore_user_module("qlib/CustomerIoDataProvider" "customerio-logo.svg")
qore_user_module("qlib/InstantlyDataProvider" "instantly-logo.svg")
qore_user_module("qlib/ShippoDataProvider" "shippo-logo.svg")
qore_user_module("qlib/DeelDataProvider" "deel-logo.svg")
qore_user_module("qlib/GoHighLevelDataProvider" "gohighlevel-logo.svg")
qore_user_module("qlib/PdfCoDataProvider" "pdfco-logo.svg")
qore_user_module("qlib/ApolloDataProvider" "apollo-logo.svg")
qore_user_module("qlib/KeapDataProvider" "keap-logo.svg")
qore_user_module("qlib/KitDataProvider" "kit-logo.svg")
qore_user_module("qlib/ConstantContactDataProvider" "constantcontact-logo.svg")
qore_user_module("qlib/SquareDataProvider" "square-logo.svg")
qore_user_module("qlib/ChargebeeDataProvider" "chargebee-logo.svg")
qore_user_module("qlib/CloseDataProvider" "close-logo.svg")
qore_user_module("qlib/SquarespaceDataProvider" "squarespace-logo.svg")
qore_user_module("qlib/TableauDataProvider" "tableau-logo.svg")
qore_user_module("qlib/Cin7CoreDataProvider" "cin7core-logo.svg")
qore_user_module("qlib/ClickFunnelsDataProvider" "clickfunnels-logo.svg")
qore_user_module("qlib/UnbounceDataProvider" "unbounce-logo.svg")
qore_user_module("qlib/UnleashedDataProvider" "unleashed-logo.svg")
qore_user_module("qlib/KatanaDataProvider" "katana-logo.svg")

# modules migrated from module-json (issue #5366); build order is derived from each module's
# own %requires directives by qore_finalize_user_module_dependencies() below
qore_user_module("qlib/A2aClient")
qore_user_module("qlib/A2aClientDataProvider" "a2a-logo.svg")
qore_user_module("qlib/A2aServerHandler")
qore_user_module("qlib/FhirRestClient")
qore_user_module("qlib/FhirRestDataProvider" "fhir-logo.svg")
qore_user_module("qlib/JsonFileDataProvider" "json-file-logo.svg")
qore_user_module("qlib/AgUi")
qore_user_module("qlib/JsonLd")
qore_user_module("qlib/JsonPatch.qm")
qore_user_module("qlib/JsonRpcClientIo")
qore_user_module("qlib/JsonRpcConnection.qm")
qore_user_module("qlib/JsonRpcHandler.qm")
qore_user_module("qlib/McpClient")
qore_user_module("qlib/McpClientDataProvider")
qore_user_module("qlib/McpServerHandler")
qore_user_module("qlib/NdjsonDataProvider")
qore_user_module("qlib/OneRecordClient")
qore_user_module("qlib/OneRecordServerHandler")

qore_finalize_user_module_dependencies()

get_property(QORE_QM_METADATA_TARGETS GLOBAL PROPERTY QORE_QM_METADATA_TARGETS)
add_custom_target(qore-bootstrap
    DEPENDS
        qore
        qcc
        qpp
        libqore
        ${QORE_AOT_BINARY_MODULE_TARGETS}
        ${QORE_QM_METADATA_TARGETS})
add_custom_target(install-qore-bootstrap
    COMMAND ${CMAKE_COMMAND}
        --install ${CMAKE_BINARY_DIR}
        --component ${QORE_INSTALL_COMPONENT_BOOTSTRAP}
    DEPENDS qore-bootstrap
    COMMENT "Installing Qore bootstrap component without qlib AOT qmods"
    USES_TERMINAL
    VERBATIM)

add_custom_target(qore-qlib-aot)
if (QORE_BUILD_AOT_MODULES)
    get_property(_qore_user_modules GLOBAL PROPERTY QORE_USER_MODULE_TARGETS)
    get_property(_qore_external_user_modules GLOBAL PROPERTY QORE_EXTERNAL_USER_MODULE_TARGETS)
    foreach(_qore_qlib_aot_module ${_qore_user_modules} ${_qore_external_user_modules})
        if (TARGET ${_qore_qlib_aot_module}-qmod)
            add_dependencies(qore-qlib-aot ${_qore_qlib_aot_module}-qmod)
        endif()
    endforeach()
endif()
add_custom_target(install-qore-qlib-aot
    COMMAND ${CMAKE_COMMAND}
        --install ${CMAKE_BINARY_DIR}
        --component ${QORE_INSTALL_COMPONENT_QLIB_AOT}
    DEPENDS qore-qlib-aot
    COMMENT "Installing Qore qlib AOT qmods"
    USES_TERMINAL
    VERBATIM)

# Self-healing guarantee for the in-tree source qmod symlinks.  The per-qmod
# create_symlink command only recreates a symlink when its qmod target rebuilds,
# but the qcc-format-change cleanup (RemoveSourceQmodSymlinks.cmake) removes the
# symlinks unconditionally — so a partial build, or up-to-date qmods, after a
# format-changing change can leave a module without its symlink and silently fall
# back to slow source parsing at load.  This idempotent step recreates any
# missing/incorrect symlink from the already-built qmods, both on (re)configure
# and as an always-run build step after all qmod targets.
get_property(_qore_source_qmod_link_targets GLOBAL PROPERTY QORE_SOURCE_QMOD_LINK_TARGETS)
if (QORE_BUILD_AOT_MODULES AND QORE_AOT_LINK_SOURCE_MODULES
        AND _qore_source_qmod_link_targets)
    add_custom_target(qore-ensure-source-qmod-symlinks ALL
        COMMAND ${CMAKE_COMMAND}
            -DQORE_SOURCE_QLIB_DIR=${CMAKE_SOURCE_DIR}/qlib
            -DQORE_BUILD_QMOD_DIR=${CMAKE_BINARY_DIR}/qlib-qmod
            -P ${CMAKE_SOURCE_DIR}/cmake/EnsureSourceQmodSymlinks.cmake
        COMMENT "Ensuring in-tree source qmod symlinks"
        VERBATIM)
    add_dependencies(qore-ensure-source-qmod-symlinks ${_qore_source_qmod_link_targets})
    # Also heal on (re)configure so an existing build tree is corrected even
    # before the next build runs.
    if (IS_DIRECTORY ${CMAKE_BINARY_DIR}/qlib-qmod)
        execute_process(COMMAND ${CMAKE_COMMAND}
            -DQORE_SOURCE_QLIB_DIR=${CMAKE_SOURCE_DIR}/qlib
            -DQORE_BUILD_QMOD_DIR=${CMAKE_BINARY_DIR}/qlib-qmod
            -P ${CMAKE_SOURCE_DIR}/cmake/EnsureSourceQmodSymlinks.cmake
            RESULT_VARIABLE _qore_ensure_qmod_links_result)
        if (NOT _qore_ensure_qmod_links_result EQUAL 0)
            message(FATAL_ERROR "Could not ensure in-tree source qmod symlinks")
        endif()
    endif()
endif()

# Configure lang Doxyfile final pass with module tag files after all modules are defined
    if (DOXYGEN_FOUND)
        # Build TAGFILES string from collected module names
        set(QORE_LANG_DOC_TAGFILES "")
    foreach(mod ${QORE_USER_MODULE_NAMES})
        # Tag file is generated in build root as {mod}.tag
        # Relative path to module html from lang html: ../../modules/{mod}/html
        set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/${mod}.tag=../../modules/${mod}/html\"")
    endforeach()
    # Add binary module tag files required for lang cross-references.
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/astparser/astparser.tag=../../modules/astparser/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/reflection/reflection.tag=../../modules/reflection/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/logger_bin/logger_bin.tag=../../modules/logger_bin/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/linenoise/linenoise.tag=../../modules/linenoise/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/krb5/krb5.tag=../../modules/krb5/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/ml/ml.tag=../../modules/ml/html\"")
    set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/sshutil/sshutil.tag=../../modules/sshutil/html\"")
    if (MONGOC_FOUND)
        set(QORE_LANG_DOC_TAGFILES "${QORE_LANG_DOC_TAGFILES} \"${CMAKE_BINARY_DIR}/modules/mongodb/mongodb.tag=../../modules/mongodb/html\"")
    endif()
    set(QORE_LANG_DOC_PREDEFINED "QORE_LANG_FINAL" CACHE INTERNAL "Preprocessor definitions for lang docs (final pass)")
    set(QORE_LANG_DOC_SECTIONS "QORE_LANG_FINAL" CACHE INTERNAL "Enabled sections for lang docs (final pass)")
    message(STATUS "Configuring Doxyfile for lang final pass with module cross-references")
    configure_file(${CMAKE_SOURCE_DIR}/doxygen/lang/Doxyfile.in ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.final @ONLY)

    # docs-lang-final: rebuilds lang docs after all modules are built, with module tagfiles
    # This enables @ref links from lang docs to module symbols
    add_custom_target(docs-lang-final
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.final
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/*.js
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.js
        COMMENT "Generating API documentation with Doxygen for: lang (final pass with module cross-references)"
        VERBATIM
    )
    add_dependencies(docs-lang-final qore)

    # docs-lang-final-fast: same as docs-lang-final but without module dependencies
    # Use this for quick testing when module docs are already built manually
    add_custom_target(docs-lang-final-fast
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doxygen/Doxyfile.lang.final
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/*.js
        COMMAND ${QORE_DOCS_ENV} ${QORE_QDX_COMMAND} --post ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.html ${CMAKE_BINARY_DIR}/docs/lang/html/search/*.js
        COMMENT "Generating API documentation with Doxygen for: lang (final pass, no module deps)"
        VERBATIM
    )
    add_dependencies(docs-lang-final-fast qore)

    # docs-lang-final depends on all module docs (so their tagfiles exist)
    foreach(mod ${QORE_USER_MODULE_NAMES})
        add_dependencies(docs-lang-final docs-${mod})
    endforeach()
    # Binary module doc dependencies
    add_dependencies(docs-lang-final docs-astparser docs-reflection docs-logger_bin docs-linenoise docs-krb5 docs-ml docs-sshutil)
    if (MONGOC_FOUND)
        add_dependencies(docs-lang-final docs-mongodb)
    endif()

    # docs depends on docs-lang-final (the final lang docs with module cross-refs)
    add_dependencies(docs docs-lang-final)
endif()

# print info
message(STATUS "")
message(STATUS "Implicit link libraries and flags detected for C++: ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES}")
qore_config_info()

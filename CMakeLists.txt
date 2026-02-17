cmake_minimum_required(VERSION 3.5)

# NOTE: Qore should always use semantic versioning: https://semver.org/
set(VERSION_MAJOR 2)
set(VERSION_MINOR 3)
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

option(ICONV_TRANSLIT
       "Force //Translit to iconv encoding to do transliteration, if OFF it is tested for"
       OFF)

option(SINGLE_COMPILATION_UNIT
       "Build libqore as a single compilation unit for faster builds"
       ON)

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
else ()
    add_definitions(-DNDEBUG)
endif ()

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_BINARY_DIR}/include/)
include_directories(BEFORE ${CMAKE_BINARY_DIR})

# for add the source dir include for cmake runs that refuse to add it in some environments for an unknown reason
set(CMAKE_CXX_FLAGS "-I${CMAKE_SOURCE_DIR}/include")

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
    lib/QC_RangeIterator.qpp
    lib/QC_Datasource.qpp
    lib/QC_DatasourcePool.qpp
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
    lib/QC_EventLoop.qpp
    lib/QC_EventNotifier.qpp
    lib/ql_misc.qpp
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
    lib/Pseudo_QC_Bool.qpp
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
    bin/schema-reverse
    bin/sfrest
    bin/saprest
    bin/sqlutil
    bin/qdbg
    bin/qdbg-server
    bin/qdbg-remote
    bin/qdbg-vsc-adapter
    bin/qore-openapi3-gen
    bin/qore-asyncapi-gen
    bin/qfmt
    bin/qls
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
    doxygen/lang/225_exception_handling.dox.tmpl
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
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(MPFR REQUIRED)
find_package(BZip2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

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
        list(APPEND LIBQORE_OPTIONAL_INCLUDES ${BROTLI_INCLUDE_DIR})
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
    message(STATUS "Found Zstandard: ${ZSTD_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_LIBS ${ZSTD_LIBRARIES})
    list(APPEND LIBQORE_OPTIONAL_INCLUDE_DIRS ${ZSTD_INCLUDE_DIRS})
else()
    # Try to find Zstd without pkg-config
    find_path(ZSTD_INCLUDE_DIR zstd.h)
    find_library(ZSTD_LIBRARY NAMES zstd)
    if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
        message(STATUS "Found Zstandard: ${ZSTD_LIBRARY}")
        list(APPEND LIBQORE_OPTIONAL_INCLUDES ${ZSTD_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${ZSTD_LIBRARY})
    else()
        message(FATAL_ERROR "Zstandard is required for compression support. Please install libzstd-dev (Debian/Ubuntu) or libzstd-devel (Fedora/RHEL)")
    endif()
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
        list(APPEND LIBQORE_OPTIONAL_INCLUDES ${LZ4_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${LZ4_LIBRARY})
    else()
        message(FATAL_ERROR "LZ4 is required for compression support. Please install liblz4-dev (Debian/Ubuntu) or lz4-devel (Fedora/RHEL)")
    endif()
endif()
# Always defined since lz4 is now required
set(HAVE_LZ4 TRUE)

if (NOT (WIN32 OR MINGW OR MSYS))
    find_package(Backtrace)
    if (Backtrace_FOUND)
        list(APPEND LIBQORE_OPTIONAL_INCLUDES ${Backtrace_INCLUDE_DIRS})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${Backtrace_LIBRARIES})
    endif (Backtrace_FOUND)
endif (NOT (WIN32 OR MINGW OR MSYS))

# HTTP/2 support via nghttp2 (required)
pkg_check_modules(NGHTTP2 QUIET libnghttp2)
if (NGHTTP2_FOUND)
    message(STATUS "Found nghttp2: ${NGHTTP2_VERSION}")
    list(APPEND LIBQORE_OPTIONAL_INCLUDES ${NGHTTP2_INCLUDE_DIRS})
    list(APPEND LIBQORE_OPTIONAL_LIBS ${NGHTTP2_LIBRARIES})
else()
    # Try to find nghttp2 without pkg-config
    find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h)
    find_library(NGHTTP2_LIBRARY NAMES nghttp2)
    if (NGHTTP2_INCLUDE_DIR AND NGHTTP2_LIBRARY)
        message(STATUS "Found nghttp2: ${NGHTTP2_LIBRARY}")
        list(APPEND LIBQORE_OPTIONAL_INCLUDES ${NGHTTP2_INCLUDE_DIR})
        list(APPEND LIBQORE_OPTIONAL_LIBS ${NGHTTP2_LIBRARY})
    else()
        message(FATAL_ERROR "nghttp2 is required for HTTP/2 support. Please install libnghttp2-dev (Debian/Ubuntu) or libnghttp2-devel (Fedora/RHEL)")
    endif()
endif()

if (WIN32 OR MINGW OR MSYS)
    SET(CMAKE_DL_LIBS dl)
    SET(MPFR_LIBRARIES mpfr gmp)
    SET(OPENSSL_LIBRARIES ssl crypto)
    SET(WINSOCKET_LIBRARIES ws2_32)
    SET(NO_SIGNAL_HANDLING ON)
endif (WIN32 OR MINGW OR MSYS)

create_git_revision()

# check for C++17
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(COMPILER_SUPPORTS_CXX17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    set(HAVE_CXX17 true)
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a different C++ compiler.")
endif()

# in case we want to the jni module, we need to link with libjsig if available
find_package(JNI)
if (JNI_FOUND AND DEFINED JAVA_JVM_LIBRARY AND NOT DEFINED ENV{QORE_WITHOUT_JSIG})
    get_filename_component(JAVA_LIB_DIR ${JAVA_JVM_LIBRARY} DIRECTORY)
    set(LIBJSIG "${JAVA_LIB_DIR}/libjsig${CMAKE_SHARED_LIBRARY_SUFFIX}")
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
bison_target(qoreparser lib/parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
flex_target(qorescanner lib/scanner.lpp ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp COMPILE_FLAGS --nowarn)
add_flex_bison_dependency(qorescanner qoreparser)

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

# libqore generators
qore_wrap_qpp_value(LIBQORE_QPP_CPP_SRC DOXLIST QPP_DOX_SRC METALIST QPP_META_SRC ${LIBQORE_QPP_SRC})
qore_wrap_qpp_value(PSEUDO_QPP_CPP_SRC DOXLIST QPP_DOX_SRC METALIST QPP_META_SRC ${PSEUDO_QPP_SRC})

add_custom_target(QPP_GENERATED_FILES DEPENDS ${LIBQORE_QPP_CPP_SRC} ${PSEUDO_QPP_CPP_SRC})

# install qpp metadata files
install(FILES ${QPP_META_SRC} DESTINATION ${CMAKE_INSTALL_DATADIR}/qore/metadata)
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
    lib/QoreEventLoop.cpp
    lib/QoreEventNotifier.cpp
    lib/QoreSandboxManager.cpp
    lib/DateTime.cpp
    lib/QoreLib.cpp
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
    lib/DatasourcePool.cpp
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
    lib/SystemEnvironment.cpp
    lib/QoreCounter.cpp
    lib/ReferenceArgumentHelper.cpp
    lib/ReferenceHelper.cpp
    lib/QoreTypeInfo.cpp
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
# Non-blocking I/O support for streams (used by HTTP/2 streaming to avoid buffering large responses in memory)
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/InputStream.cpp
    lib/OutputStream.cpp
)

# HTTP/2 support is always enabled (nghttp2 is required)
set(LIBQORE_CPP_SRC ${LIBQORE_CPP_SRC}
    lib/Http2Session.cpp
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
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp
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
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEventLoop.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreEventNotifier.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreSandboxManager.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/DateTime.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreLib.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTimeZoneManager.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreString.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreObject.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RSet.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/RSection.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreListNode.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/qore-main.cpp\"
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
#include \"${CMAKE_SOURCE_DIR}/lib/DatasourcePool.cpp\"
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
#include \"${CMAKE_SOURCE_DIR}/lib/SystemEnvironment.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreCounter.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReferenceArgumentHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/ReferenceHelper.cpp\"
#include \"${CMAKE_SOURCE_DIR}/lib/QoreTypeInfo.cpp\"
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
#include \"${CMAKE_BINARY_DIR}/QC_ThreadPool.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractDatasource.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_AbstractSQLStatement.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Datasource.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_DatasourcePool.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SQLStatement.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Queue.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Mutex.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Condition.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_RWLock.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_SandboxManager.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Gate.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Sequence.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_Counter.cpp\"
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
#include \"${CMAKE_BINARY_DIR}/QC_EventNotifier.cpp\"
#include \"${CMAKE_BINARY_DIR}/QC_EventLoop.cpp\"

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
")
    add_library(libqore SHARED ${CMAKE_CURRENT_BINARY_DIR}/lib/single-compilation-unit.cpp ${LIBQORE_SOURCE_HEADERS})
else (SINGLE_COMPILATION_UNIT)
    add_library(libqore SHARED ${LIBQORE_QPP_CPP_SRC} ${LIBQORE_CPP_SRC} ${LIBQORE_ASM_SRC} ${LIBQORE_SOURCE_HEADERS})
endif (SINGLE_COMPILATION_UNIT)

add_dependencies(QPP_GENERATED_FILES qpp)
add_dependencies(libqore QPP_GENERATED_FILES BISON_GENERATED_FILES FLEX_GENERATED_FILES)

target_compile_definitions(libqore PUBLIC _QORE_LIB_INTERN HAVE_UNIX_CONFIG_H)
target_compile_definitions(libqore PRIVATE MODULE_DIR="${MODULE_DIR}")
target_compile_definitions(libqore PRIVATE MODULE_VER_DIR="${MODULE_VER_DIR}")
target_compile_definitions(libqore PRIVATE USER_MODULE_DIR="${USER_MODULE_DIR}")
target_compile_definitions(libqore PRIVATE USER_MODULE_VER_DIR="${USER_MODULE_VER_DIR}")
if (WIN32 OR MINGW OR MSYS)
    target_compile_definitions(libqore PUBLIC BUILDING_DLL)
endif (WIN32 OR MINGW OR MSYS)

# Include CMAKE_BINARY_DIR first so QPP-generated files take precedence over autotools-generated ones
target_include_directories(libqore PUBLIC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/include ${BZIP2_INCLUDE_DIR} ${ICONV_INCLUDE_DIR} ${MPFR_INCLUDE_DIRS} ${LIBQORE_OPTIONAL_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${PCRE2_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

set(QORE_LIBS ${LIBJSIG} ${BZIP2_LIBRARIES} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${ICONV_LIBRARY} ${LIBQORE_LIBS} ${LIBQORE_OPTIONAL_LIBS} ${MPFR_LIBRARIES} ${OPENSSL_LIBRARIES} ${PCRE2_LDFLAGS} ${ZLIB_LIBRARIES})
string(REPLACE ";" " " CMAKE_LDFLAGS "${QORE_LIBS}")
string(STRIP "${CMAKE_LDFLAGS}" CMAKE_LDFLAGS)
string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
target_link_libraries(libqore ${QORE_LIBS})
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

# Ensure the installed qore binary can resolve @rpath/libqore.20.dylib.
set_target_properties(qore PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
set_target_properties(qore PROPERTIES BUILD_RPATH ${CMAKE_BINARY_DIR})

if (DEFINED LIBJSIG AND APPLE)
    add_custom_command(TARGET qore
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -change @rpath/libjsig.dylib ${LIBJSIG} qore
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -change @rpath/libjsig.dylib ${LIBJSIG} libqore.dylib
    )
endif()

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
        DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

install(TARGETS qore qpp libqore
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
#install(TARGETS qore qpp qr libqore
#        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
#        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
install(FILES ${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake ${CMAKE_BINARY_DIR}/cmake/QoreConfigVersion.cmake cmake/QoreMacros.cmake
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Qore)
install(FILES ${CMAKE_BINARY_DIR}/qore.pc
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig)
install(DIRECTORY include/qore
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
        PATTERN intern EXCLUDE
        PATTERN minitest.hpp EXCLUDE
        PATTERN macros-none.h EXCLUDE
        PATTERN hash_map_include.h EXCLUDE
        PATTERN slist_include.h EXCLUDE
        PATTERN qore-version.h EXCLUDE)
install(FILES ${CMAKE_BINARY_DIR}/include/qore/qore-version.h
        DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/qore)
install(FILES qore.1
        DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1)
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
        DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/qore)

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

# astparser module
add_subdirectory(modules/astparser)
# reflection module
add_subdirectory(modules/reflection)
# logger module
add_subdirectory(modules/logger_bin)
# linenoise module
add_subdirectory(modules/linenoise)
# mongodb module (optional, requires libmongoc)
# Try pkg-config first (more reliable, avoids broken cmake configs on some distros)
set(MONGOC_FOUND FALSE)
find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(MONGOC QUIET libmongoc-1.0)
endif()
if (NOT MONGOC_FOUND)
    # Fall back to cmake config (may fail on distros with broken static lib references)
    find_package(mongoc-1.0 QUIET)
    if (mongoc-1.0_FOUND)
        set(MONGOC_FOUND TRUE)
    endif()
endif()
if (MONGOC_FOUND)
    add_subdirectory(modules/mongodb)
else()
    message(STATUS "libmongoc not found - mongodb module will not be built")
endif()

# HACK: we need to simulate QORE_USER_MODULES_DIR as the versioned one
#       for qore core qlib modules.
# NOTE: anything below will be installed into versioned dirs!
set(QORE_USER_MODULES_DIR ${USER_MODULE_VER_DIR})
# modules must go after docs target - it's depending on it

# qlib modules + their documentation
# ---
# modules here are divided to groups in the order of dependence
# and then sorted alphabetically inside the groups

# base modules
qore_user_module("qlib/DatasourceProvider.qm" "")
qore_user_module("qlib/DebugUtil.qm" "")
qore_user_module("qlib/FsUtil.qm" "")
qore_user_module("qlib/HttpStreamClient.qm" "")
qore_user_module("qlib/Qdx.qm" "")
qore_user_module("qlib/Qorize.qm" "")
qore_user_module("qlib/QoreHistory" "modules/linenoise/linenoise")
qore_user_module("qlib/QoreRepl" "QoreHistory;modules/linenoise/linenoise")
qore_user_module("qlib/Util.qm" "")
qore_user_module("qlib/WebSocketUtil.qm" "")
qore_user_module("qlib/MapperUtil.qm" "")
qore_user_module("qlib/ProviderIndexUtil" "")
qore_user_module("qlib/QoreApiMetadata" "modules/astparser/astparser")
qore_user_module("qlib/QoreCodeCompletion" "")
qore_user_module("qlib/QoreCodeFormat" "modules/astparser/astparser")
qore_user_module("qlib/QLS" "modules/astparser/astparser;QoreCodeCompletion;QoreCodeFormat;QoreApiMetadata;Util;Mime")

# base 2 modules
qore_user_module("qlib/FileLocationHandler" "Util")
qore_user_module("qlib/Diff.qm" "Util")
qore_user_module("qlib/Logger.qm" "Util;modules/logger_bin/logger_bin")
qore_user_module("qlib/Mime.qm" "Util")
qore_user_module("qlib/QUnit.qm" "Util")
qore_user_module("qlib/TextWrap.qm" "Util")
qore_user_module("qlib/EdifactUtil" "Util;DataProvider;FileLocationHandler")

# base 3 modules
qore_user_module("qlib/DataProvider" "MapperUtil;Mime;Util" "DataProvider-Full.svg;qore-q-logo.svg")
qore_user_module("qlib/DpqlWebSocket" "DataProvider;HttpServerUtil;WebSocketHandler")
qore_user_module("qlib/DebugProgramControl.qm" "DebugUtil")
qore_user_module("qlib/HttpServerUtil.qm" "Mime;Util;FileLocationHandler;Logger;modules/logger_bin/logger_bin")
qore_user_module("qlib/MailMessage.qm" "Mime")

# base 4 modules
qore_user_module("qlib/AsyncSocketIo" "HttpServerUtil;Logger")
qore_user_module("qlib/FilePoller.qm" "Util;DataProvider")
qore_user_module("qlib/FileDataProvider" "FsUtil;DataProvider" "file-logo-white.svg;file-logo-black.svg")
qore_user_module("qlib/FtpClientDataProvider" "DataProvider" "ftp-logo-white.svg;ftp-logo-black.svg")
qore_user_module("qlib/HttpClientDataProvider" "DataProvider;FileLocationHandler")
qore_user_module("qlib/ServerSentEventHandler" "HttpServerUtil;Logger")
qore_user_module("qlib/SqlUtil" "Util;DataProvider" "SqlUtil-Full.svg")
qore_user_module("qlib/ConnectionProvider" "Util;DataProvider")
qore_user_module("qlib/CsvUtil" "Util;DataProvider")
qore_user_module("qlib/FixedLengthUtil" "Util;DataProvider")
qore_user_module("qlib/Mapper.qm" "MapperUtil;DataProvider")
qore_user_module("qlib/FtpPollerUtil.qm" "DataProvider")
qore_user_module("qlib/BulkSqlUtil" "SqlUtil")
qore_user_module("qlib/Schema.qm" "SqlUtil;Util")
qore_user_module("qlib/TelnetClient.qm" "ConnectionProvider")
qore_user_module("qlib/DebugCmdLine.qm" "ConnectionProvider;DebugUtil;DebugProgramControl")
qore_user_module("qlib/DiscordWebSocketClient.qm" "ConnectionProvider;Util;WebSocketUtil;WebSocketClient")
qore_user_module("qlib/HttpServer.qm" "HttpServerUtil;Mime;Util")
qore_user_module("qlib/HttpServerAsyncIo" "HttpServer")
qore_user_module("qlib/Http2ClientIo" "Logger")
qore_user_module("qlib/Pop3Client.qm" "ConnectionProvider;MailMessage;Mime")
qore_user_module("qlib/RestSchemaValidator.qm" "HttpServerUtil;Logger")
qore_user_module("qlib/SewioWebSocketClient.qm" "ConnectionProvider;Util;WebSocketUtil;WebSocketClient")
qore_user_module("qlib/SmtpClient.qm" "ConnectionProvider;MailMessage;Mime")
qore_user_module("qlib/WebSocketHandler.qm" "HttpServerUtil;WebSocketUtil")
qore_user_module("qlib/WebUtil.qm" "HttpServerUtil;Mime;Util")

# base 5 modules
qore_user_module("qlib/ProviderIndex" "DataProvider;ConnectionProvider;ProviderIndexUtil")
qore_user_module("qlib/ServerSentEventClient" "DataProvider;ConnectionProvider;Logger;Mime;Util")
qore_user_module("qlib/MssqlSqlUtilBase.qm" "SqlUtil;DataProvider")
qore_user_module("qlib/MysqlSqlUtil.qm" "SqlUtil;DataProvider")
qore_user_module("qlib/OracleSqlUtilBase.qm" "SqlUtil;Util;DataProvider")
qore_user_module("qlib/PgsqlSqlUtilBase.qm" "SqlUtil;DataProvider")
qore_user_module("qlib/Sqlite3SqlUtil.qm" "SqlUtil;DataProvider")
qore_user_module("qlib/XdbcFirebirdSqlUtilBase.qm" "SqlUtil;DataProvider")
qore_user_module("qlib/WebSocketClient.qm" "ConnectionProvider;Util;WebSocketUtil;DataProvider")
qore_user_module("qlib/TableMapper.qm" "MapperUtil;Mapper;SqlUtil;DataProvider")
qore_user_module("qlib/DbDataProvider" "DataProvider;SqlUtil;BulkSqlUtil")
qore_user_module("qlib/FtpPoller.qm" "FtpPollerUtil;Util;DataProvider")
qore_user_module("qlib/SchemaReverse.qm" "Qorize;Schema;SqlUtil;Util")
qore_user_module("qlib/DebugHandler.qm" "DebugProgramControl;DebugUtil;HttpServer;HttpServerUtil;WebSocketHandler;WebSocketUtil")
qore_user_module("qlib/DebugLinenoiseCmdLine.qm" "DebugUtil;DebugCmdLine")
qore_user_module("qlib/AsyncApi.qm" "Mime;Util;HttpServerUtil;RestSchemaValidator;Logger")
qore_user_module("qlib/OpenApi3.qm" "Mime;Util;HttpServerUtil;RestSchemaValidator;Logger")
qore_user_module("qlib/RestHandler.qm" "HttpServerUtil;HttpServer;Mime;Util;RestSchemaValidator;Logger")
qore_user_module("qlib/Swagger.qm" "Mime;Util;HttpServerUtil;RestSchemaValidator;Logger")
qore_user_module("qlib/VscDebugAdapter.qm" "DebugCmdLine;DebugUtil;Util")
qore_user_module("qlib/Pop3ClientDataProvider" "Pop3Client;ConnectionProvider;DataProvider" "pop3-logo-white.svg")

# base 6 modules
qore_user_module("qlib/FreetdsSqlUtil.qm" "SqlUtil;DataProvider;MssqlSqlUtilBase")
qore_user_module("qlib/JdbcMicrosoftSqlUtil.qm" "SqlUtil;DataProvider;MssqlSqlUtilBase")
qore_user_module("qlib/OracleSqlUtil.qm" "OracleSqlUtil;SqlUtil;Util;DataProvider")
qore_user_module("qlib/JdbcOracleSqlUtil.qm" "OracleSqlUtil;SqlUtil;Util;DataProvider")
qore_user_module("qlib/PgsqlSqlUtil.qm" "PgsqlSqlUtilBase;SqlUtil;DataProvider")
qore_user_module("qlib/JdbcPostgresqlSqlUtil.qm" "PgsqlSqlUtilBase;SqlUtil;DataProvider")
qore_user_module("qlib/JdbcFirebirdSqlUtil.qm" "XdbcFirebirdSqlUtilBase;SqlUtil;DataProvider")
qore_user_module("qlib/OdbcFirebirdSqlUtil.qm" "XdbcFirebirdSqlUtilBase;SqlUtil;DataProvider")
qore_user_module("qlib/RestClient.qm" "DataProvider;ConnectionProvider;Mime;RestSchemaValidator;Swagger;OpenApi3;FileLocationHandler")

# base 7 modules
qore_user_module("qlib/SalesforceRestClient.qm" "ConnectionProvider;Mime;RestClient;DataProvider")
qore_user_module("qlib/SewioRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ZeyosRestClient.qm" "ConnectionProvider;RestClient;Swagger")
qore_user_module("qlib/BillwerkRestClient.qm" "ConnectionProvider;RestClient")
qore_user_module("qlib/Sap4HanaRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/CdsRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ChargebeeRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/DiscordRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/FreshBooksRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/LinearRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/WaveRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/GoogleRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/HueRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/JotformRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/MailgunRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/MewsRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/NetSuiteRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/OpenAiRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ServiceNowRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ShipStationRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ShippoRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/SquareRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/Cin7CoreRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/UnleashedRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ZohoBooksRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ZohoInventoryRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/ZohoInvoiceRestClient.qm" "ConnectionProvider;Mime;RestClient")
qore_user_module("qlib/AwsRestClient.qm" "ConnectionProvider;RestClient;RestSchemaValidator")
qore_user_module("qlib/SwaggerDataProvider" "Swagger;RestClient;RestSchemaValidator;DataProvider")
qore_user_module("qlib/AsyncApiDataProvider" "AsyncApi;WebSocketClient;DataProvider")
qore_user_module("qlib/RestSchemaDataProvider" "Swagger;OpenApi3;RestClient;RestSchemaValidator;DataProvider")
qore_user_module("qlib/CdsRestDataProvider" "RestClient;CdsRestClient;RestSchemaValidator;DataProvider" "dynamics-logo.svg;business-central-logo.svg")
qore_user_module("qlib/ServiceNowRestDataProvider" "RestClient;ServiceNowRestClient;RestSchemaValidator;DataProvider" "servicenow-logo-white.svg;servicenow-logo-black.svg")
qore_user_module("qlib/RestClientDataProvider" "RestClient;DataProvider" "webhook-logo.svg")
qore_user_module("qlib/ElasticSearchDataProvider" "RestClient;DataProvider" "elastic-logo.svg")
qore_user_module("qlib/EmpathicBuildingDataProvider" "RestClient;DataProvider" "Haltian-EmpathicBuilding.svg")
qore_user_module("qlib/GeneratorDataProvider" "DataProvider" "generator-logo.svg")
qore_user_module("qlib/MemcachedDataProvider" "DataProvider" "memcached-logo.svg")
qore_user_module("qlib/MongoDbDataProvider" "DataProvider;ConnectionProvider" "mongodb-logo.svg")
qore_user_module("qlib/RedisDataProvider" "DataProvider;ConnectionProvider" "redis-logo.svg")

# base 8 modules
qore_user_module("qlib/ElasticSearchLoggerAppender.qm" "Logger;RestClient;ElasticSearchDataProvider")
qore_user_module("qlib/SalesforceRestDataProvider" "RestClient;SalesforceRestClient;DataProvider" "salesforce-logo.svg")
qore_user_module("qlib/AwsRestClientDataProvider" "RestClient;DataProvider;RestClientDataProvider")
qore_user_module("qlib/GoogleDataProvider" "GoogleRestClient;RestClient;DataProvider")
qore_user_module("qlib/DiscordDataProvider" "DiscordRestClient;RestClient;DataProvider" "discord-logo.svg")
qore_user_module("qlib/FreshBooksDataProvider" "FreshBooksRestClient;RestClient;DataProvider" "freshbooks-logo.svg")
qore_user_module("qlib/LinearDataProvider" "LinearRestClient;RestClient;DataProvider" "linear-logo.svg")
qore_user_module("qlib/WaveDataProvider" "WaveRestClient;RestClient;DataProvider" "wave-logo.svg")
qore_user_module("qlib/JotformDataProvider" "JotformRestClient;RestClient;DataProvider" "jotform-logo.svg")
qore_user_module("qlib/MailgunDataProvider" "MailgunRestClient;RestClient;DataProvider" "mailgun-logo.svg")
qore_user_module("qlib/MewsRestDataProvider" "MewsRestClient;RestClient;DataProvider;SwaggerDataProvider" "mews-logo-white.svg")
qore_user_module("qlib/OpenAiDataProvider" "OpenAiRestClient;ConnectionProvider;Mime;RestClient" "openai-logo.svg")

# base 9 modules
qore_user_module("qlib/GoogleCalendarDataProvider" "GoogleDataProvider;GoogleRestClient;RestClient;DataProvider" "google-calendar-logo.svg")
qore_user_module("qlib/GmailDataProvider" "GoogleDataProvider;GoogleRestClient;RestClient;DataProvider" "gmail-logo.svg")
qore_user_module("qlib/ZohoBooksDataProvider" "ZohoBooksRestClient;RestClient;DataProvider" "zohobooks-logo.svg")
qore_user_module("qlib/ZohoInventoryDataProvider" "ZohoInventoryRestClient;RestClient;DataProvider" "zohoinventory-logo.svg")
qore_user_module("qlib/ZohoInvoiceDataProvider" "ZohoInvoiceRestClient;RestClient;DataProvider" "zohoinvoice-logo.svg")
qore_user_module("qlib/ShipStationDataProvider" "ShipStationRestClient;RestClient;DataProvider" "shipstation-logo.svg")
qore_user_module("qlib/ShippoDataProvider" "ShippoRestClient;RestClient;DataProvider" "shippo-logo.svg")
qore_user_module("qlib/SquareDataProvider" "SquareRestClient;RestClient;DataProvider" "square-logo.svg")
qore_user_module("qlib/ChargebeeDataProvider" "ChargebeeRestClient;RestClient;DataProvider" "chargebee-logo.svg")
qore_user_module("qlib/Cin7CoreDataProvider" "Cin7CoreRestClient;RestClient;DataProvider" "cin7core-logo.svg")
qore_user_module("qlib/UnleashedDataProvider" "UnleashedRestClient;RestClient;DataProvider" "unleashed-logo.svg")

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
    add_dependencies(docs-lang-final docs-astparser docs-reflection docs-logger_bin docs-linenoise)
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

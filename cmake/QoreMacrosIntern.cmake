
#
# Written by Niclas Rosenvik <youremailsarecrap@gmail.com>
# Based on the tests found in qores configure.ac
#
#

include(CheckCSourceCompiles)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CMakeParseArguments)
include(CMakePushCheckState)

macro(qore_check_headers_cxx)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs)

    cmake_parse_arguments(_HEADERS "${options}" "${onevalueArgs}" "${multiValueArgs}" ${ARGN})

    foreach (include_file ${_HEADERS_UNPARSED_ARGUMENTS})
        string(REPLACE "." "_" include_file_ ${include_file})
	string(REPLACE "/" "_" _include_file_ ${include_file_})
	string(TOUPPER ${_include_file_} upper_include_file_)
	check_include_file_cxx(${include_file} "HAVE_${upper_include_file_}" ${QORE_CHECK_HEADERS_CXX_FLAGS})
#	string(CONCAT file_output ${file_output} "\n#cmakedefine " ${have_var})
    endforeach()
#    file(WRITE ${CMAKE_BINARY_DIR}/header_defines.txt ${file_output})
#    unset(file_output)
endmacro()

macro(qore_check_funcs)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs)

    cmake_parse_arguments(_FUNCS "${options}" "${onevalueArgs}" "${multiValueArgs}" ${ARGN})

    foreach (FUNCTION_TO_CHECK ${_FUNCS_UNPARSED_ARGUMENTS})
        string(TOUPPER ${FUNCTION_TO_CHECK} UPPER_FUNCTION_TO_CHECK)
        check_function_exists(${FUNCTION_TO_CHECK} "HAVE_${UPPER_FUNCTION_TO_CHECK}")
#	string(CONCAT file_output ${file_output} "\n#cmakedefine " ${HAVE_VAR})
    endforeach()
#    file(WRITE ${CMAKE_BINARY_DIR}/funcs_defines.txt ${file_output})
#    unset(file_output)
endmacro()

function(qore_search_libs _list_to_append_lib_to _function)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs)

    cmake_parse_arguments(_LIBS "${options}" "${onevalueArgs}" "${multiValueArgs}" ${ARGN})
    message(STATUS "Looking for library containing ${_function}")
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_QUIET true)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
    check_function_exists(${_function} qore_search_libs-${_function}_IMPLICIT)
    if(qore_search_libs-${_function}_IMPLICIT)
        message(STATUS "Found ${_function} in the implicitly linked libs")
    else()
        set(FUNCTIONNOTFOUND true)
        foreach(_lib ${_LIBS_UNPARSED_ARGUMENTS})
        set(CMAKE_REQUIRED_LIBRARIES ${_lib})
            check_function_exists(${_function} qore_search_libs-${_function}_FOUND_IN_${_lib})
            if(qore_search_libs-${_function}_FOUND_IN_${_lib})
                set(${_list_to_append_lib_to} ${${_list_to_append_lib_to}} ${_lib} PARENT_SCOPE)
                message(STATUS "Found ${_function} in ${_lib}")
                unset(${_lib})
                set(FUNCTIONNOTFOUND false)
                break()
            endif()
        endforeach()
        if(FUNCTIONNOTFOUND)
	    message(STATUS "${_function} could not be found in ${_LIBS_UNPARSED_ARGUMENTS} or the implicity linked libs")
	endif()
    endif()
    cmake_pop_check_state()
endfunction()

macro(qore_openssl_checks)

set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})

check_cxx_symbol_exists(EVP_mdc2 openssl/evp.h HAVE_OPENSSL_MDC2)
check_cxx_symbol_exists(EVP_sha512 openssl/evp.h HAVE_OPENSSL_SHA512)
check_cxx_symbol_exists(EVP_sha openssl/evp.h HAVE_OPENSSL_SHA)

# check if the EVP_mdc2() function links
check_cxx_source_compiles("
#include <openssl/evp.h>
int main(void) {
EVP_mdc2();
return 0;
}"
HAVE_OPENSSL_MDC2)

# check for openssl const correctness (later versions have it)
check_cxx_source_compiles("
#include <openssl/x509.h>
int main(void) {
const unsigned char *p;
d2i_X509(0, &p, 0l);
return 0;
}"
HAVE_OPENSSL_CONST)

# check for const required with SSL_CTX_new
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
const SSL_METHOD *meth;
SSL_CTX_new(meth);
return 0;
}"
NEED_SSL_CTX_NEW_CONST)

# check for return value with HMAC_Update and friends
check_cxx_source_compiles("
#include <openssl/hmac.h>
int main(void) {
int rc = HMAC_Update(0, 0, 0);
return 0;
}"
HAVE_OPENSSL_HMAC_RV)

# check for X509_get_signature_nid()
check_cxx_source_compiles("
#include <openssl/x509.h>
int main(void) {
int nid = X509_get_signature_nid(0);
return 0;
}"
HAVE_X509_GET_SIGNATURE_NID)

# check for const correctness in X509_get0_signature() (1.1+ has it)
check_cxx_source_compiles("
#include <openssl/x509.h>
int main(void) {
const ASN1_BIT_STRING* sig;
const X509_ALGOR* alg;
X509_get0_signature(&sig, &alg, 0);
return 0;
}"
HAVE_X509_GET0_SIGNATURE_CONST)

# check for X509_get0_pubkey() (1.1+ has it)
check_cxx_source_compiles("
#include <openssl/x509.h>
int main(void) {
X509_get0_pubkey(0);
return 0;
}"
HAVE_X509_GET0_PUBKEY)

# check for OPENSSL_init_crypto()
check_cxx_source_compiles("
#include <openssl/crypto.h>
int main(void) {
int nid = OPENSSL_init_crypto(0, 0);
return 0;
}"
HAVE_OPENSSL_INIT_CRYPTO)

# check for SSL_set_max_proto_version()
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
SSL_set_max_proto_version(nullptr, 0x0303);
return 0;
}"
HAVE_SSL_SET_MAX_PROTO_VERSION)

# check for SSL_set_hostflags()
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
SSL_set_hostflags(nullptr, 0x4);
return 0;
}"
HAVE_SSL_SET_HOSTFLAGS)

# check for SSL_set1_host()
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
SSL_set1_host(nullptr, nullptr);
return 0;
}"
HAVE_SSL_SET1_HOST)

# check for EVP_MAC_CTX_new()
check_cxx_source_compiles("
#include <openssl/evp.h>
int main(void) {
EVP_MAC_CTX_new(nullptr);
return 0;
}"
HAVE_EVP_MAC_CTX_NEW)

# check for SSL_read_ex()
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
size_t num;
SSL_read_ex(nullptr, nullptr, 0, &num);
return 0;
}"
HAVE_SSL_READ_EX)

# check for TLS_server_method()
check_cxx_source_compiles("
#include <openssl/ssl.h>
int main(void) {
TLS_server_method();
return 0;
}"
HAVE_TLS_SERVER_METHOD)

unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

endmacro()

macro(qore_mpfr_checks)

set(CMAKE_REQUIRED_INCLUDES ${MPFR_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${MPFR_LIBRARIES})

check_cxx_symbol_exists(mpfr_divby0_p mpfr.h HAVE_MPFR_DIVBY0)
check_cxx_symbol_exists(mpfr_regular_p mpfr.h HAVE_MPFR_REGULAR)
check_cxx_symbol_exists(mpfr_sprintf mpfr.h HAVE_MPFR_SPRINTF)
check_cxx_symbol_exists(mpfr_buildopt_tls_p mpfr.h HAVE_MPFR_BUILDOPT_TLS_P)

check_cxx_source_compiles("
#include <mpfr.h>
int main(void){
mpfr_exp_t test;
return 0;
}"
HAVE_MPFR_EXP_T)

check_cxx_source_compiles("
#include <mpfr.h>
int main(void){
mpfr_rnd_t test = MPFR_RNDN;
return 0;
}"
HAVE_RNDN)

check_cxx_source_compiles("
#include <mpfr.h>
int main(void){
mpfr_rnd_t test = MPFR_RNDN;
return 0;
}"
HAVE_MPFR_RNDN)

unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

endmacro()


macro(qore_gethost_checks)

check_cxx_symbol_exists(gethostbyaddr_r netdb.h HAVE_GETHOSTBYADDR_R)
check_cxx_symbol_exists(gethostbyname_r netdb.h HAVE_GETHOSTBYNAME_R)

if(HAVE_GETHOSTBYADDR_R)

# Check definition of gethostbyaddr_r (glibc2 defines this with 8 arguments)
check_c_source_compiles("
#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
int main(void) {
int skr;
struct hostent *foo = gethostbyaddr_r((const char *) 0,0, 0, (struct hostent *) 0, (char *) NULL,  0, &skr);
return (foo == 0);
}"
HAVE_SOLARIS_STYLE_GETHOST)

endif(HAVE_GETHOSTBYADDR_R)



if(HAVE_GETHOSTBYNAME_R)

# Check definition of gethostbyname_r (glibc2.0.100 is different from Solaris)
check_c_source_compiles("
#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
int main(void){
int skr;
skr = gethostbyname_r((const char *) 0,(struct hostent*) 0, (char*) 0, 0, (struct hostent **) 0, &skr);
return 0;
}"
HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE)

# Check 3rd argument of gethostbyname_r
check_c_source_compiles("
#undef inline
#if !defined(SCO) && !defined(__osf__) && !defined(_REENTRANT)
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
int main(void){
int skr;
skr = gethostbyname_r((const char *) 0, (struct hostent*) 0, (struct hostent_data*) 0);
return 0;
}"
HAVE_GETHOSTBYNAME_R_RETURN_INT)

endif(HAVE_GETHOSTBYNAME_R)

endmacro()


macro(qore_other_checks)

# check if the compiler supports variadic local arrays
check_cxx_source_compiles("
int main(void){
int x = 2; int y[x];
return 0;
}"
HAVE_LOCAL_VARIADIC_ARRAYS)

# see if struct flock is declared
check_c_source_compiles("
#include <fcntl.h>
int main(void){
struct flock fl;
return 0;
}"
HAVE_STRUCT_FLOCK)

check_cxx_compiler_flag(-fvisibility=hidden HAVE_GCC_VISIBILITY)

check_cxx_symbol_exists(strtoimax inttypes.h HAVE_STRTOIMAX)

#see if compiler supports namespaces
check_cxx_source_compiles("
namespace Outer {namespace Inner { int i = 0; }}
int main(void){
using namespace Outer::Inner;
return i;
}" HAVE_NAMESPACES)

endmacro()

macro(create_git_revision)
    # Always regenerate git-revision.h during cmake configure to ensure
    # the git hash is up-to-date after pulling new commits
    find_package(Git)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        OUTPUT_VARIABLE GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE)
        # Check if file exists and has same content to avoid unnecessary rebuilds
        set(_git_rev_file "${CMAKE_BINARY_DIR}/include/qore/intern/git-revision.h")
        set(_git_rev_content "#define BUILD \"${GIT_REV}\"")
        if(EXISTS ${_git_rev_file})
            file(READ ${_git_rev_file} _existing_content)
            string(STRIP "${_existing_content}" _existing_content)
            string(STRIP "${_git_rev_content}" _git_rev_content_stripped)
            if(NOT "${_existing_content}" STREQUAL "${_git_rev_content_stripped}")
                file(WRITE ${_git_rev_file} "${_git_rev_content}")
                message(STATUS "Updated git-revision.h: ${GIT_REV}")
            endif()
        else()
            file(WRITE ${_git_rev_file} "${_git_rev_content}")
            message(STATUS "Created git-revision.h: ${GIT_REV}")
        endif()
    else()
        message(FATAL_ERROR "Git is needed to generate git-revision.h")
    endif()
endmacro()


macro(qore_stl_hash_map _hash_map_output_file)

set(CMAKE_REQUIRED_QUIET true)
set(STL_HASH_MAP_FOUND false)
message(STATUS "Checking location of STL unordered_map")

#unordered map
check_cxx_source_compiles("
#include <unordered_map>
int main(void){
unordered_map<int, int> t;
const unordered_map<int, int> &tr = t;
tr.find(1);
return 0;
}" UNORDERED_MAP_FOUND)

if(UNORDERED_MAP_FOUND)
set(HAVE_UNORDERED_MAP true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<unordered_map>")
set(HASH_TYPE "unordered_map")
set(STL_HASH_MAP_FOUND true)
endif()

#std unordered map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <unordered_map>
int main(void){
std::unordered_map<int, int> t;
const std::unordered_map<int, int> &tr = t;
tr.find(1);
return 0;
}" STD_UNORDERED_MAP_FOUND)

if(STD_UNORDERED_MAP_FOUND)
set(HAVE_UNORDERED_MAP true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<unordered_map>")
set(HASH_NAMESPACE "std")
set(HASH_TYPE "std_unordered_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

#tr1 unordered map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <tr1/unordered_map>
int main(void){
std::tr1::unordered_map<int, int> t;
const std::tr1::unordered_map<int, int> &tr = t;
tr.find(1);
return 0;
}" TR1_UNORDERED_MAP_FOUND)

if(TR1_UNORDERED_MAP_FOUND)
set(HAVE_UNORDERED_MAP true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<tr1/unordered_map>")
set(HASH_NAMESPACE "std::tr1")
set(HASH_TYPE "tr1_unordered_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

#hash map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <hash_map>
int main(void){
hash_map<int, int> t;
return 0;
}" HASH_MAP_FOUND)

if(HASH_MAP_FOUND)
set(HAVE_HASH_MAP true)
set(HAVE_HASH_SET true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<hash_map>")
set(HASH_SET_INCLUDE "<hash_set>")
set(HASH_TYPE "hash_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

#ext hash map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <ext/hash_map>
int main(void){
__gnu_cxx::hash_map<int, int> t;
return 0;
}" EXT_HASH_MAP_FOUND)

if(EXT_HASH_MAP_FOUND)
set(HAVE_EXT_HASH_MAP true)
set(HAVE_EXT_HASH_SET true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<ext/hash_map>")
set(HASH_SET_INCLUDE "<ext/hash_set>")
set(HASH_NAMESPACE "__gnu_cxx")
set(HASH_TYPE "ext_hash_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

#std hash map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <hash_map>
int main(void){
std::hash_map<int, int> t;
return 0;
}" STD_HASH_MAP_FOUND)

if(STD_HASH_MAP_FOUND)
set(HAVE_HASH_MAP true)
set(HAVE_HASH_SET true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<hash_map>")
set(HASH_SET_INCLUDE "<hash_set>")
set(HASH_NAMESPACE "std")
set(HASH_TYPE "std_hash_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

#stdext hash map
if(NOT STL_HASH_MAP_FOUND)
check_cxx_source_compiles("
#include <hash_map>
int main(void){
std::hash_map<int, int> t;
return 0;
}" STDEXT_HASH_MAP_FOUND)

if(STDEXT_HASH_MAP_FOUND)
set(HAVE_HASH_MAP true)
set(HAVE_HASH_SET true)
set(HAVE_QORE_HASH_MAP true)
set(HASH_MAP_INCLUDE "<hash_map>")
set(HASH_SET_INCLUDE "<hash_set>")
set(HASH_NAMESPACE "stdext")
set(HASH_TYPE "stdext_hash_map")
set(STL_HASH_MAP_FOUND true)
endif()
endif()

if(STL_HASH_MAP_FOUND)
message(STATUS "STL unordered_map found: ${HASH_TYPE}")
else()
message(WARNING "Couldn't find an STL unordered_map")
endif()

#write the file.
configure_file(${CMAKE_SOURCE_DIR}/cmake/stl_hash_map.in
               ${CMAKE_BINARY_DIR}/${_hash_map_output_file})

unset(CMAKE_REQUIRED_QUIET)
endmacro()



macro(qore_stl_slist _stl_slist_output_file)

set(CMAKE_REQUIRED_QUIET true)
set(STL_SLIST_FOUND false)
message(STATUS "Checking location of STL slist")

#slist
check_cxx_source_compiles("
#include <slist>
int main(void){
slist<int> t;
return 0;
}" SLIST_FOUND)

if(SLIST_FOUND)
set(HAVE_SLIST true)
set(HAVE_QORE_SLIST true)
set(SLIST_INCLUDE "<slist>")
set(LIST_SET_INCLUDE "list_set")
set(SLIST_TYPE "slist")
set(STL_SLIST_FOUND true)
endif()

#ext slist
if(NOT STL_SLIST_FOUND)
check_cxx_source_compiles("
#include <ext/slist>
int main(void){
__gnu_cxx::slist<int> t;
return 0;
}" EXT_SLIST_FOUND)

if(EXT_SLIST_FOUND)
set(HAVE_SLIST true)
set(HAVE_QORE_SLIST true)
set(SLIST_INCLUDE "<ext/slist>")
set(LIST_SET_INCLUDE "<ext/list_set>")
set(LIST_NAMESPACE "__gnu_cxx")
set(SLIST_TYPE "ext_slist")
set(STL_SLIST_FOUND true)
endif()
endif()


#std slist
if(NOT STL_SLIST_FOUND)
check_cxx_source_compiles("
#include <slist>
int main(void){
std::slist<int> t;
return 0;
}" STD_SLIST_FOUND)

if(STD_SLIST_FOUND)
set(HAVE_SLIST true)
set(HAVE_QORE_SLIST true)
set(SLIST_INCLUDE "<slist>")
set(LIST_SET_INCLUDE "<list_set>")
set(LIST_NAMESPACE "std")
set(SLIST_TYPE "std_slist")
set(STL_SLIST_FOUND true)
endif()
endif()

#stdext slist
if(NOT STL_SLIST_FOUND)
check_cxx_source_compiles("
#include <slist>
int main(void){
stdext::slist<int> t;
return 0;
}" STDEXT_SLIST_FOUND)

if(STDEXT_SLIST_FOUND)
set(HAVE_SLIST true)
set(HAVE_QORE_SLIST true)
set(SLIST_INCLUDE "<slist>")
set(LIST_SET_INCLUDE "<list_set>")
set(LIST_NAMESPACE "stdext")
set(SLIST_TYPE "stdext_slist")
set(STL_SLIST_FOUND true)
endif()
endif()

if(STL_SLIST_FOUND)
message(STATUS "STL slist found: ${SLIST_TYPE}")
else()
message(WARNING "couldn't find an STL slist")
endif()

configure_file(${CMAKE_SOURCE_DIR}/cmake/stl_slist.in
               ${CMAKE_BINARY_DIR}/${_stl_slist_output_file})

unset(CMAKE_REQUIRED_QUIET)

endmacro()

macro(qore_func_strerror_r)
set(CMAKE_REQUIRED_QUIET_STRERROR_R_SAVE ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET true)
message(STATUS "Looking for strerror_r")
check_cxx_symbol_exists(strerror_r string.h HAVE_DECL_STRERROR_R)
if(HAVE_DECL_STRERROR_R)
   check_function_exists(strerror_r HAVE_STRERROR_R)
   if(HAVE_STRERROR_R)
      message(STATUS "Looking for strerror_r - found")
      message(STATUS "Checking the return type of strerror_r")
      check_cxx_source_compiles("
      #include <string.h>
      int main(void) {
      char strbuf[100];
      char *retbuf = NULL;
      retbuf = strerror_r(0, strbuf, sizeof(strbuf));
      return 0;
      }" STRERROR_R_CHAR_P)
      if(STRERROR_R_CHAR_P)
         message(STATUS "Checking the return type of strerror_r - char*")
      else()
         message(STATUS "Checking the return type of strerror_r - int")
      endif()
   else()
      message(STATUS "Looking for strerror_r - not found")
   endif()
else()
   message(STATUS "Looking for strerror_r - not found")
endif()
set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_STRERROR_R_SAVE})
endmacro()

macro(qore_cpu_checks)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
   set(CPU_X86_64 true)
endif()
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "sparc")
   set(SPARC true)
endif()
endmacro(qore_cpu_checks)

macro(qore_iconv_translit_check)
if(ICONV_TRANSLIT)
   set(NEED_ICONV_TRANSLIT true)
else()
   cmake_push_check_state(RESET)
   set(CMAKE_REQUIRED_INCLUDES ${ICONV_INCLUDE_DIR})
   set(CMAKE_REQUIRED_LIBRARIES ${ICONV_LIBRARY})
   check_cxx_source_runs("
      #include <iconv.h>
      int main(void){
      iconv_t cd;
      cd = iconv_open(\"ISO8859-1//TRANSLIT\",\"ISO8859-1\");
      if(cd == (iconv_t)-1)
        return 1;
      iconv_close(cd);
      return 0; }
   " NEED_ICONV_TRANSLIT)
endif()
cmake_pop_check_state()
endmacro(qore_iconv_translit_check)

macro(qore_os_checks)
if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
   #set(QORE_CPPFLAGS -D_POSIX_C_SOURCE=199506L -D_XPG4_2 -D_XPG5 -D__EXTENSIONS__)
   set(ZONEINFO_LOCATION "/usr/share/lib/zoneinfo")
   set(SOLARIS true)
   if (CMAKE_COMPILER_IS_GNUCXX)
      # netinet/tcp.h needs sys/types.h on illumos but it does not include it
      set(QORE_CHECK_HEADERS_CXX_FLAGS "-include sys/types.h")
   endif (CMAKE_COMPILER_IS_GNUCXX)
   #use libumem if available
   find_library(LIBUMEM NAMES umem libumem)
   if(LIBUMEM)
      check_include_file_cxx(umem.h HAVE_UMEM_H)
      if(HAVE_UMEM_H)
         set(LIBQORE_LIBS ${LIBUMEM} ${LIBQORE_LIBS})
      endif()
   endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
   set(QORE_CPPFLAGS -D_APPLE_C_SOURCE -D_DARWIN_C_SOURCE)
   set(DARWIN true)
   #look for libtbbmalloc
   find_library(LIBTBBMALLOC NAMES tbbmalloc libtbbmalloc)
   if(LIBTBBMALLOC)
      set(LIBQORE_LIBS ${LIBTBBMALLOC} ${LIBQORE_LIBS})
   endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
   set(QORE_CPPFLAGS -D_XOPEN_SOURCE_EXTENDED)
   set(NEED_ENVIRON_LOCK true)
   set(HPUX true)
   if(CMAKE_CXX_COMPILER_ID STREQUAL "HP")
      list(APPEND QORE_CPPFLAGS -D_HPUX_SOURCE -D__STDC_EXT__ -D_RWSTD_MULTI_THREAD)
   endif()
endif()

add_definitions(${QORE_CPPFLAGS})
set(CMAKE_REQUIRED_DEFINITIONS ${QORE_CPPFLAGS})
endmacro()

# make sure dlfcn.h header file can be parsed without 'extern "C" {}'
function(qore_dlcpp_check)
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_DL_LIBS})
check_cxx_source_compiles("
   #include <dlfcn.h>
   int main(void){
   dlopen(\"tmp.dll\", RTLD_LAZY|RTLD_GLOBAL);
   return 0;
   }" DONT_NEED_DLFCN_WRAPPER)
if(NOT DEFINED DONT_NEED_DLFCN_WRAPPER)
   set(NEED_DLFCN_WRAPPER true PARENT_SCOPE)
endif()
endfunction()

macro(qore_assembler_files)
if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
   if(CMAKE_VOID_P EQUAL 8) #64-bit
      if(DEFINED SPARC)
        # set(LIBQORE_ASM_SRC lib/sunpro-sparc64.s)
      else()
         set(LIBQORE_ASM_SRC lib/sunpro-x86_64.s)
      endif()
   else() #32-bit
      if(DEFINED SPARC)
         set(LIBQORE_ASM_SRC lib/sunpro-sparc32.s)
      else()
         set(LIBQORE_ASM_SRC lib/sunpro-i386.s)
      endif()
   endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
   if(CMAKE_SYSTEM_PROCESSOR MATCHES "hppa")
      if(CMAKE_VOID_P EQUAL 8)
      else()
         set(LIBQORE_ASM_SRC lib/acc-parisc32.s)
      endif()
   elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ia64")
      if(CMAKE_VOID_P EQUAL 8)
         set(LIBQORE_ASM_SRC lib/acc-64bit-ia64.s)
      endif()
   endif()
endif()

if(DEFINED LIBQORE_ASM_SRC)
   enable_language(ASM)
endif()
endmacro()

# Check for explicit link against libatomic
# Helper for checking for atomics
function(check_working_cxx_atomics varname additional_lib)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state()
  #set(CMAKE_REQUIRED_FLAGS "-std=c++11")
  set(CMAKE_REQUIRED_LIBRARIES "${additional_lib}")
  set(CMAKE_REQUIRED_QUIET 1)
  CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
std::atomic<int> x;
int main() {
  bool b = std::atomic_int{}.is_lock_free();
  return std::atomic_fetch_add_explicit(&x, 1, std::memory_order_seq_cst);
}
" ${varname})
  cmake_pop_check_state()
endfunction(check_working_cxx_atomics)

# First check if atomics work without the library.
# If not, check if the library exists, and atomics work with it.
function(check_cxx_atomic)
  check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITHOUT_LIB "")
  if(HAVE_CXX_ATOMICS_WITHOUT_LIB)
    message(STATUS "C++ atomics provided at compile time")
  else()
    message(STATUS "C++ atomics require a library")
    find_library(LIBATOMIC_LIBRARY NAMES atomic PATH_SUFFIXES lib)
    if(LIBATOMIC_LIBRARY)
      check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITH_LIB "${LIBATOMIC_LIBRARY}")
      if (HAVE_CXX_ATOMICS_WITH_LIB)
        set(LIBQORE_LIBS "${LIBQORE_LIBS};${LIBATOMIC_LIBRARY}" PARENT_SCOPE)
        message(STATUS "Atomics provided by libatomic")
      else()
        message(STATUS "no libatomic library found")
        message(FATAL_ERROR "Compiler must support std::atomic!")
      endif()
    else()
      # find_library() fails for libatomic on fedora, no clue why - so here we hardcode it anyway
      #message(FATAL_ERROR "Compiler appears to require libatomic, but cannot find it.")
      message(STATUS "no libatomic library found; trying to link with it anyway")
      set(LIBQORE_LIBS "${LIBQORE_LIBS};atomic" PARENT_SCOPE)
    endif()
  endif()
endfunction(check_cxx_atomic)

function(get_module_api_versions)
    file(READ ${CMAKE_SOURCE_DIR}/include/qore/ModuleManager.h MM_CONTENTS)

    string(REGEX MATCH "#define QORE_MODULE_API_MAJOR [0-9]+" QMAMA_L "${MM_CONTENTS}")
    string(REGEX REPLACE "#define QORE_MODULE_API_MAJOR ([0-9]+)" "\\1" QMAMA_L "${QMAMA_L}")
    string(REGEX MATCH "#define QORE_MODULE_API_MINOR [0-9]+" QMAMI_L "${MM_CONTENTS}")
    string(REGEX REPLACE "#define QORE_MODULE_API_MINOR ([0-9]+)" "\\1" QMAMI_L "${QMAMI_L}")
    string(REGEX MATCH "#define QORE_MODULE_COMPAT_API_MAJOR [0-9]+" QMCAMA_L "${MM_CONTENTS}")
    string(REGEX REPLACE "#define QORE_MODULE_COMPAT_API_MAJOR ([0-9]+)" "\\1" QMCAMA_L "${QMCAMA_L}")
    string(REGEX MATCH "#define QORE_MODULE_COMPAT_API_MINOR [0-9]+" QMCAMI_L "${MM_CONTENTS}")
    string(REGEX REPLACE "#define QORE_MODULE_COMPAT_API_MINOR ([0-9]+)" "\\1" QMCAMI_L "${QMCAMI_L}")

    set(MODULE_API_MAJOR ${QMAMA_L} PARENT_SCOPE)
    set(MODULE_API_MINOR ${QMAMI_L} PARENT_SCOPE)
    set(MODULE_COMPAT_API_MAJOR ${QMCAMA_L} PARENT_SCOPE)
    set(MODULE_COMPAT_API_MINOR ${QMCAMI_L} PARENT_SCOPE)
endfunction(get_module_api_versions)

# issue #3812: add library architecture directory to all library dirs; GNUInstallDirs
# will only set this for /usr, but it needs to be set
# for all library dirs
# cmake change removing this support:
#   https://gitlab.kitware.com/cmake/cmake/commit/620939e4e6f5a61cd5c0fac2704de4bfda0eb7ef
# debian refs showing that in fact the arch-specific library dir should be used:
#   https://wiki.debian.org/Multiarch/TheCaseForMultiarch
#   https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=685519
macro(check_libdir_arch)
    if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$"
       AND NOT CMAKE_CROSSCOMPILING
       AND EXISTS "/etc/debian_version"
       AND CMAKE_LIBRARY_ARCHITECTURE
       AND NOT (CMAKE_INSTALL_LIBDIR MATCHES "${CMAKE_LIBRARY_ARCHITECTURE}"))
        set (CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${CMAKE_LIBRARY_ARCHITECTURE}")
        set (CMAKE_INSTALL_FULL_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_LIBRARY_ARCHITECTURE}")
        message(STATUS "library dir: " ${CMAKE_INSTALL_LIBDIR})
        message(STATUS "full library dir: " ${CMAKE_INSTALL_FULL_LIBDIR})
    endif()
endmacro(check_libdir_arch)

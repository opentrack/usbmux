# opentrack-usbmux: libusbmuxd, libimobiledevice-glue and libplist, built from
# unmodified upstream sources (git submodules) into a single library.
#
# The upstream projects only ship autotools builds, which on Windows means
# MinGW. This replaces them with a CMake build that also works with MSVC, so
# opentrack can build the library itself on every platform.
#
# All three libraries are LGPL-2.1-or-later, so by default they are linked into
# one shared library (opentrack-usbmux.dll, libopentrack-usbmux.so,
# libopentrack-usbmux.dylib). Only the usbmuxd_* API is exported from it.
#
# Usage from another CMake project:
#
#   add_subdirectory(opentrack-usbmux)
#   target_link_libraries(my-target PRIVATE opentrack-usbmux)
#   #include <usbmuxd.h>

cmake_minimum_required(VERSION 3.22)

project(opentrack-usbmux VERSION 1.0.0 LANGUAGES C)

# Keep these in sync with the submodule checkouts. Upstream derives them from
# git tags, which a submodule checkout does not reliably have.
set(LIBPLIST_VERSION "2.8.0")
set(LIBIMOBILEDEVICE_GLUE_VERSION "1.3.3")
set(LIBUSBMUXD_VERSION "2.1.1")

option(OPENTRACK_USBMUX_SHARED "Build a shared library. If OFF, consumers link the object files directly." ON)
option(OPENTRACK_USBMUX_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(OPENTRACK_USBMUX_TESTS "Build the link test" ${PROJECT_IS_TOP_LEVEL})

foreach(_sub IN ITEMS libplist libimobiledevice-glue libusbmuxd)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_sub}/src")
        message(FATAL_ERROR "${_sub} submodule is missing; run: git submodule update --init")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Platform checks, the equivalent of the upstream configure.ac checks.
# ---------------------------------------------------------------------------

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckStructHasMember)

# A parent project may set this to STATIC_LIBRARY (opentrack does), which
# turns check_function_exists() into a compile-only test that always passes.
# The checks below need a real link, as autoconf's AC_CHECK_FUNCS does.
set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

if(NOT WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
endif()

# Cache variables are prefixed so they cannot collide with a parent project's
# checks of the same name.
macro(usbmux_check_function func var)
    check_function_exists(${func} OTR_USBMUX_${var})
    set(${var} ${OTR_USBMUX_${var}})
endmacro()

usbmux_check_function(getifaddrs HAVE_GETIFADDRS)
usbmux_check_function(gmtime_r HAVE_GMTIME_R)
usbmux_check_function(localtime_r HAVE_LOCALTIME_R)
usbmux_check_function(memmem HAVE_MEMMEM)
usbmux_check_function(poll HAVE_POLL)
usbmux_check_function(stpcpy HAVE_STPCPY)
usbmux_check_function(stpncpy HAVE_STPNCPY)
usbmux_check_function(strndup HAVE_STRNDUP)
usbmux_check_function(strptime HAVE_STRPTIME)
usbmux_check_function(timegm HAVE_TIMEGM)

# Windows uses native threads. MinGW's winpthreads would pass these checks, so
# skip them there, as upstream's configure does.
if(NOT WIN32)
    usbmux_check_function(pselect HAVE_PSELECT)
    usbmux_check_function(pthread_cancel HAVE_PTHREAD_CANCEL)
endif()

check_include_file(net/if_dl.h OTR_USBMUX_HAVE_NET_IF_DL_H)
set(HAVE_NET_IF_DL_H ${OTR_USBMUX_HAVE_NET_IF_DL_H})
check_include_file(netpacket/packet.h OTR_USBMUX_HAVE_NETPACKET_PACKET_H)
set(HAVE_NETPACKET_PACKET_H ${OTR_USBMUX_HAVE_NETPACKET_PACKET_H})

check_struct_has_member("struct tm" tm_gmtoff time.h OTR_USBMUX_HAVE_TM_TM_GMTOFF)
set(HAVE_TM_TM_GMTOFF ${OTR_USBMUX_HAVE_TM_TM_GMTOFF})
check_struct_has_member("struct tm" tm_zone time.h OTR_USBMUX_HAVE_TM_TM_ZONE)
set(HAVE_TM_TM_ZONE ${OTR_USBMUX_HAVE_TM_TM_ZONE})

# libusbmuxd watches /var/run for usbmuxd restarts via inotify on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    check_include_file(sys/inotify.h OTR_USBMUX_HAVE_INOTIFY)
    set(HAVE_INOTIFY ${OTR_USBMUX_HAVE_INOTIFY})

    check_c_source_compiles("
        extern char* program_invocation_short_name;
        int main(void) { return program_invocation_short_name[0]; }"
        OTR_USBMUX_HAVE_PROGRAM_INVOCATION_SHORT_NAME)
    set(HAVE_PROGRAM_INVOCATION_SHORT_NAME ${OTR_USBMUX_HAVE_PROGRAM_INVOCATION_SHORT_NAME})
    if(HAVE_PROGRAM_INVOCATION_SHORT_NAME)
        check_c_source_compiles("
            #include <errno.h>
            int main(void) { return program_invocation_short_name[0]; }"
            OTR_USBMUX_HAVE_PROGRAM_INVOCATION_SHORT_NAME_ERRNO_H)
        set(HAVE_PROGRAM_INVOCATION_SHORT_NAME_ERRNO_H ${OTR_USBMUX_HAVE_PROGRAM_INVOCATION_SHORT_NAME_ERRNO_H})
    endif()
endif()

unset(CMAKE_REQUIRED_LIBRARIES)

# libplist uses pow() and fmin(), which live in libm except on Windows.
check_library_exists(m fmin "" OTR_USBMUX_HAVE_LIBM)

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(__BIG_ENDIAN__ 1)
else()
    set(__LITTLE_ENDIAN__ 1)
endif()

# One config.h per library: they share the platform checks but each reports
# its own PACKAGE_VERSION.
function(usbmux_configure_header package version)
    set(usbmux_package "${package}")
    set(usbmux_version "${version}")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${package}/config.h" @ONLY)
endfunction()

usbmux_configure_header(libplist "${LIBPLIST_VERSION}")
usbmux_configure_header(libimobiledevice-glue "${LIBIMOBILEDEVICE_GLUE_VERSION}")
usbmux_configure_header(libusbmuxd "${LIBUSBMUXD_VERSION}")

# ---------------------------------------------------------------------------
# The three libraries, as object libraries.
# ---------------------------------------------------------------------------

function(usbmux_object_library name config_dir)
    add_library(${name} OBJECT ${ARGN})
    set_target_properties(${name} PROPERTIES
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${config_dir}")
    # LIBPLIST_STATIC and LIMD_GLUE_STATIC keep libplist and glue out of the
    # exported API, and stop their headers from marking symbols defined in
    # this same library as dllimport.
    target_compile_definitions(${name} PRIVATE HAVE_CONFIG_H LIBPLIST_STATIC LIMD_GLUE_STATIC)
    # Upstream code: don't let a parent project's warning flags flood its build
    # log with warnings that are not ours to fix.
    target_compile_options(${name} PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>)
    if(WIN32)
        target_compile_definitions(${name} PRIVATE WIN32)
    endif()
    if(MSVC)
        target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    endif()
    if(NOT WIN32)
        target_link_libraries(${name} PRIVATE Threads::Threads)
    endif()
endfunction()

set(_plist "${CMAKE_CURRENT_SOURCE_DIR}/libplist")
usbmux_object_library(usbmux-libplist libplist
    "${_plist}/libcnary/node.c"
    "${_plist}/libcnary/node_list.c"
    "${_plist}/src/base64.c"
    "${_plist}/src/bytearray.c"
    "${_plist}/src/hashtable.c"
    "${_plist}/src/ptrarray.c"
    "${_plist}/src/time64.c"
    "${_plist}/src/common.c"
    "${_plist}/src/xplist.c"
    "${_plist}/src/bplist.c"
    "${_plist}/src/jsmn.c"
    "${_plist}/src/jplist.c"
    "${_plist}/src/oplist.c"
    "${_plist}/src/out-default.c"
    "${_plist}/src/out-plutil.c"
    "${_plist}/src/out-limd.c"
    "${_plist}/src/plist.c")
target_include_directories(usbmux-libplist PRIVATE
    "${_plist}/include" "${_plist}" "${_plist}/src" "${_plist}/libcnary/include")

set(_glue "${CMAKE_CURRENT_SOURCE_DIR}/libimobiledevice-glue")
usbmux_object_library(usbmux-glue libimobiledevice-glue
    "${_glue}/src/glue.c"
    "${_glue}/src/socket.c"
    "${_glue}/src/thread.c"
    "${_glue}/src/utils.c"
    "${_glue}/src/nskeyedarchive.c"
    "${_glue}/src/collection.c"
    "${_glue}/src/termcolors.c"
    "${_glue}/src/cbuf.c"
    "${_glue}/src/opack.c"
    "${_glue}/src/tlv.c"
    "${_glue}/src/sha1.c"
    "${_glue}/src/sha256.c"
    "${_glue}/src/sha512.c")
target_include_directories(usbmux-glue PRIVATE "${_glue}/include" "${_glue}" "${_plist}/include")

set(_usbmuxd "${CMAKE_CURRENT_SOURCE_DIR}/libusbmuxd")
usbmux_object_library(usbmux-libusbmuxd libusbmuxd "${_usbmuxd}/src/libusbmuxd.c")
target_include_directories(usbmux-libusbmuxd PRIVATE "${_usbmuxd}/include" "${_glue}/include" "${_plist}/include")
if(NOT OPENTRACK_USBMUX_SHARED)
    target_compile_definitions(usbmux-libusbmuxd PRIVATE LIBUSBMUXD_STATIC)
endif()

set(_objects
    $<TARGET_OBJECTS:usbmux-libplist>
    $<TARGET_OBJECTS:usbmux-glue>
    $<TARGET_OBJECTS:usbmux-libusbmuxd>)

# ---------------------------------------------------------------------------
# The library consumers link against.
# ---------------------------------------------------------------------------

if(OPENTRACK_USBMUX_SHARED)
    # Unversioned: a private library shipped next to its one consumer.
    add_library(opentrack-usbmux SHARED ${_objects})
    if(WIN32)
        # opentrack-usbmux.dll with MinGW too, not libopentrack-usbmux.dll.
        set_target_properties(opentrack-usbmux PROPERTIES PREFIX "")
    endif()
    set(_scope PRIVATE)
else()
    # Not a STATIC library on purpose. Nothing references glue.c, but its
    # static initializer is what calls WSAStartup() on Windows; an archive
    # would let the linker drop it. Handing the object files straight to the
    # consumer's link keeps all of them.
    add_library(opentrack-usbmux INTERFACE)
    target_link_libraries(opentrack-usbmux INTERFACE ${_objects})
    target_compile_definitions(opentrack-usbmux INTERFACE LIBUSBMUXD_STATIC)
    set(_scope INTERFACE)
endif()

add_library(opentrack-usbmux::opentrack-usbmux ALIAS opentrack-usbmux)

target_include_directories(opentrack-usbmux INTERFACE
    $<BUILD_INTERFACE:${_usbmuxd}/include>
    $<INSTALL_INTERFACE:include>)

if(WIN32)
    target_link_libraries(opentrack-usbmux ${_scope} ws2_32 iphlpapi)
else()
    target_link_libraries(opentrack-usbmux ${_scope} Threads::Threads)
endif()
if(OTR_USBMUX_HAVE_LIBM)
    target_link_libraries(opentrack-usbmux ${_scope} m)
endif()

if(OPENTRACK_USBMUX_SHARED AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Fail at build time, not at dlopen() time, if a symbol is missing.
    target_link_options(opentrack-usbmux PRIVATE "LINKER:--no-undefined")
endif()

# ---------------------------------------------------------------------------

if(OPENTRACK_USBMUX_TESTS)
    enable_testing()
    add_executable(usbmux-link-test tests/link-test.c)
    target_link_libraries(usbmux-link-test PRIVATE opentrack-usbmux)
    if(WIN32)
        target_link_libraries(usbmux-link-test PRIVATE ws2_32)
    endif()
    add_test(NAME link-test COMMAND usbmux-link-test)
    if(WIN32 AND OPENTRACK_USBMUX_SHARED)
        set_tests_properties(link-test PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:opentrack-usbmux>")
    endif()
endif()

if(OPENTRACK_USBMUX_INSTALL AND OPENTRACK_USBMUX_SHARED)
    include(GNUInstallDirs)
    install(TARGETS opentrack-usbmux
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES "${_usbmuxd}/include/usbmuxd.h" "${_usbmuxd}/include/usbmuxd-proto.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/opentrack-usbmux")
endif()

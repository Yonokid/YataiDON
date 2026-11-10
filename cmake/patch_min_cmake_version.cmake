# Portable replacement for the `sed -i "s/cmake_minimum_required(VERSION X)/...3.5)/" || true`
# PATCH_COMMANDs. Those only work when a Unix toolchain is on PATH: FetchContent
# runs PATCH_COMMAND through cmd.exe on Windows, where `sed` and `true` do not
# exist, so the populate step fails the whole configure on a plain Windows host.
#
# Usage (working directory is the fetched source tree):
#   ${CMAKE_COMMAND} -DPATCH_FILE=CMakeLists.txt -DOLD_VERSION=3.30 -P patch_min_cmake_version.cmake
#
# string(REPLACE) is a no-op when the pattern is absent, so re-running on an
# already-patched tree is harmless -- same intent as the old `|| true`.
if(NOT DEFINED PATCH_FILE OR NOT DEFINED OLD_VERSION)
    message(FATAL_ERROR "patch_min_cmake_version.cmake needs -DPATCH_FILE= and -DOLD_VERSION=")
endif()
if(NOT DEFINED NEW_VERSION OR NEW_VERSION STREQUAL "")
    set(NEW_VERSION "3.5")
endif()
file(READ "${PATCH_FILE}" _content)
set(_original "${_content}")
string(CONCAT _pattern
       "[Cc][Mm][Aa][Kk][Ee]_[Mm][Ii][Nn][Ii][Mm][Uu][Mm]_[Rr][Ee][Qq][Uu][Ii][Rr][Ee][Dd]"
       "[ \t]*\\([ \t]*VERSION[ \t]+${OLD_VERSION}([ \t]*\\.\\.\\.[0-9.]+)?")
string(REGEX REPLACE "${_pattern}"
       "cmake_minimum_required(VERSION ${NEW_VERSION}"
       _content "${_content}")
if(_content STREQUAL _original)
    message(WARNING "patch_min_cmake_version: no 'cmake_minimum_required(VERSION ${OLD_VERSION})' "
        "found in '${PATCH_FILE}' - upstream may have changed")
endif()
file(WRITE "${PATCH_FILE}" "${_content}")

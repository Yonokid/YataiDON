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
file(READ "${PATCH_FILE}" _content)
string(REPLACE "cmake_minimum_required(VERSION ${OLD_VERSION})"
               "cmake_minimum_required(VERSION 3.5)"
               _content "${_content}")
file(WRITE "${PATCH_FILE}" "${_content}")

# Portable replacement for `git apply <patch> || true`: the `|| true` is a shell
# idiom, and on Windows FetchContent runs PATCH_COMMAND through cmd.exe, where
# `true` is not a command -- so the second populate (patch already applied,
# git apply exits non-zero) failed the whole configure.
#
# Usage: ${CMAKE_COMMAND} -DPATCH_FILE=<abs path to .patch> -P apply_git_patch.cmake
#
# If the patch reverse-applies cleanly it is already in the tree: skip. A real
# apply failure is reported as a warning, never a fatal error (matching the old
# `|| true` behaviour -- the sol2 android patch is irrelevant on other targets).
if(NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "apply_git_patch.cmake needs -DPATCH_FILE=")
endif()
execute_process(COMMAND git apply --reverse --check "${PATCH_FILE}"
                RESULT_VARIABLE _reversed OUTPUT_QUIET ERROR_QUIET)
if(_reversed EQUAL 0)
    return()   # already applied
endif()
execute_process(COMMAND git apply "${PATCH_FILE}" RESULT_VARIABLE _res ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(WARNING "git apply ${PATCH_FILE} failed (ignored): ${_err}")
endif()

# Idempotently applies the local SDL+Vulkan fix on top of the vendored raylib fork.
# FetchContent re-clones raylib whenever .cmake-deps/raylib-subbuild is missing (e.g.
# build_release.sh deletes it on every run), silently discarding any hand-edited files
# under raylib-src. This patch step re-applies the fix so it survives that re-populate.
# Invoked as: cmake -D RAYLIB_PATCH_FILE=<path> -P ApplyRaylibPatch.cmake
# (working directory is the raylib source checkout, set by ExternalProject's PATCH_COMMAND)

execute_process(
    COMMAND git apply --reverse --check "${RAYLIB_PATCH_FILE}"
    RESULT_VARIABLE ALREADY_APPLIED
    OUTPUT_QUIET
    ERROR_QUIET
)

if (NOT ALREADY_APPLIED EQUAL 0)
    message(STATUS "Applying ${RAYLIB_PATCH_FILE} to raylib checkout")
    execute_process(
        COMMAND git apply "${RAYLIB_PATCH_FILE}"
        RESULT_VARIABLE APPLY_RESULT
    )
    if (NOT APPLY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply ${RAYLIB_PATCH_FILE} to raylib checkout")
    endif()
else()
    message(STATUS "${RAYLIB_PATCH_FILE} already applied to raylib checkout")
endif()

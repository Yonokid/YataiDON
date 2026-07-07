# shaderc's third_party/CMakeLists.txt sets GLSLANG_ENABLE_INSTALL via an unevaluated
# generator expression ($<NOT:${SKIP_GLSLANG_INSTALL}>) -- plain set()/if() never evaluates
# generator-expression syntax, so the variable ends up as a literal, always-truthy string.
# glslang then always attempts install(EXPORT glslang-targets ...), which fails CMake's
# configure-time export-completeness check because SPIRV-Tools' own install is (correctly)
# skipped here, so SPIRV-Tools-opt is never part of any export set. Force a real boolean.
# Invoked as: cmake -P FixShadercGlslangInstall.cmake
# (working directory is the shaderc source checkout, set by ExternalProject's PATCH_COMMAND)

set(_file "third_party/CMakeLists.txt")
file(READ "${_file}" _contents)
set(_buggy "set(GLSLANG_ENABLE_INSTALL $<NOT:\${SKIP_GLSLANG_INSTALL}>)")
set(_fixed "set(GLSLANG_ENABLE_INSTALL OFF)")

string(FIND "${_contents}" "${_buggy}" _buggy_pos)
if(_buggy_pos GREATER -1)
    string(REPLACE "${_buggy}" "${_fixed}" _contents "${_contents}")
    file(WRITE "${_file}" "${_contents}")
    message(STATUS "Patched ${_file}: forced GLSLANG_ENABLE_INSTALL OFF")
else()
    string(FIND "${_contents}" "${_fixed}" _fixed_pos)
    if(_fixed_pos GREATER -1)
        message(STATUS "${_file} already patched (GLSLANG_ENABLE_INSTALL OFF)")
    else()
        message(FATAL_ERROR "FixShadercGlslangInstall: expected line not found in ${_file} -- shaderc's third_party/CMakeLists.txt may have changed upstream, update this patch")
    endif()
endif()

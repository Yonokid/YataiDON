file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/shaders/spirv")
file(GLOB shader_files CONFIGURE_DEPENDS "${game_root}/shader/bgfx/[fv]s_*.sc")
file(GLOB shader_includes CONFIGURE_DEPENDS "${game_root}/shader/bgfx/*.sh")
foreach(shader_file ${shader_files})
  get_filename_component(shader "${shader_file}" NAME_WE)
  if(shader MATCHES "^vs_")
    set(type vertex)
  else()
    set(type fragment)
  endif()
  set(output "${CMAKE_BINARY_DIR}/generated/shaders/spirv/${shader}.sc.bin.h")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${YATAIDON_SHADERC}" -f "${shader_file}" -o "${output}"
      --type ${type} --platform linux -p spirv --bin2c ${shader}_spv
      --varyingdef "${game_root}/shader/bgfx/varying.def.sc" -i "${vendor}/bgfx.cmake/bgfx/src"
    DEPENDS "${shader_file}" ${shader_includes} "${game_root}/shader/bgfx/varying.def.sc" VERBATIM)
  target_sources(YataiDON PRIVATE "${output}")
endforeach()
